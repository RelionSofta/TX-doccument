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

    // Normal broadcast — session->send() → asio::post() (control msgs)
    void broadcast(const Message& msg,
        std::shared_ptr<Session> sender);

    // ── NEW: Direct broadcast — file chunks ke liye ──────────
    // session->send() ka asio::post() bypass karta hai.
    // Seedha receiver ki writeQueue_ mein push karta hai.
    // Call karo SIRF jab tum already sender ke strand_ pe ho.
    void broadcastDirect(const Message& msg,
        std::shared_ptr<Session> sender);

    bool        empty() const;
    std::size_t size()  const;
    const std::string& code() const { return code_; }

private:
    std::string                code_;
    std::weak_ptr<RoomHandler> handler_;

    mutable std::mutex mutex_;
    std::set<
        std::weak_ptr<Session>,
        std::owner_less<std::weak_ptr<Session>>> sessions_;
};