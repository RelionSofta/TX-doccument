#pragma once
#include <asio.hpp>
#include <asio/strand.hpp>
#include <iostream>
#include <memory>
#include <deque>
#include <array>
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
    void sendNextChunk();
    void send_file(const std::string& path);
    tcp::socket& socket();

private:
    void doReadHeader();
    void doReadBody();
    void handleMessage();
    void doWrite();
    void disconnect();

private:
    asio::ip::tcp::socket  socket_;
    Message                readMsg_;
    std::deque<Message>    writeQueue_;

    asio::strand<asio::any_io_executor> strand_;
    //asio::strand<asio::io_context::executor_type> strand_;

    std::shared_ptr<Room>        room_;
    std::shared_ptr<RoomHandler> room_H;
    std::unique_ptr<FileHandler> file_;
    std::array<char, CHUNK_SIZE> fileBuffer_{};
    bool sendingFile_ = false;
};