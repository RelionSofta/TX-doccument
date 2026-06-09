#include "Room.h"
#include "Session.h"
#include "RoomHandler.h"

Room::Room(std::string code, std::weak_ptr<RoomHandler> handler)
    : code_(std::move(code))
    , handler_(std::move(handler))
{
}

void Room::join(std::shared_ptr<Session> session)
{
    std::lock_guard<std::mutex> lk(mutex_);
    sessions_.insert(session);
}

void Room::leave(std::shared_ptr<Session> session)
{
    bool isEmpty = false;
    {
        std::lock_guard<std::mutex> lk(mutex_);
        sessions_.erase(session);

        for (auto it = sessions_.begin();
            it != sessions_.end(); )
        {
            if (it->expired()) it = sessions_.erase(it);
            else               ++it;
        }
        isEmpty = sessions_.empty();
    }
    // mutex release karne ke BAAD removeRoom — deadlock se bachao
    if (isEmpty)
        if (auto h = handler_.lock())
            h->removeRoom(code_);
}

// ── Normal broadcast — control messages + chat ───────────────
// session->send() use karta hai jo asio::post() karta hai.
// Safe hai kyunki ye messages bahut kam aate hain.
void Room::broadcast(const Message& msg,
    std::shared_ptr<Session> sender)
{
    std::lock_guard<std::mutex> lk(mutex_);
    for (auto it = sessions_.begin();
        it != sessions_.end(); )
    {
        if (auto session = it->lock())
        {
            if (session != sender)
                session->send(msg);   // asio::post path
            ++it;
        }
        else
        {
            it = sessions_.erase(it);
        }
    }
}

// ── Direct broadcast — FILE CHUNKS KE LIYE ONLY ─────────────
//
// BUG 2 FIX:
//   Normal broadcast → send() → asio::post(strand_, ...) matlab
//   har chunk schedule hone ke liye ek async round-trip wait
//   karta tha. Pipeline ka fayda nahi hota tha.
//
//   broadcastDirect() Session ka pushToQueue() call karta hai
//   jo directly writeQueue_ mein push karta hai — koi post nahi.
//
//   IMPORTANT: Yeh SIRF tab call karo jab tum sender ke
//   strand_ pe ho (jo Session::pushOneChunk() mein hota hai).
//   Agar alag thread se call karo toh data race hoga.
void Room::broadcastDirect(const Message& msg,
    std::shared_ptr<Session> sender)
{
    std::lock_guard<std::mutex> lk(mutex_);
    for (auto it = sessions_.begin();
        it != sessions_.end(); )
    {
        if (auto session = it->lock())
        {
            if (session != sender)
                session->pushToQueue(msg); // direct, no post
            ++it;
        }
        else
        {
            it = sessions_.erase(it);
        }
    }
}

bool Room::empty() const
{
    std::lock_guard<std::mutex> lk(mutex_);
    for (auto& wp : sessions_)
        if (!wp.expired()) return false;
    return true;
}

std::size_t Room::size() const
{
    std::lock_guard<std::mutex> lk(mutex_);
    std::size_t n = 0;
    for (auto& wp : sessions_)
        if (!wp.expired()) ++n;
    return n;
}