#include "RoomHandler.h"
#include <cstdlib>
#include <ctime>

std::string RoomHandler::generateCode()
{
    static const char chars[] =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";

    std::string code;
    code.reserve(6);
    for (int i = 0; i < 6; ++i)
        code += chars[rand() % (sizeof(chars) - 1)];
    return code;
}

std::pair<std::string, std::shared_ptr<Room>>
RoomHandler::createRoom()
{
    std::lock_guard<std::mutex> lk(mutex_);

    std::string code;
    do {
        code = generateCode();
    } while (rooms_.find(code) != rooms_.end());

    auto room = std::make_shared<Room>(code, weak_from_this());
    rooms_[code] = room;
    return { code, room };
}

std::shared_ptr<Room>
RoomHandler::getRoom(const std::string& code)
{
    std::lock_guard<std::mutex> lk(mutex_);
    auto it = rooms_.find(code);
    if (it != rooms_.end())
        return it->second;
    return nullptr;
}

void RoomHandler::removeRoom(const std::string& code)
{
    // FIXED: trylock prevent deadlock agar same thread se aaye
    // (e.g. createRoom holds lock → Room constructor → impossible
    //  but leave() → removeRoom() is cross-thread safe now
    //  because Room::leave() releases its own mutex first)
    std::lock_guard<std::mutex> lk(mutex_);
    rooms_.erase(code);
}

std::size_t RoomHandler::roomCount() const
{
    std::lock_guard<std::mutex> lk(mutex_);
    return rooms_.size();
}