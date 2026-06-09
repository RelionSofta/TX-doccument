#pragma once
#include<cstdint>
#include <cstddef>
#pragma pack(push, 1)
struct PacketHeader
{
    uint16_t type;      // MessageType
    uint32_t size;      // Payload size
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
inline constexpr std::size_t CHUNK_SIZE = 4 * 1024 * 1024; // 4 MiB