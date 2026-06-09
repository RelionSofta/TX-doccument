#include "Session.h"
#include "Room.h"
#include "FileTransfer.h"
#include "RoomHandler.h"

#ifdef _WIN32
#  include <winsock2.h>
#else
#  include <arpa/inet.h>
#endif

Session::Session(tcp::socket socket,
    std::shared_ptr<RoomHandler> room)
    : socket_(std::move(socket))
    , strand_(asio::make_strand(socket_.get_executor()))
    , room_H(std::move(room))
    , fileBuffer_(CHUNK_SIZE)
{
    asio::error_code ec;
    socket_.set_option(asio::ip::tcp::no_delay(true), ec);
    socket_.set_option(
        asio::socket_base::send_buffer_size(4 * CHUNK_SIZE), ec);
    socket_.set_option(
        asio::socket_base::receive_buffer_size(2 * CHUNK_SIZE), ec);
}

void Session::start() { doReadHeader(); }
tcp::socket& Session::socket() { return socket_; }

// ── send() — chat + control messages ────────────────────────
void Session::send(const Message& msg)
{
    auto self = shared_from_this();
    asio::post(strand_, [self, msg]()
        {
            auto type = static_cast<MessageType>(
                msg.header().type);
            bool isFileChunk = (type == MessageType::FileChunk);

            if (isFileChunk &&
                self->writeQueue_.size() >= MAX_CHUNK_QUEUE)
                return;

            if (!isFileChunk &&
                self->writeQueue_.size() >= MAX_CHAT_QUEUE)
                return;

            const bool idle = self->writeQueue_.empty();
            self->writeQueue_.push_back(msg);
            if (idle)
                self->doWrite();
        });
}

// ── pushToQueue — broadcastDirect() ke liye ─────────────────
// Seedha writeQueue_ mein push — koi asio::post nahi
// Sirf tab call karo jab already strand_ pe ho
void Session::pushToQueue(const Message& msg)
{
    const bool idle = writeQueue_.empty();
    writeQueue_.push_back(msg);
    if (idle)
        doWrite();
}

// ── Read pipeline ────────────────────────────────────────────

void Session::doReadHeader()
{
    auto self = shared_from_this();
    asio::async_read(socket_,
        asio::buffer(&readMsg_.header(), sizeof(PacketHeader)),
        asio::bind_executor(strand_,
            [self](asio::error_code ec, std::size_t)
            {
                if (ec) { self->disconnect(); return; }

                self->readMsg_.header().type =
                    ntohs(self->readMsg_.header().type);
                self->readMsg_.header().size =
                    ntohl(self->readMsg_.header().size);

                constexpr uint32_t MAX_BODY = 8u * 1024u * 1024u;
                if (self->readMsg_.header().size > MAX_BODY)
                {
                    self->disconnect();
                    return;
                }

                self->readMsg_.body().resize(
                    self->readMsg_.header().size);

                if (self->readMsg_.header().size == 0)
                {
                    self->handleMessage();
                    self->doReadHeader();
                    return;
                }
                self->doReadBody();
            }));
}

void Session::doReadBody()
{
    auto self = shared_from_this();
    asio::async_read(socket_,
        asio::buffer(readMsg_.body().data(),
            readMsg_.body().size()),
        asio::bind_executor(strand_,
            [self](asio::error_code ec, std::size_t)
            {
                if (ec) { self->disconnect(); return; }
                self->handleMessage();
                self->doReadHeader();
            }));
}

void Session::handleMessage()
{
    auto type = static_cast<MessageType>(
        readMsg_.header().type);

    switch (type)
    {
    case MessageType::CreateRoom:
    {
        auto [code, room] = room_H->createRoom();
        room_ = room;
        room_->join(shared_from_this());

        Message reply;
        reply.header().type =
            static_cast<uint16_t>(MessageType::RoomCode);
        reply.body().assign(code.begin(), code.end());
        reply.header().size =
            static_cast<uint32_t>(reply.body().size());
        send(reply);
        break;
    }

    case MessageType::JoinRoom:
    {
        std::string code(readMsg_.body().begin(),
            readMsg_.body().end());
        auto room = room_H->getRoom(code);
        if (!room)
        {
            Message err;
            err.header().type =
                static_cast<uint16_t>(MessageType::Error);
            std::string msg = "Invalid room code";
            err.body().assign(msg.begin(), msg.end());
            err.header().size =
                static_cast<uint32_t>(err.body().size());
            send(err);
        }
        else
        {
            room_ = room;
            room_->join(shared_from_this());

            Message ok;
            ok.header().type =
                static_cast<uint16_t>(MessageType::RoomCode);
            ok.body().assign(code.begin(), code.end());
            ok.header().size =
                static_cast<uint32_t>(ok.body().size());
            send(ok);
        }
        break;
    }

    default:
        if (room_)
            room_->broadcast(readMsg_, shared_from_this());
        break;
    }
}

// ── Write pipeline ───────────────────────────────────────────

void Session::doWrite()
{
    if (writeQueue_.empty()) return;

    auto self = shared_from_this();
    const Message& msg = writeQueue_.front();

    auto netHdr = std::make_shared<PacketHeader>();
    netHdr->type = htons(msg.header().type);
    netHdr->size = htonl(msg.header().size);

    std::array<asio::const_buffer, 2> bufs = {
        asio::buffer(netHdr.get(), sizeof(PacketHeader)),
        asio::buffer(msg.body().data(), msg.body().size())
    };

    asio::async_write(socket_, bufs,
        asio::bind_executor(strand_,
            [self, netHdr](asio::error_code ec, std::size_t)
            {
                if (ec) { self->disconnect(); return; }

                const auto type = static_cast<MessageType>(
                    self->writeQueue_.front().header().type);
                if (type == MessageType::FileChunk)
                    self->chunksInFlight_--;

                self->writeQueue_.pop_front();

                if (self->sendingFile_)
                    self->scheduleChunks();

                if (!self->writeQueue_.empty())
                    self->doWrite();
            }));
}

// ── File pipeline ────────────────────────────────────────────

void Session::pushOneChunk()
{
    if (!file_ || !sendingFile_) return;

    const std::size_t bytesRead =
        file_->readChunk(
            *reinterpret_cast<std::array<char, CHUNK_SIZE>*>(
                fileBuffer_.data()));

    if (bytesRead == 0)
    {
        // EOF
        Message endMsg;
        endMsg.header().type =
            static_cast<uint16_t>(MessageType::FileEnd);
        endMsg.header().size = 0;
        if (room_)
            room_->broadcast(endMsg, shared_from_this());

        sendingFile_ = false;
        chunksInFlight_ = 0;
        file_.reset();
        return;
    }

    Message chunkMsg;
    chunkMsg.header().type =
        static_cast<uint16_t>(MessageType::FileChunk);
    chunkMsg.body().assign(
        fileBuffer_.begin(),
        fileBuffer_.begin() + bytesRead);
    chunkMsg.header().size =
        static_cast<uint32_t>(chunkMsg.body().size());

    if (room_)
        room_->broadcastDirect(chunkMsg, shared_from_this());

    chunksInFlight_++;
}

void Session::scheduleChunks()
{
    while (sendingFile_ &&
        chunksInFlight_ < PIPELINE_DEPTH &&
        file_)
    {
        pushOneChunk();
    }
}

void Session::send_file(const std::string& path)
{
    auto self = shared_from_this();
    asio::post(strand_, [self, path]()
        {
            self->file_ = std::make_unique<FileHandler>();
            if (!self->file_->openRead(path))
            {
                std::cerr << "[Session] Cannot open file: "
                    << path << '\n';
                return;
            }

            self->sendingFile_ = true;
            self->chunksInFlight_ = 0;

            Message startMsg;
            startMsg.header().type =
                static_cast<uint16_t>(MessageType::FileStart);
            startMsg.header().size = 0;
            if (self->room_)
                self->room_->broadcast(startMsg,
                    self->shared_from_this());

            self->scheduleChunks();

            if (!self->writeQueue_.empty())
                self->doWrite();
        });
}

void Session::disconnect()
{
    auto self = shared_from_this();
    asio::post(strand_, [self]()
        {
            if (self->socket_.is_open())
            {
                asio::error_code ec;
                self->socket_.cancel(ec);
                self->socket_.shutdown(
                    tcp::socket::shutdown_both, ec);
                self->socket_.close(ec);
            }
            if (self->room_)
            {
                self->room_->leave(self);
                self->room_.reset();
            }
        });
}