#pragma once

#include <string>
#include <fstream>
#include <array>
#include <filesystem>
#include <functional>
#include <cstdint>

// ============================================================
//  FileTransferManager.h
//
//  CHUNK_SIZE server ke protocol.h se match karna ZAROORI hai.
//  Server 4MB chunks bhejta hai — agar client 1MB expect kare
//  toh incomplete chunks write honge aur file corrupt hogi.
// ============================================================

// Server ke protocol.h mein bhi 4MB hai — dono same hone chahiye
static constexpr std::size_t CHUNK_SIZE = 4 * 1024 * 1024; // 4 MiB

class FileTransferManager
{
public:
    FileTransferManager() = default;
    ~FileTransferManager() { close(); }

    FileTransferManager(const FileTransferManager&) = delete;
    FileTransferManager& operator=(const FileTransferManager&) = delete;

    // ---- Send side ----
    bool        openForSend(const std::string& path);
    std::size_t readChunk();
    const char* chunkData() const { return fileBuffer_.data(); }

    // ---- Receive side ----
    bool openForReceive(const std::string& path);
    void writeChunk(const char* data, std::size_t size);

    // ---- Common ----
    void close();

    uint64_t    fileSize()     const { return fileSize_; }
    uint64_t    bytesRead()    const { return bytesRead_; }
    uint64_t    bytesWritten() const { return bytesWritten_; }
    std::string filename()     const { return filename_; }

    bool isSendOpen()    const { return sendFile_.is_open(); }
    bool isReceiveOpen() const { return recvFile_.is_open(); }

private:
    std::ifstream sendFile_;
    std::ofstream recvFile_;

    // 4MB buffer — server ke ek chunk ke barabar
    std::array<char, CHUNK_SIZE> fileBuffer_{};

    std::string filename_;
    uint64_t    fileSize_ = 0;
    uint64_t    bytesRead_ = 0;
    uint64_t    bytesWritten_ = 0;
};