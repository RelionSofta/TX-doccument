#pragma once
#include <memory>
#include <string>
#include <unordered_map>
#include <mutex>
#include "Room.h"

class RoomHandler : public std::enable_shared_from_this<RoomHandler>
{
public:
    std::pair<std::string, std::shared_ptr<Room>> createRoom();
    std::shared_ptr<Room> getRoom(const std::string& code);
    void removeRoom(const std::string& code);   // called by Room::leave()

    // Debug — kitne rooms active hain
    std::size_t roomCount() const;

private:
    std::string generateCode();

    mutable std::mutex mutex_;
    std::unordered_map<std::string,
        std::shared_ptr<Room>> rooms_;
};