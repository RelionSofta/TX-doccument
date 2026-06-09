#pragma once
#include <cstdint>
#include <cstddef>

#pragma pack(push, 1)
struct PacketHeader
{
    uint16_t type;
    uint32_t size;
};
#pragma pack(pop)

enum class MessageType : uint16_t
{
    TextMessage = 1,
    FileStart = 2,
    FileChunk = 3,
    FileEnd = 4,
    CreateRoom = 10,
    JoinRoom = 11,
    RoomCode = 12,
    Error = 13,
};

// Single source of truth — server aur client dono yahi use karte hain
inline constexpr std::size_t CHUNK_SIZE = 512 * 1024; // 512 KB