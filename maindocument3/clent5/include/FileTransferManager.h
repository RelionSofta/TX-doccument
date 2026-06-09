#pragma once
#include <cstddef>   // <-- yeh ZAROOR chahiye std::size_t ke liye
#include <string>
#include <fstream>
#include <array>
#include <filesystem>
#include <cstdint>
#include "protocol.h"

// CHUNK_SIZE = 512KB — protocol.h se aata hai
// Server aur client dono same size use karte hain

class FileTransferManager
{
public:
    FileTransferManager() = default;
    ~FileTransferManager() { close(); }

    FileTransferManager(const FileTransferManager&) = delete;
    FileTransferManager& operator=(const FileTransferManager&) = delete;

    bool        openForSend(const std::string& path);
    std::size_t readChunk();
    const char* chunkData() const { return fileBuffer_.data(); }

    bool openForReceive(const std::string& path);
    void writeChunk(const char* data, std::size_t size);

    void close();

    uint64_t    fileSize() const { return fileSize_; }
    uint64_t    bytesRead() const { return bytesRead_; }
    uint64_t    bytesWritten() const { return bytesWritten_; }
    std::string filename() const { return filename_; }

    bool isSendOpen() const { return sendFile_.is_open(); }
    bool isReceiveOpen() const { return recvFile_.is_open(); }

private:
    std::ifstream sendFile_;
    std::ofstream recvFile_;
    // Yeh hona chahiye:
    
    // 512KB buffer — protocol.h se aata 
    std::array<char, CHUNK_SIZE> fileBuffer_{};

    std::string filename_;
    uint64_t    fileSize_ = 0;
    uint64_t    bytesRead_ = 0;
    uint64_t    bytesWritten_ = 0;
};