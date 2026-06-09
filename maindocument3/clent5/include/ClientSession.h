#pragma once
#include <asio.hpp>
#include <deque>
#include <fstream>
#include <vector>
#include <functional>
#include <atomic>
#include <filesystem>
#include "Protocol.h"
#include "FileTransferManager.h"

using MessageCallback = std::function<void(const Message&)>;
using DisconnectCallback = std::function<void()>;

// 512KB — protocol.h ke CHUNK_SIZE se match
// Stack overflow nahi hoga kyunki fileBuffer_ vector hai
static constexpr std::size_t SESSION_CHUNK_SIZE = 512 * 1024;

class ClientSession : public std::enable_shared_from_this<ClientSession>
{
public:
    explicit ClientSession(asio::ip::tcp::socket socket);

    void start();
    void send(const Message& msg);
    void sendChat(const Message& msg);
    void disconnect();
    bool connected() const;

    void setMessageCallback(MessageCallback    cb);
    void setDisconnectCallback(DisconnectCallback cb);

    void createRoom();
    void joinRoom(const std::string& code);
    void sendText(const std::string& text);
    void sendFile(const std::string& path,
        std::function<void(uint64_t, uint64_t)> progressCb = {});

private:
    void readHeader();
    void readBody();
    void handleMessage();
    void write();
    void sendNextFileChunk();
    void fail(const std::string& where, const asio::error_code& ec);

    static std::filesystem::path getDownloadsFolder();
    static std::filesystem::path makeUniqueFilePath(
        const std::filesystem::path& dir,
        const std::string& filename);

    // Transport
    asio::ip::tcp::socket               socket_;
    asio::strand<asio::any_io_executor> strand_;
    std::atomic<bool>                   connected_{ true };

    // Read
    Message readMsg_;

    // Write — two priority queues
    std::deque<Message> chatQueue_; // high: control + chat
    std::deque<Message> fileQueue_; // low:  file chunks

    // Callbacks
    MessageCallback    messageCallback_;
    DisconnectCallback disconnectCallback_;

    // Receive side
    FileTransferManager fileReceiver_;
    std::string         receivedFileName_;

    // Send side
    std::ifstream     sendingFile_;
    std::vector<char> fileBuffer_; // heap — 512KB, sized in ctor

    bool sendingFileData_ = false;
    int  pipelineInFlight_ = 0;

    // Pipeline: 3 chunks in-flight — [Write][Ready][Ready]
    // 3 x 512KB = 1.5MB max in-flight
    static constexpr int PIPELINE_DEPTH = 3;

    uint64_t fileBytesTotal_ = 0;
    uint64_t fileBytesSent_ = 0;
    std::function<void(uint64_t, uint64_t)> fileProgressCb_;
};