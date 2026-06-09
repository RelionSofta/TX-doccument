#pragma once
#include <fstream>
#include <string>
#include <cstdint>
#include <filesystem>
#include <array>
#include "protocol.h"   // CHUNK_SIZE ek jagah se — protocol.h se aata hai

class FileHandler {
private:
    std::ifstream input_;
    std::ofstream output_;
    std::filesystem::path filePath{};

public:
    // CHUNK_SIZE protocol.h se inherit — dono jagah alag define nahi hoga
    static constexpr size_t CHUNK_SIZE = ::CHUNK_SIZE;

    FileHandler();
    ~FileHandler();

    bool openRead(const std::string& path);
    bool openWrite(const std::string& path);
    uint64_t fileSize(const std::string& path);

    std::size_t readChunk(std::array<char, CHUNK_SIZE>& buffer);
    void writeChunk(const char* data, std::size_t size);
    void close();
};