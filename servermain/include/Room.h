#pragma once
#include <memory>
#include <set>
#include <string>
#include <mutex>
#include "Message.h"

class Session;
class RoomHandler;

class Room
{
public:
    Room(std::string code, std::weak_ptr<RoomHandler> handler);

    void join(std::shared_ptr<Session> session);
    void leave(std::shared_ptr<Session> session);
    void broadcast(const Message& msg,
        std::shared_ptr<Session> sender);

    bool        empty() const;
    std::size_t size()  const;
    const std::string& code() const { return code_; }

private:
    std::string              code_;
    std::weak_ptr<RoomHandler> handler_;

    mutable std::mutex mutex_;   // FIXED: protects sessions_
    std::set<
        std::weak_ptr<Session>,
        std::owner_less<std::weak_ptr<Session>>> sessions_;
};