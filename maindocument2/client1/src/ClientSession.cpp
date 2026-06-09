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
//  Changes from original:
//  1. fileBuffer_ ab 4MB hai — server ke chunk size se match
//  2. readMsg_.body_ constructor mein 4MB reserve — har chunk
//     pe naya allocation nahi hoga
//  3. TCP receive buffer bhi 2x CHUNK_SIZE (8MB) — OS level
//     pe zyada data hold kar sakta hai
//  4. Pipeline send side already correct tha — untouched
//  5. Receive side: writeChunk seedha call hota hai — no change
//     needed kyunki async_read already non-blocking hai
// ============================================================

// Server ke protocol.h se match karna zaroori hai
// SESSION_CHUNK_SIZE aur PIPELINE_DEPTH ClientSession.h se aate hain

ClientSession::ClientSession(asio::ip::tcp::socket socket)
    : socket_(std::move(socket))
    , strand_(asio::make_strand(socket_.get_executor()))
{
    asio::error_code ec;

    // Nagle algorithm off — chhote packets immediately flush honge
    // File transfer mein fayda nahi lekin chat messages fast honge
    socket_.set_option(asio::ip::tcp::no_delay(true), ec);

    // Send + Receive buffer = 2 chunks — pipeline ke liye enough
    socket_.set_option(
        asio::socket_base::send_buffer_size(
            static_cast<int>(2 * SESSION_CHUNK_SIZE)), ec);
    socket_.set_option(
        asio::socket_base::receive_buffer_size(
            static_cast<int>(2 * SESSION_CHUNK_SIZE)), ec);

    // PERF: body_ ke liye ek baar 4MB reserve karo
    // Har incoming chunk pe vector reallocate nahi hoga
    readMsg_.body_.reserve(SESSION_CHUNK_SIZE);

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
                socket_.cancel(ec);
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
    sendChat(Message::makeText(text));
}

void ClientSession::sendFile(
    const std::string& path,
    std::function<void(uint64_t, uint64_t)> progressCallback)
{
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

            // Pipeline prime — PIPELINE_DEPTH chunks ek saath queue mein
            // [C1][C2][C3] ready → network hamesha busy
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

                // Max body = 8MB safety valve
                constexpr uint32_t MAX_BODY = 8u * 1024u * 1024u;
                if (readMsg_.header_.size > MAX_BODY)
                {
                    std::cerr << "[Session] Oversized body — disconnecting\n";
                    disconnect();
                    return;
                }

                // resize() reuses reserved memory if size <= 4MB
                // No new heap allocation for normal file chunks
                readMsg_.body_.resize(readMsg_.header_.size);
                readBody();
            }));
}

void ClientSession::readBody()
{
    auto self = shared_from_this();

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

                // handleMessage() ke andar disk write hota hai
                // async_read complete hone ke baad hi — sahi order
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
        receivedFileName_ = readMsg_.body_.empty()
            ? "received_file.bin"
            : readMsg_.getString();

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
        // writeChunk() — direct disk write
        // OS page cache buffer karta hai — explicit buffering zaroorat nahi
        // 4MB chunk ek baar mein likha jaata hai — fast
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
        // close() andar flush() call hota hai — sab disk pe
        fileReceiver_.close();
        std::cout << "[Session] File saved: "
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
    if (chatQueue_.empty() && fileQueue_.empty()) return;

    auto self = shared_from_this();

    bool fromChat = !chatQueue_.empty();
    auto& msg = fromChat ? chatQueue_.front() : fileQueue_.front();

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

                if (fromChat)
                    chatQueue_.pop_front();
                else
                    fileQueue_.pop_front();

                // Pipeline refill — ek chunk gaya toh ek aur queue mein
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
    if (!sendingFileData_ || !sendingFile_.is_open())
        return;

    sendingFile_.read(fileBuffer_.data(),
        static_cast<std::streamsize>(fileBuffer_.size()));
    std::streamsize bytes = sendingFile_.gcount();

    if (bytes <= 0)
    {
        // EOF — pipeline drain hone ka wait karo phir FileEnd bhejo
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
    chunk.header_.type =
        static_cast<uint16_t>(prtocol_type::FileChunk);
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
}

// ---- Error handling -----------------------------------------

void ClientSession::fail(
    const std::string& where,
    const asio::error_code& ec)
{
    if (ec == asio::error::operation_aborted ||
        ec == asio::error::eof)
    {
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
    const char* home = std::getenv("USERPROFILE");
#else
    const char* home = std::getenv("HOME");
#endif
    if (home)
    {
        std::filesystem::path p =
            std::filesystem::path(home) / "Downloads";
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
    std::filesystem::path base(filename);
    std::string stem = base.stem().string();
    std::string ext = base.extension().string();

    std::filesystem::path candidate = dir / filename;
    int counter = 1;

    while (std::filesystem::exists(candidate))
    {
        candidate = dir /
            (stem + " (" + std::to_string(counter++) + ")" + ext);
    }

    return candidate;
}