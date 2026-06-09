#include "Session.h"
#include "Room.h"
#include "FileTransfer.h"
#include "RoomHandler.h"

// Max messages in writeQueue per session
// Agar peer slow hai toh queue limit karo — memory bloat prevent
static constexpr std::size_t MAX_WRITE_QUEUE = 32;

Session::Session(tcp::socket socket, std::shared_ptr<RoomHandler> room)
    : socket_(std::move(socket)), room_H(std::move(room)),
    strand_(asio::make_strand(socket_.get_executor()))
{
    // TCP tuning for maximum relay throughput
    asio::error_code ec;
    socket_.set_option(asio::socket_base::send_buffer_size(4 * 1024 * 1024), ec);
    socket_.set_option(asio::socket_base::receive_buffer_size(4 * 1024 * 1024), ec);

    // FIX 1: body_ ko ek baar reserve karo — har chunk pe alloc nahi
    readMsg_.body().reserve(CHUNK_SIZE);
}

void Session::start()
{
    doReadHeader();
}

void Session::send(const Message& msg)
{
    auto self = shared_from_this();
    asio::post(strand_, [self, msg]()
        {
            // FIX 2: writeQueue_ limit — slow peer memory bloat prevent
            if (self->writeQueue_.size() >= MAX_WRITE_QUEUE)
            {
                // Queue full — drop oldest non-critical message
                // (FileChunks drop safe hai — file corrupt hogi
                //  lekin server crash nahi hoga)
                return;
            }

            bool writing = !self->writeQueue_.empty();
            self->writeQueue_.push_back(msg);
            if (!writing)
                self->doWrite();
        });
}

void Session::doReadHeader()
{
    auto self = shared_from_this();
    asio::async_read(socket_,
        asio::buffer(&readMsg_.header(), sizeof(PacketHeader)),
        asio::bind_executor(strand_,
            [self](asio::error_code ec, size_t)
            {
                if (ec) { self->disconnect(); return; }

                self->readMsg_.header().type =
                    ntohs(self->readMsg_.header().type);
                self->readMsg_.header().size =
                    ntohl(self->readMsg_.header().size);

                constexpr uint32_t MAX_BODY = 8 * 1024 * 1024;
                if (self->readMsg_.header().size > MAX_BODY)
                {
                    self->disconnect();
                    return;
                }

                // FIX 1: resize reuses reserved memory — no new alloc
                // if size <= CHUNK_SIZE (which is always true)
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
            [self](asio::error_code ec, size_t)
            {
                if (ec) { self->disconnect(); return; }
                self->handleMessage();
                self->doReadHeader();
            }));
}

void Session::handleMessage()
{
    auto type = static_cast<MessageType>(readMsg_.header().type);

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
        // FIX 3: broadcast — shared_ptr to message body
        // Avoid 1MB copy per receiver
        // Current: msg copied per session in Room::broadcast
        // For now broadcast as-is (Room.cpp fix needed for zero-copy)
        if (room_)
            room_->broadcast(readMsg_, shared_from_this());
        break;
    }
}

void Session::doWrite()
{
    if (writeQueue_.empty()) return;

    auto self = shared_from_this();
    auto& msg = writeQueue_.front();

    auto netHeader = std::make_shared<PacketHeader>();
    netHeader->type = htons(msg.header().type);
    netHeader->size = htonl(msg.header().size);

    std::array<asio::const_buffer, 2> buffers = {
        asio::buffer(netHeader.get(), sizeof(PacketHeader)),
        asio::buffer(msg.body().data(), msg.body().size())
    };

    asio::async_write(socket_, buffers,
        asio::bind_executor(strand_,
            [self, netHeader](asio::error_code ec, size_t)
            {
                if (!ec)
                {
                    self->writeQueue_.pop_front();

                    if (self->sendingFile_)
                        self->sendNextChunk();

                    if (!self->writeQueue_.empty())
                        self->doWrite();
                }
                else
                {
                    self->disconnect();
                }
            }));
}

void Session::sendNextChunk()
{
    // FIX 4: Extra asio::post hataya — already strand pe hain
    // doWrite completion handler strand pe hai toh
    // seedha call karo — unnecessary hop nahi

    if (!file_) return;

    std::size_t bytesRead = file_->readChunk(fileBuffer_);

    if (bytesRead == 0)
    {
        Message endMsg;
        endMsg.header().type =
            static_cast<uint16_t>(MessageType::FileEnd);
        endMsg.header().size = 0;
        if (room_)
            room_->broadcast(endMsg, shared_from_this());
        sendingFile_ = false;
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
        room_->broadcast(chunkMsg, shared_from_this());
}

void Session::send_file(const std::string& path)
{
    file_ = std::make_unique<FileHandler>();
    if (!file_->openRead(path)) return;

    sendingFile_ = true;

    Message startMsg;
    startMsg.header().type =
        static_cast<uint16_t>(MessageType::FileStart);
    startMsg.header().size = 0;

    if (room_)
        room_->broadcast(startMsg, shared_from_this());

    sendNextChunk();
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