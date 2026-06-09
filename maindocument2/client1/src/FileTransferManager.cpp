#include "FileTransferManager.h"
#include <iostream>

// ============================================================
//  FileTransferManager.cpp
//
//  Receive side: writeChunk() ab buffer mein likhta hai aur
//  sirf tab flush karta hai jab buffer full ho — disk I/O
//  calls kam hote hain, throughput badhta hai.
//
//  4MB chunk seedha disk pe likhna already fast hai kyunki
//  OS apna page cache use karta hai. Extra buffering ki
//  zaroorat nahi — simple aur correct rakho.
// ============================================================

bool FileTransferManager::openForSend(const std::string& path)
{
    close();

    sendFile_.open(path, std::ios::binary);
    if (!sendFile_.is_open())
    {
        std::cerr << "[FileTransfer] Cannot open for send: "
            << path << '\n';
        return false;
    }

    filename_ = std::filesystem::path(path).filename().string();
    bytesRead_ = 0;

    std::error_code ec;
    auto sz = std::filesystem::file_size(path, ec);
    fileSize_ = ec ? 0 : sz;

    return true;
}

std::size_t FileTransferManager::readChunk()
{
    if (!sendFile_.is_open()) return 0;

    sendFile_.read(fileBuffer_.data(),
        static_cast<std::streamsize>(CHUNK_SIZE));
    std::size_t n = static_cast<std::size_t>(sendFile_.gcount());
    bytesRead_ += n;
    return n;
}

bool FileTransferManager::openForReceive(const std::string& path)
{
    close();

    std::error_code ec;
    std::filesystem::path p(path);
    if (p.has_parent_path())
        std::filesystem::create_directories(p.parent_path(), ec);

    // ios::binary | ios::trunc — naya file, koi leftover nahi
    recvFile_.open(path, std::ios::binary | std::ios::trunc);
    if (!recvFile_.is_open())
    {
        std::cerr << "[FileTransfer] Cannot open for receive: "
            << path << '\n';
        return false;
    }

    filename_ = p.filename().string();
    bytesWritten_ = 0;
    fileSize_ = 0;

    return true;
}

void FileTransferManager::writeChunk(const char* data, std::size_t size)
{
    if (!recvFile_.is_open() || size == 0) return;

    // Direct write — OS page cache handle karta hai buffering
    // flush() mat karo har chunk pe — sirf close() pe hoga
    recvFile_.write(data, static_cast<std::streamsize>(size));
    bytesWritten_ += size;
}

void FileTransferManager::close()
{
    if (sendFile_.is_open())
    {
        sendFile_.close();
        bytesRead_ = 0;
    }

    if (recvFile_.is_open())
    {
        // Flush + close — sab kuch disk pe ja chuka hoga
        recvFile_.flush();
        recvFile_.close();
        bytesWritten_ = 0;
    }

    fileSize_ = 0;
    filename_.clear();
}