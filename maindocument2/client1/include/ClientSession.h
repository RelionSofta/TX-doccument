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
//  ClientSession.h
// ============================================================

using MessageCallback = std::function<void(const Message&)>;
using DisconnectCallback = std::function<void()>;

// Server ke SESSION_CHUNK_SIZE se match karna zaroori hai
// Server protocol.h mein 4MB hai — dono same hone chahiye
static constexpr std::size_t SESSION_CHUNK_SIZE = 4 * 1024 * 1024; // 4 MiB

class ClientSession
    : public std::enable_shared_from_this<ClientSession>
{
public:
    explicit ClientSession(asio::ip::tcp::socket socket);

    void start();

    void send(const Message& msg);
    void sendChat(const Message& msg);
    void disconnect();
    bool connected() const;

    void setMessageCallback(MessageCallback cb);
    void setDisconnectCallback(DisconnectCallback cb);

    void createRoom();
    void joinRoom(const std::string& code);
    void sendText(const std::string& text);

    void sendFile(
        const std::string& path,
        std::function<void(uint64_t, uint64_t)> progressCallback = {});

private:
    void readHeader();
    void readBody();
    void handleMessage();
    void write();
    void sendNextFileChunk();
    void fail(const std::string& where, const asio::error_code& ec);

    // ---- Members ----
    asio::ip::tcp::socket               socket_;
    asio::strand<asio::any_io_executor> strand_;
    std::atomic<bool>                   connected_{ true };

    Message             readMsg_;
    std::deque<Message> chatQueue_;
    std::deque<Message> fileQueue_;

    MessageCallback    messageCallback_;
    DisconnectCallback disconnectCallback_;

    FileTransferManager fileReceiver_;
    std::string         receivedFileName_;

    static std::filesystem::path getDownloadsFolder();
    static std::filesystem::path makeUniqueFilePath(
        const std::filesystem::path& dir,
        const std::string& filename);

    // File send
    std::ifstream sendingFile_;

    // 4MB buffer — server ek baar mein max 4MB bhejta hai
    std::array<char, SESSION_CHUNK_SIZE> fileBuffer_{};

    bool sendingFileData_ = false;

    // Pipeline depth 3 — [Write][Ready][Ready]
    // 3x4MB = 12MB max in-flight — network hamesha busy
    static constexpr int PIPELINE_DEPTH = 3;
    int pipelineInFlight_ = 0;

    std::atomic<uint64_t> atomicBytesSent_{ 0 };
    std::atomic<uint64_t> atomicBytesTotal_{ 0 };
    uint64_t fileBytesTotal_ = 0;
    uint64_t fileBytesSent_ = 0;
    std::function<void(uint64_t, uint64_t)> fileProgressCb_;
};