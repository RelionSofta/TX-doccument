#pragma once

#include <asio.hpp>
#include <memory>
#include <string>
#include <functional>
#include <atomic>

#include "Protocol.h"
#include "AppState.h"

// Forward declarations
class ClientSession;

// ============================================================
//  Client.h  (network/Client.h)
//
//  NetworkClient owns an asio::io_context, a resolver, and
//  a ClientSession.  It drives the io_context on a dedicated
//  background thread so that all async I/O is off the render
//  thread.
//
//  Public API (thread-safe — can be called from any thread):
//    connect()
//    disconnect()
//    createRoom()
//    joinRoom()
//    sendMessage()
//    sendFile()
//
//  Callbacks are invoked on the io_context thread; the
//  Application posts them back to the main thread's
//  pending-callback queue (see Application.h).
// ============================================================

using MessageCallback    = std::function<void(const Message&)>;
using DisconnectCallback = std::function<void()>;

class NetworkClient
    : public std::enable_shared_from_this<NetworkClient>
{
public:
    // Inject a reference to AppState so the network layer can
    // update file-transfer progress directly (protected by
    // the strand; main thread reads are safe for UI display
    // purposes given the atomic updates).
    explicit NetworkClient(AppState& state);
    ~NetworkClient();

    NetworkClient(const NetworkClient&)            = delete;
    NetworkClient& operator=(const NetworkClient&) = delete;

    // ---- Lifecycle ----

    // Starts the io_context worker thread.
    void start();

    // Gracefully stops the worker thread and closes all
    // sockets.  Safe to call multiple times.
    void stop();

    // ---- Connection ----

    void connect(const std::string& host,
                 const std::string& port);

    void disconnect();

    bool isConnected() const;

    // ---- High-level API ----

    // Sends a CreateRoom protocol packet.
    void createRoom();

    // Sends a JoinRoom protocol packet with the given code.
    void joinRoom(const std::string& roomCode);

    // Sends a TextMessage packet.
    void sendMessage(const std::string& text);

    // Opens the file at `path` and streams it as
    // FileStart / FileChunk... / FileEnd packets.
    void sendFile(const std::string& path);

    // ---- Callback registration ----
    // Must be called before connect().
    void setMessageCallback(MessageCallback cb);
    void setDisconnectCallback(DisconnectCallback cb);

private:
    // Internal connection helpers (run on strand)
    void doResolve(const std::string& host,
                   const std::string& port);

    void doConnect(
        const asio::ip::tcp::resolver::results_type& endpoints);

    void fail(const std::string& where,
              const asio::error_code& ec);

    void closeSocket();

    // ---- Members ----
    AppState&  state_;

    asio::io_context               io_;
    asio::executor_work_guard<
        asio::io_context::executor_type> work_;
    asio::ip::tcp::resolver        resolver_;
    asio::ip::tcp::socket          socket_;
    asio::strand<asio::any_io_executor> strand_;

    std::shared_ptr<ClientSession> session_;

    std::atomic<bool>   connected_{ false };
    std::thread         ioThread_;

    MessageCallback     messageCallback_;
    DisconnectCallback  disconnectCallback_;
};
