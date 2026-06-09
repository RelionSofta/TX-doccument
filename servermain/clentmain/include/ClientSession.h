#pragma once

#include <asio.hpp>
#include <deque>
#include <fstream>
#include <array>
#include <functional>
#include <atomic>
#include <filesystem>

#include "protocol.h"
#include "FileTransferManager.h"

// ============================================================
//  ClientSession.h  (network/ClientSession.h)
//
//  A ClientSession is created the moment a TCP connection is
//  established.  It:
//    - Owns the connected socket
//    - Runs a continuous read loop (readHeader → readBody)
//    - Queues outbound messages and writes them serially
//    - Handles file-send chunking via sendNextFileChunk()
//
//  All methods that touch socket/queues must run on the
//  session's strand_.  External callers use send() which
//  posts to the strand automatically.
// ============================================================

using MessageCallback = std::function<void(const Message&)>;
using DisconnectCallback = std::function<void()>;

static constexpr std::size_t SESSION_CHUNK_SIZE = 2 * 1024 * 1024; // 2 MiB

class ClientSession
    : public std::enable_shared_from_this<ClientSession>
{
public:
    explicit ClientSession(asio::ip::tcp::socket socket);

    // Kick off the read loop — call once after construction.
    void start();

    // Thread-safe: posts to strand
    void send(const Message& msg);          // file chunks
    void sendChat(const Message& msg);       // high-priority chat

    // Thread-safe: posts to strand
    void disconnect();

    bool connected() const;

    // ---- Callbacks ----
    void setMessageCallback(MessageCallback cb);
    void setDisconnectCallback(DisconnectCallback cb);

    // ---- High-level outbound helpers ----
    void createRoom();
    void joinRoom(const std::string& code);
    void sendText(const std::string& text);

    // Initiates a streaming file send.
    // progressCallback is called on each chunk with
    // (bytesWritten, totalBytes).
    void sendFile(
        const std::string& path,
        std::function<void(uint64_t, uint64_t)> progressCallback = {});

private:
    // ---- Read pipeline ----
    void readHeader();
    void readBody();
    void handleMessage();

    // ---- Write pipeline ----
    void write();

    // ---- File chunking ----
    void sendNextFileChunk();

    // ---- Error handling ----
    void fail(const std::string& where,
        const asio::error_code& ec);

    // ---- Members ----
    asio::ip::tcp::socket socket_;
    asio::strand<asio::any_io_executor> strand_;

    std::atomic<bool>  connected_{ true };

    Message            readMsg_;
    std::deque<Message> chatQueue_;   // high-priority: text messages
    std::deque<Message> fileQueue_;   // low-priority:  file chunks

    MessageCallback    messageCallback_;
    DisconnectCallback disconnectCallback_;

    // File receive — delegated to FileTransferManager
    FileTransferManager     fileReceiver_;
    std::string             receivedFileName_;  // original name from peer

    // ---- Download path helpers (static) ----
    static std::filesystem::path getDownloadsFolder();
    static std::filesystem::path makeUniqueFilePath(
        const std::filesystem::path& dir,
        const std::string& filename);

    // File send
    std::ifstream  sendingFile_;
    std::array<char, SESSION_CHUNK_SIZE> fileBuffer_{};
    bool           sendingFileData_ = false;

    // Pipeline — multiple chunks in-flight ek saath
    static constexpr int PIPELINE_DEPTH = 2;  // 2x2MB = 4MB max
    int  pipelineInFlight_ = 0;

    // Atomic progress — IO thread write, UI thread read safely
    std::atomic<uint64_t> atomicBytesSent_{ 0 };
    std::atomic<uint64_t> atomicBytesTotal_{ 0 };
    uint64_t       fileBytesTotal_ = 0;
    uint64_t       fileBytesSent_ = 0;
    std::function<void(uint64_t, uint64_t)> fileProgressCb_;
};