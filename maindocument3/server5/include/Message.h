#pragma once
#include "protocol.h"
#include <vector>
#include <cstdint>
#include <cstring>
#include <type_traits>
class Message
{
private:
    
    PacketHeader header_{};
    std::vector<uint8_t> body_;

public:
    size_t size() const;

    PacketHeader& header();
    const PacketHeader& header()const;

    std::vector<uint8_t>& body();
    const  std::vector<uint8_t>& body() const;

    // Push data inside body
    template<typename T>
    void push(const T& data);

    // Pop data from body
    template<typename T>
    void pop(T& data);
};