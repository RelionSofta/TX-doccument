#include "Client.h"
#include "ClientSession.h"

#include <iostream>
#include <thread>

// ============================================================
//  Client.cpp  (NetworkClient)
//
//  Drives asio::io_context on a dedicated thread.
//  Lifecycle: start() → connect() → {createRoom|joinRoom|...}
//            → disconnect() → stop()
// ============================================================

NetworkClient::NetworkClient(AppState& state)
    : state_(state)
    , work_(asio::make_work_guard(io_))
    , resolver_(io_)
    , socket_(io_)
    , strand_(asio::make_strand(io_))
{
}

NetworkClient::~NetworkClient()
{
    stop();
}

// ---- Lifecycle ----------------------------------------------

void NetworkClient::start()
{
    ioThread_ = std::thread([this]()
        {
            io_.run();
        });
}

void NetworkClient::stop()
{
    // 1. Disconnect session cleanly — cancels pending async_read/write
    if (session_)
        session_->disconnect();

    // 2. Release work guard — io_.run() can now exit
    work_.reset();

    // 3. Close socket — unblocks any pending async ops immediately
    closeSocket();

    // 4. Force io_ to stop — no more handlers will run
    io_.stop();

    // 5. Wait for worker thread to exit
    if (ioThread_.joinable())
        ioThread_.join();
}

// ---- Connection ---------------------------------------------

void NetworkClient::connect(
    const std::string& host,
    const std::string& port)
{
    auto self = shared_from_this();

    asio::post(strand_, [this, self, host, port]()
        {
            if (connected_.load()) return;  // already connected
            doResolve(host, port);
        });
}

void NetworkClient::disconnect()
{
    if (session_)
        session_->disconnect();
    else
        closeSocket();
}

bool NetworkClient::isConnected() const
{
    return connected_.load();
}

// ---- High-level API -----------------------------------------

void NetworkClient::createRoom()
{
    if (session_) session_->createRoom();
}

void NetworkClient::joinRoom(const std::string& roomCode)
{
    if (session_) session_->joinRoom(roomCode);
}

void NetworkClient::sendMessage(const std::string& text)
{
    if (session_) session_->sendText(text);
}

void NetworkClient::sendFile(const std::string& path)
{
    if (!session_) return;

    // Set initial state on main thread (safe)
    state_.fileTransfer.active = true;
    state_.fileTransfer.sending = true;
    state_.fileTransfer.bytesDone = 0;

    session_->sendFile(path,
        [this](uint64_t done, uint64_t total)
        {
            // IO thread se atomic write — UI thread safely reads
            // Use postCallback via stored ref to avoid race
            // Simple atomic stores are safe for uint64_t on x64
            state_.fileTransfer.bytesDone = done;
            state_.fileTransfer.bytesTotal = total;

            if (done >= total && total > 0)
                state_.fileTransfer.active = false;
        });
}

// ---- Callbacks ----------------------------------------------

void NetworkClient::setMessageCallback(MessageCallback cb)
{
    messageCallback_ = std::move(cb);
}

void NetworkClient::setDisconnectCallback(DisconnectCallback cb)
{
    disconnectCallback_ = std::move(cb);
}

// ---- Internal connection pipeline ---------------------------

void NetworkClient::doResolve(
    const std::string& host,
    const std::string& port)
{
    auto self = shared_from_this();

    resolver_.async_resolve(host, port,
        asio::bind_executor(strand_,
            [this, self](const asio::error_code& ec,
                asio::ip::tcp::resolver::results_type results)
            {
                if (ec) { fail("resolve", ec); return; }
                doConnect(results);
            }));
}

void NetworkClient::doConnect(
    const asio::ip::tcp::resolver::results_type& endpoints)
{
    auto self = shared_from_this();

    asio::async_connect(socket_, endpoints,
        asio::bind_executor(strand_,
            [this, self](const asio::error_code& ec,
                const asio::ip::tcp::endpoint&)
            {
                if (ec) { fail("connect", ec); return; }

                connected_ = true;
                std::cout << "[Network] Connected to server\n";

                // Hand the socket to a new session
                session_ = std::make_shared<ClientSession>(
                    std::move(socket_));

                // Forward callbacks into the session
                if (messageCallback_)
                    session_->setMessageCallback(messageCallback_);

                if (disconnectCallback_)
                {
                    session_->setDisconnectCallback(
                        [this]()
                        {
                            connected_ = false;
                            state_.connected = false;
                            if (disconnectCallback_)
                                disconnectCallback_();
                        });
                }

                session_->start();
            }));
}

void NetworkClient::fail(
    const std::string& where,
    const asio::error_code& ec)
{
    connected_ = false;
    session_.reset();   // release session so reconnect works
    std::cerr << "[Network] Error in " << where
        << ": " << ec.message() << '\n';
    closeSocket();
}

void NetworkClient::closeSocket()
{
    asio::error_code ec;
    resolver_.cancel();

    if (socket_.is_open())
    {
        socket_.shutdown(
            asio::ip::tcp::socket::shutdown_both, ec);
        socket_.close(ec);
    }

    connected_ = false;

    // Restart io_ so it can accept new async operations
    // after a disconnect — without this connect() does nothing
    if (io_.stopped())
        io_.restart();
}