#pragma once

#include <string>
#include <fstream>
#include <array>
#include <filesystem>
#include <functional>
#include <cstdint>

// ============================================================
//  FileTransferManager.h  (filetransfer/)
//
//  Wraps low-level file I/O for both sending and receiving.
//  The networking layer calls these methods; all file handles
//  live here so the rest of the codebase never touches fstreams.
//
//  Responsibilities:
//    - Open files for reading (send) or writing (receive)
//    - Return typed chunk spans to the networking layer
//    - Track byte counts for progress reporting
//    - Query file metadata (size, name)
// ============================================================

static constexpr std::size_t CHUNK_SIZE = 1 * 1024 * 1024; // 1 MiB

class FileTransferManager
{
public:
    FileTransferManager() = default;
    ~FileTransferManager() { close(); }

    FileTransferManager(const FileTransferManager&) = delete;
    FileTransferManager& operator=(const FileTransferManager&) = delete;

    // ---- Send side ----

    // Opens a file for reading.  Returns false on failure.
    bool openForSend(const std::string& path);

    // Reads up to CHUNK_SIZE bytes into the internal buffer.
    // Returns the number of bytes read (0 = EOF).
    std::size_t readChunk();

    // Pointer to the last readChunk() data
    const char* chunkData()  const { return fileBuffer_.data(); }

    // ---- Receive side ----

    // Opens a file for writing.  Creates parent dirs if needed.
    bool openForReceive(const std::string& path);

    // Appends raw bytes to the receive file.
    void writeChunk(const char* data, std::size_t size);

    // ---- Common ----

    void close();

    uint64_t fileSize()    const { return fileSize_; }
    uint64_t bytesRead()   const { return bytesRead_; }
    uint64_t bytesWritten()const { return bytesWritten_; }

    std::string filename() const { return filename_; }

    bool isSendOpen()    const { return sendFile_.is_open(); }
    bool isReceiveOpen() const { return recvFile_.is_open(); }

private:
    std::ifstream sendFile_;
    std::ofstream recvFile_;

    std::array<char, CHUNK_SIZE> fileBuffer_{};

    std::string filename_;
    uint64_t    fileSize_ = 0;
    uint64_t    bytesRead_ = 0;
    uint64_t    bytesWritten_ = 0;
};