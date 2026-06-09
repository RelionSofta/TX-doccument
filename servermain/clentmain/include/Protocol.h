#pragma once

#include <cstdint>
#include <vector>
#include <string>
#include <cstring>

// ============================================================
//  Protocol.h
//  Wire-level message types and the Message helper class.
//
//  Layout on the wire (big-endian):
//    [ uint16_t type ][ uint16_t size ][ body... ]
//
//  Note: the struct field order is (type, size) to match the
//  existing server — do NOT reorder.
// ============================================================

#pragma pack(push, 1)

struct ProtocolHeader
{
    uint16_t type = 0;   // prtocol_type cast to uint16_t
    uint32_t size = 0;   // body length in bytes — must be uint32_t to match server
};

#pragma pack(pop)

// ----------------------------------------------------------------
// Message type enumeration
// (spelling "prtocol_type" preserved to stay compatible with
//  the existing server codebase)
// ----------------------------------------------------------------
enum class prtocol_type : uint16_t
{
    TextMessage = 1,

    FileStart = 2,
    FileChunk = 3,
    FileEnd = 4,

    CreateRoom = 10,
    JoinRoom = 11,
    RoomCode = 12,
    Error = 13
};

// ----------------------------------------------------------------
// Message — a header + variable-length byte body
// Helper methods for common payload patterns (string, raw blob)
// ----------------------------------------------------------------
struct Message
{
    ProtocolHeader        header_;
    std::vector<uint8_t>  body_;

    // ---- string payload ----

    void setString(const std::string& s)
    {
        body_.assign(s.begin(), s.end());
        header_.size = static_cast<uint32_t>(body_.size());
    }

    std::string getString() const
    {
        return { reinterpret_cast<const char*>(body_.data()),
                 body_.size() };
    }

    // ---- raw blob payload ----

    void setBlob(const uint8_t* data, std::size_t len)
    {
        body_.assign(data, data + len);
        header_.size = static_cast<uint32_t>(body_.size());
    }

    // ---- factory helpers ----

    static Message make(prtocol_type type)
    {
        Message m;
        m.header_.type = static_cast<uint16_t>(type);
        m.header_.size = 0;
        return m;
    }

    static Message makeText(const std::string& text)
    {
        Message m = make(prtocol_type::TextMessage);
        m.setString(text);
        return m;
    }

    static Message makeJoinRoom(const std::string& code)
    {
        Message m = make(prtocol_type::JoinRoom);
        m.setString(code);
        return m;
    }

    // FileStart carries the original filename so the receiver
    // can save it under the correct name in Downloads.
    static Message makeFileStart(const std::string& filename)
    {
        Message m = make(prtocol_type::FileStart);
        m.setString(filename);
        return m;
    }
};