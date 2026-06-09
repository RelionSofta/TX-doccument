#include "ClientSession.h"

#include <iostream>
#include <filesystem>

#ifdef _WIN32
#  include <winsock2.h>
#else
#  include <arpa/inet.h>
#endif

// ============================================================
//  ClientSession.cpp
//
//  Adapted from the user's original ClientSession.cpp:
//   - Chunk size raised to 64 KiB for throughput
//   - File-size query added for progress reporting
//   - sendNextFileChunk uses iterative (not recursive) loop
//     to avoid stack growth on large files
//   - Receive path stores files under a configurable directory
//     (currently cwd; extend via setReceiveDirectory())
// ============================================================

ClientSession::ClientSession(asio::ip::tcp::socket socket)
    : socket_(std::move(socket))
    , strand_(asio::make_strand(socket_.get_executor()))
{
    // TCP tuning for maximum throughput
    asio::error_code ec;
    socket_.set_option(asio::ip::tcp::no_delay(false), ec);
    socket_.set_option(asio::socket_base::send_buffer_size(4 * 1024 * 1024), ec);
    socket_.set_option(asio::socket_base::receive_buffer_size(4 * 1024 * 1024), ec);
}

void ClientSession::start()
{
    readHeader();
}

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
                socket_.cancel(ec);   // unblock pending async_read immediately
                socket_.shutdown(
                    asio::ip::tcp::socket::shutdown_both, ec);
                socket_.close(ec);
            }

            if (disconnectCallback_)
                disconnectCallback_();
        });
}

// ---- Outbound -----------------------------------------------

void ClientSession::send(const Message& msg)
{
    // File chunks go to low-priority queue
    auto self = shared_from_this();
    asio::post(strand_, [this, self, msg]()
        {
            bool writing = !chatQueue_.empty() || !fileQueue_.empty();
            fileQueue_.push_back(msg);
            if (!writing)
                write();
        });
}

void ClientSession::sendChat(const Message& msg)
{
    // Chat messages go to high-priority queue — sent before file chunks
    auto self = shared_from_this();
    asio::post(strand_, [this, self, msg]()
        {
            bool writing = !chatQueue_.empty() || !fileQueue_.empty();
            chatQueue_.push_back(msg);
            if (!writing)
                write();
        });
}

void ClientSession::createRoom()
{
    sendChat(Message::make(prtocol_type::CreateRoom));
}

void ClientSession::joinRoom(const std::string& code)
{
    sendChat(Message::makeJoinRoom(code));
}

void ClientSession::sendText(const std::string& text)
{
    sendChat(Message::makeText(text));  // high-priority queue
}

void ClientSession::sendFile(
    const std::string& path,
    std::function<void(uint64_t, uint64_t)> progressCallback)
{
    // FIXED: Everything runs on strand — no cross-thread file access
    auto self = shared_from_this();
    auto cb = std::make_shared<std::function<void(uint64_t, uint64_t)>>(
        std::move(progressCallback));

    asio::post(strand_, [this, self, path, cb]()
        {
            sendingFile_.open(path, std::ios::binary);
            if (!sendingFile_)
            {
                std::cerr << "[Session] Failed to open: " << path << '\n';
                return;
            }

            std::error_code fec;
            fileBytesTotal_ = std::filesystem::file_size(path, fec);
            if (fec) fileBytesTotal_ = 0;

            fileBytesSent_ = 0;
            fileProgressCb_ = *cb;
            sendingFileData_ = true;
            pipelineInFlight_ = 0;

            std::string fname =
                std::filesystem::path(path).filename().string();
            sendChat(Message::makeFileStart(fname));

            // Pipeline start — PIPELINE_DEPTH chunks queue mein
            for (int i = 0; i < PIPELINE_DEPTH; i++)
            {
                if (!sendingFileData_) break;
                sendNextFileChunk();
            }
        });
}

// ---- Read pipeline ------------------------------------------

void ClientSession::readHeader()
{
    auto self = shared_from_this();

    asio::async_read(
        socket_,
        asio::buffer(&readMsg_.header_, sizeof(ProtocolHeader)),
        asio::bind_executor(strand_,
            [this, self](std::error_code ec, std::size_t)
            {
                if (ec) { fail("readHeader", ec); return; }

                readMsg_.header_.type =
                    ntohs(readMsg_.header_.type);
                readMsg_.header_.size =
                    ntohl(readMsg_.header_.size);

                constexpr uint32_t MAX_BODY = 8u * 1024u * 1024u; // 8MB chunks support

                if (readMsg_.header_.size > MAX_BODY)
                {
                    std::cerr << "[Session] Oversized body — disconnecting\n";
                    disconnect();
                    return;
                }

                // Reuse existing allocation if possible
                readMsg_.body_.resize(readMsg_.header_.size);
                readBody();
            }));
}

void ClientSession::readBody()
{
    auto self = shared_from_this();

    // If body is empty, skip async_read and handle immediately
    if (readMsg_.body_.empty())
    {
        handleMessage();
        readHeader();
        return;
    }

    asio::async_read(
        socket_,
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
    auto type = static_cast<prtocol_type>(readMsg_.header_.type);

    switch (type)
    {
    case prtocol_type::RoomCode:
    {
        std::string code = readMsg_.getString();
        std::cout << "[Session] Room code: " << code << '\n';
        break;
    }

    case prtocol_type::TextMessage:
    {
        std::cout << "[Session] Text: " << readMsg_.getString() << '\n';
        break;
    }

    case prtocol_type::FileStart:
    {
        // Body may carry the original filename; fall back to timestamp
        if (!readMsg_.body_.empty())
            receivedFileName_ = readMsg_.getString();
        else
            receivedFileName_ = "received_file.bin";

        auto savePath = makeUniqueFilePath(
            getDownloadsFolder(), receivedFileName_);

        std::cout << "[Session] Receiving file -> "
            << savePath.string() << '\n';

        if (!fileReceiver_.openForReceive(savePath.string()))
            std::cerr << "[Session] Cannot open save path: "
            << savePath << '\n';
        break;
    }

    case prtocol_type::FileChunk:
    {
        if (fileReceiver_.isReceiveOpen())
        {
            fileReceiver_.writeChunk(
                reinterpret_cast<const char*>(readMsg_.body_.data()),
                readMsg_.body_.size());
        }
        break;
    }

    case prtocol_type::FileEnd:
    {
        fileReceiver_.close();

        std::cout << "[Session] File saved to Downloads: "
            << receivedFileName_ << '\n';
        break;
    }

    case prtocol_type::Error:
    {
        std::cerr << "[Session] Server error: "
            << readMsg_.getString() << '\n';
        break;
    }

    default:
        break;
    }

    if (messageCallback_)
        messageCallback_(readMsg_);
}

// ---- Write pipeline -----------------------------------------

void ClientSession::write()
{
    auto self = shared_from_this();

    // Safety check — should never happen but prevents crash
    if (chatQueue_.empty() && fileQueue_.empty()) return;

    // Chat messages have priority over file chunks
    bool fromChat = !chatQueue_.empty();
    auto& msg = fromChat ? chatQueue_.front() : fileQueue_.front();

    // Encode header into network byte order into a shared buffer
    auto netHeader = std::make_shared<ProtocolHeader>();
    netHeader->type = htons(msg.header_.type);
    netHeader->size = htonl(msg.header_.size);

    std::vector<asio::const_buffer> buffers;
    buffers.push_back(
        asio::buffer(netHeader.get(), sizeof(ProtocolHeader)));

    if (!msg.body_.empty())
        buffers.push_back(
            asio::buffer(msg.body_.data(), msg.body_.size()));

    asio::async_write(
        socket_, buffers,
        asio::bind_executor(strand_,
            [this, self, netHeader, fromChat](std::error_code ec, std::size_t)
            {
                if (ec) { fail("write", ec); return; }

                // Pop from whichever queue was used (fromChat captured above)
                if (fromChat)
                    chatQueue_.pop_front();
                else
                    fileQueue_.pop_front();

                // Pipeline refill
                if (!fromChat && sendingFileData_)
                {
                    if (pipelineInFlight_ > 0)
                        pipelineInFlight_--;
                    if (chatQueue_.empty())
                        sendNextFileChunk();
                }

                if (!chatQueue_.empty() || !fileQueue_.empty())
                    write();
            }));
}

// ---- File chunking ------------------------------------------

void ClientSession::sendNextFileChunk()
{
    // ONE chunk per call — next chunk is triggered from
    // write() completion handler, keeping the queue short
    // and preventing memory explosion on large files.

    if (!sendingFileData_ || !sendingFile_.is_open())
        return;

    sendingFile_.read(fileBuffer_.data(),
        static_cast<std::streamsize>(fileBuffer_.size()));
    std::streamsize bytes = sendingFile_.gcount();

    if (bytes <= 0)
    {
        // EOF — FileEnd sirf tab bhejo jab pipeline empty ho
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
    chunk.header_.size =
        static_cast<uint32_t>(chunk.body_.size());

    fileBytesSent_ += static_cast<uint64_t>(bytes);

    if (fileProgressCb_)
        fileProgressCb_(fileBytesSent_, fileBytesTotal_);

    pipelineInFlight_++;
    send(chunk);
    // Next chunk triggered by write() completion handler
}

// ---- Error handling -----------------------------------------

void ClientSession::fail(
    const std::string& where,
    const asio::error_code& ec)
{
    if (ec == asio::error::operation_aborted ||
        ec == asio::error::eof)
    {
        // Normal closure — not an error worth printing
        disconnect();
        return;
    }

    std::cerr << "[Session] " << where << ": " << ec.message() << '\n';
    disconnect();
}

// ---- Callbacks ----------------------------------------------

void ClientSession::setMessageCallback(MessageCallback cb)
{
    messageCallback_ = std::move(cb);
}

void ClientSession::setDisconnectCallback(DisconnectCallback cb)
{
    disconnectCallback_ = std::move(cb);
}

// ---- Download path helpers ----------------------------------

std::filesystem::path ClientSession::getDownloadsFolder()
{
#ifdef _WIN32
    // Windows: use USERPROFILE\Downloads
    const char* home = std::getenv("USERPROFILE");
    if (home)
    {
        std::filesystem::path p =
            std::filesystem::path(home) / "Downloads";
        std::error_code ec;
        std::filesystem::create_directories(p, ec);
        return p;
    }
#else
    // Linux / macOS: use $HOME/Downloads
    const char* home = std::getenv("HOME");
    if (home)
    {
        std::filesystem::path p =
            std::filesystem::path(home) / "Downloads";
        std::error_code ec;
        std::filesystem::create_directories(p, ec);
        return p;
    }
#endif
    // Fallback: current working directory
    return std::filesystem::current_path();
}

std::filesystem::path ClientSession::makeUniqueFilePath(
    const std::filesystem::path& dir,
    const std::string& filename)
{
    // Split stem and extension: "photo.png" -> "photo" + ".png"
    std::filesystem::path base(filename);
    std::string stem = base.stem().string();
    std::string ext = base.extension().string();

    std::filesystem::path candidate = dir / filename;
    int counter = 1;

    // If file already exists, append (1), (2), ...
    while (std::filesystem::exists(candidate))
    {
        candidate = dir / (stem + " (" +
            std::to_string(counter++) + ")" + ext);
    }

    return candidate;
}