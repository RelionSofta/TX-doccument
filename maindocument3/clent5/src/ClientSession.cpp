#include "ClientSession.h"
#include <iostream>
#include <filesystem>

#ifdef _WIN32
#  include <winsock2.h>
#else
#  include <arpa/inet.h>
#endif

// ============================================================
//  ClientSession.cpp — 512KB + no-wait pipeline
//
//  Pipeline flow:
//    send_file() → sendNextFileChunk() x PIPELINE_DEPTH
//                  seedha fileQueue_ mein push (no asio::post)
//    write() complete → sendNextFileChunk() → next chunk ready
//    Network kabhi idle nahi hota
//
//  FIX — pehle wala problem:
//    sendNextFileChunk() → send() → asio::post(strand_)
//    Matlab chunk queue mein jaane se pehle event loop ghoomta tha
//    = gap = ruk ruk ke transfer
//
//  AB:
//    sendNextFileChunk() → fileQueue_.push_back() seedha
//    Koi post nahi, koi wait nahi
//    write() complete hote hi next chunk already queue mein hai
// ============================================================

ClientSession::ClientSession(asio::ip::tcp::socket socket)
    : socket_(std::move(socket))
    , strand_(asio::make_strand(socket_.get_executor()))
    , fileBuffer_(SESSION_CHUNK_SIZE) // 512KB heap pe
{
    asio::error_code ec;
    socket_.set_option(asio::ip::tcp::no_delay(true), ec);
    socket_.set_option(
        asio::socket_base::send_buffer_size(
            static_cast<int>(4 * SESSION_CHUNK_SIZE)), ec); // 2MB OS buffer
    socket_.set_option(
        asio::socket_base::receive_buffer_size(
            static_cast<int>(4 * SESSION_CHUNK_SIZE)), ec);

    readMsg_.body_.reserve(SESSION_CHUNK_SIZE);
}

void ClientSession::start() { readHeader(); }

bool ClientSession::connected() const
{
    return connected_.load() && socket_.is_open();
}

void ClientSession::disconnect()
{
    auto self = shared_from_this();
    asio::post(strand_, [this, self]()
        {
            connected_ = false;
            if (socket_.is_open())
            {
                asio::error_code ec;
                socket_.cancel(ec);
                socket_.shutdown(
                    asio::ip::tcp::socket::shutdown_both, ec);
                socket_.close(ec);
            }
            if (disconnectCallback_) disconnectCallback_();
        });
}

// ── Outbound — chat/control (high priority) ─────────────────
void ClientSession::sendChat(const Message& msg)
{
    auto self = shared_from_this();
    asio::post(strand_, [this, self, msg]()
        {
            const bool idle = chatQueue_.empty() && fileQueue_.empty();
            chatQueue_.push_back(msg);
            if (idle) write();
        });
}

// ── Outbound — file chunks (low priority) ───────────────────
// NOTE: send() ab sirf fileQueue_ ke liye hai.
// seedha push — koi asio::post hop nahi
// Sirf strand_ pe call karo (sendNextFileChunk se call hota hai)
void ClientSession::send(const Message& msg)
{
    // Yeh function ab directly strand_ pe call hota hai
    // sendNextFileChunk() se — jo already strand_ pe hai
    // Isliye asio::post nahi chahiye — seedha push
    const bool idle = chatQueue_.empty() && fileQueue_.empty();
    fileQueue_.push_back(msg);
    if (idle) write();
}

void ClientSession::createRoom() { sendChat(Message::make(prtocol_type::CreateRoom)); }
void ClientSession::joinRoom(const std::string& c) { sendChat(Message::makeJoinRoom(c)); }
void ClientSession::sendText(const std::string& t) { sendChat(Message::makeText(t)); }

// ── sendFile — pipeline prime ────────────────────────────────
void ClientSession::sendFile(
    const std::string& path,
    std::function<void(uint64_t, uint64_t)> progressCb)
{
    auto self = shared_from_this();
    asio::post(strand_, [this, self, path, progressCb]()
        {
            sendingFile_.open(path, std::ios::binary);
            if (!sendingFile_)
            {
                std::cerr << "[Client] Cannot open: " << path << '\n';
                return;
            }

            std::error_code fec;
            fileBytesTotal_ = std::filesystem::file_size(path, fec);
            if (fec) fileBytesTotal_ = 0;

            fileBytesSent_ = 0;
            fileProgressCb_ = progressCb;
            sendingFileData_ = true;
            pipelineInFlight_ = 0;

            const std::string fname =
                std::filesystem::path(path).filename().string();
            sendChat(Message::makeFileStart(fname));

            // Pipeline prime — PIPELINE_DEPTH chunks seedha queue mein
            // Koi async hop nahi — sab ek saath ready
            for (int i = 0; i < PIPELINE_DEPTH; i++)
            {
                if (!sendingFileData_) break;
                sendNextFileChunk();
            }
        });
}

// ── Read pipeline ────────────────────────────────────────────

void ClientSession::readHeader()
{
    auto self = shared_from_this();
    asio::async_read(socket_,
        asio::buffer(&readMsg_.header_, sizeof(ProtocolHeader)),
        asio::bind_executor(strand_,
            [this, self](std::error_code ec, std::size_t)
            {
                if (ec) { fail("readHeader", ec); return; }

                readMsg_.header_.type = ntohs(readMsg_.header_.type);
                readMsg_.header_.size = ntohl(readMsg_.header_.size);

                constexpr uint32_t MAX_BODY = 8u * 1024u * 1024u;
                if (readMsg_.header_.size > MAX_BODY)
                {
                    disconnect(); return;
                }

                readMsg_.body_.resize(readMsg_.header_.size);

                if (readMsg_.header_.size == 0)
                {
                    handleMessage();
                    readHeader();
                    return;
                }
                readBody();
            }));
}

void ClientSession::readBody()
{
    auto self = shared_from_this();
    asio::async_read(socket_,
        asio::buffer(readMsg_.body_.data(), readMsg_.body_.size()),
        asio::bind_executor(strand_,
            [this, self](std::error_code ec, std::size_t)
            {
                if (ec) { fail("readBody", ec); return; }
                handleMessage();
                readHeader();
            }));
}

void ClientSession::handleMessage()
{
    const auto type = static_cast<prtocol_type>(readMsg_.header_.type);

    switch (type)
    {
    case prtocol_type::RoomCode:
        std::cout << "[Client] Room: " << readMsg_.getString() << '\n';
        break;

    case prtocol_type::TextMessage:
        std::cout << "[Client] Msg: " << readMsg_.getString() << '\n';
        break;

    case prtocol_type::FileStart:
    {
        receivedFileName_ = readMsg_.body_.empty()
            ? "received_file" : readMsg_.getString();
        auto p = makeUniqueFilePath(getDownloadsFolder(),
            receivedFileName_);
        if (!fileReceiver_.openForReceive(p.string()))
            std::cerr << "[Client] Cannot open save: " << p << '\n';
        break;
    }

    case prtocol_type::FileChunk:
        if (fileReceiver_.isReceiveOpen())
            fileReceiver_.writeChunk(
                reinterpret_cast<const char*>(readMsg_.body_.data()),
                readMsg_.body_.size());
        break;

    case prtocol_type::FileEnd:
        fileReceiver_.close();
        std::cout << "[Client] File saved: " << receivedFileName_ << '\n';
        break;

    case prtocol_type::Error:
        std::cerr << "[Client] Error: " << readMsg_.getString() << '\n';
        break;

    default: break;
    }

    if (messageCallback_) messageCallback_(readMsg_);
}

// ── Write pipeline ───────────────────────────────────────────

void ClientSession::write()
{
    if (chatQueue_.empty() && fileQueue_.empty()) return;

    auto self = shared_from_this();

    // Chat priority: pehle chat/control, phir file chunks
    const bool fromChat = !chatQueue_.empty();
    auto& msg = fromChat ? chatQueue_.front() : fileQueue_.front();

    auto netHdr = std::make_shared<ProtocolHeader>();
    netHdr->type = htons(msg.header_.type);
    netHdr->size = htonl(msg.header_.size);

    std::vector<asio::const_buffer> bufs;
    bufs.push_back(asio::buffer(netHdr.get(), sizeof(ProtocolHeader)));
    if (!msg.body_.empty())
        bufs.push_back(asio::buffer(msg.body_.data(), msg.body_.size()));

    asio::async_write(socket_, bufs,
        asio::bind_executor(strand_,
            [this, self, netHdr, fromChat](std::error_code ec, std::size_t)
            {
                if (ec) { fail("write", ec); return; }

                if (fromChat) chatQueue_.pop_front();
                else          fileQueue_.pop_front();

                // Pipeline refill — ek chunk gaya, ek aur daalo
                // seedha push — koi post nahi, koi wait nahi
                if (!fromChat && sendingFileData_)
                {
                    if (pipelineInFlight_ > 0) pipelineInFlight_--;
                    sendNextFileChunk();
                }

                if (!chatQueue_.empty() || !fileQueue_.empty())
                    write();
            }));
}

// ── File chunking — NO WAIT pipeline ────────────────────────
// seedha fileQueue_.push_back() — koi asio::post nahi
// write() complete hote hi next chunk already queue mein hoga

void ClientSession::sendNextFileChunk()
{
    if (!sendingFileData_ || !sendingFile_.is_open()) return;

    // Pipeline cap — zyada chunks queue mein nahi daalo
    if (pipelineInFlight_ >= PIPELINE_DEPTH) return;

    sendingFile_.read(fileBuffer_.data(),
        static_cast<std::streamsize>(fileBuffer_.size()));
    const std::streamsize bytes = sendingFile_.gcount();

    if (bytes <= 0)
    {
        // EOF — sirf tab FileEnd bhejo jab pipeline drain ho
        if (pipelineInFlight_ == 0)
        {
            sendChat(Message::make(prtocol_type::FileEnd));
            sendingFileData_ = false;
            sendingFile_.close();
            if (fileProgressCb_)
                fileProgressCb_(fileBytesTotal_, fileBytesTotal_);
        }
        return;
    }

    Message chunk;
    chunk.header_.type = static_cast<uint16_t>(prtocol_type::FileChunk);
    chunk.body_.assign(
        fileBuffer_.begin(),
        fileBuffer_.begin() + bytes);
    chunk.header_.size = static_cast<uint32_t>(chunk.body_.size());

    fileBytesSent_ += static_cast<uint64_t>(bytes);
    if (fileProgressCb_)
        fileProgressCb_(fileBytesSent_, fileBytesTotal_);

    pipelineInFlight_++;

    // KEY: seedha push — send() ka asio::post bypass
    // write() already strand_ pe hai — safe hai
    const bool idle = chatQueue_.empty() && fileQueue_.empty();
    fileQueue_.push_back(std::move(chunk));
    if (idle) write();
}

// ── Error handling ───────────────────────────────────────────

void ClientSession::fail(const std::string& where,
    const asio::error_code& ec)
{
    if (ec == asio::error::operation_aborted ||
        ec == asio::error::eof)
    {
        disconnect(); return;
    }
    std::cerr << "[Client] " << where << ": " << ec.message() << '\n';
    disconnect();
}

void ClientSession::setMessageCallback(MessageCallback cb)
{
    messageCallback_ = std::move(cb);
}
void ClientSession::setDisconnectCallback(DisconnectCallback cb)
{
    disconnectCallback_ = std::move(cb);
}

// ── Download helpers ─────────────────────────────────────────

std::filesystem::path ClientSession::getDownloadsFolder()
{
#ifdef _WIN32
    const char* home = std::getenv("USERPROFILE");
#else
    const char* home = std::getenv("HOME");
#endif
    if (home)
    {
        auto p = std::filesystem::path(home) / "Downloads";
        std::error_code ec;
        std::filesystem::create_directories(p, ec);
        return p;
    }
    return std::filesystem::current_path();
}

std::filesystem::path ClientSession::makeUniqueFilePath(
    const std::filesystem::path& dir,
    const std::string& filename)
{
    auto base = std::filesystem::path(filename);
    auto stem = base.stem().string();
    auto ext = base.extension().string();
    auto cand = dir / filename;
    int  n = 1;
    while (std::filesystem::exists(cand))
        cand = dir / (stem + " (" + std::to_string(n++) + ")" + ext);
    return cand;
}