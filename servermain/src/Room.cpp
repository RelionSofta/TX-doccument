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

        // Expired weak_ptrs clean karo
        for (auto it = sessions_.begin(); it != sessions_.end(); )
        {
            if (it->expired())
                it = sessions_.erase(it);
            else
                ++it;
        }

        isEmpty = sessions_.empty();
    }
    // IMPORTANT: mutex release karne ke BAAD removeRoom call karo
    // warna RoomHandler ka mutex + Room ka mutex = deadlock
    if (isEmpty)
    {
        if (auto handler = handler_.lock())
            handler->removeRoom(code_);
    }
}

void Room::broadcast(const Message& msg,
    std::shared_ptr<Session> sender)
{
    std::lock_guard<std::mutex> lk(mutex_);
    for (auto it = sessions_.begin(); it != sessions_.end(); )
    {
        if (auto session = it->lock())
        {
            if (session != sender)
                session->send(msg);
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
    std::size_t count = 0;
    for (auto& wp : sessions_)
        if (!wp.expired()) ++count;
    return count;
}