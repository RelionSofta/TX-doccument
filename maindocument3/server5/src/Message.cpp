#include "Message.h"
#include"protocol.h"


size_t Message::size() const
{
    return sizeof(PacketHeader) + body_.size();
}
PacketHeader& Message::header() { return header_; }
const PacketHeader& Message::header() const { return header_; }

std::vector<uint8_t>& Message::body() { return body_; }
const  std::vector<uint8_t>& Message::body() const { return body_; }


template<typename T>
void Message::push(const T& data)
{
    static_assert(std::is_standard_layout_v<T>, "T must be POD/standard layout");
    size_t i = body_.size();
    body_.resize(body_.size() + sizeof(T));
    std::memcpy(body_.data() + i, &data, sizeof(T));
    header_.size = body_.size();
}

template<typename T>
void Message::pop(T& data)
{
    static_assert(std::is_standard_layout_v<T>,
        "T must be POD/standard layout");
    size_t i = body_.size() - sizeof(T);
    std::memcpy(&data, body_.data() + i, sizeof(T));
    body_.resize(i);
    header_.size = body_.size();
}
