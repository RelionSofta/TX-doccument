#pragma once
#include <asio.hpp>
#include <asio/strand.hpp>
#include <iostream>
#include <memory>
#include <deque>
#include <vector>    // array → vector (heap allocation)
#include <string>
#include "Message.h"
#include "protocol.h"

using asio::ip::tcp;
class RoomHandler;
class Room;
class FileHandler;

class Session : public std::enable_shared_from_this<Session>
{
public:
    explicit Session(tcp::socket socket,
        std::shared_ptr<RoomHandler> room);

    void start();
    void send(const Message& msg);
    void send_file(const std::string& path);
    void pushToQueue(const Message& msg);  // ← ADD
    tcp::socket& socket();

private:
    void doReadHeader();
    void doReadBody();
    void handleMessage();
    void doWrite();
    void disconnect();
    void pushOneChunk();    // ← ADD
    void scheduleChunks();  // ← ADD

private:
    asio::ip::tcp::socket socket_;
    Message               readMsg_;
    std::deque<Message>   writeQueue_;

    asio::strand<asio::any_io_executor> strand_;

    std::shared_ptr<Room>        room_;
    std::shared_ptr<RoomHandler> room_H;
    std::unique_ptr<FileHandler> file_;

    std::vector<char> fileBuffer_;  // ← array → vector (4MB stack pe nahi jaayega)

    bool sendingFile_ = false;

    static constexpr int PIPELINE_DEPTH = 3;  // original rakha
    int chunksInFlight_ = 0;

    // ← MAX_WRITE_QUEUE ki jagah do alag limits
    static constexpr std::size_t MAX_CHAT_QUEUE = 64;
    static constexpr std::size_t MAX_CHUNK_QUEUE = 8;
};