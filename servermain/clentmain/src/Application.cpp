#include "Application.h"

#include "Window.h"
#include "Renderer.h"
#include "UIManager.h"
#include "ChatView.h"
#include "Client.h"
#include "Protocol.h"

#include <SDL3/SDL.h>
#include <imgui.h>
#include <iostream>
#include <chrono>
#include <thread>

// NFD
#include <nfd.hpp>

// ============================================================
//  Application.cpp
// ============================================================

Application::Application() = default;
Application::~Application() { shutdownSystems(); }

// ---- Entry point --------------------------------------------

int Application::run()
{
    if (!initSystems()) return 1;
    registerCallbacks();

    network_->start();
    network_->connect(serverHost_, serverPort_);

    // ---- Main loop ----
    using clock = std::chrono::steady_clock;
    constexpr auto TARGET_FRAME =
        std::chrono::duration<double>(1.0 / 60.0); // 60 FPS target

    while (!window_->shouldClose())
    {
        auto frameStart = clock::now();

        processEvents();
        drainCallbacks();
        renderFrame();

        // Frame cap — prevent busy loop draining CPU from IO thread
        auto elapsed = clock::now() - frameStart;
        if (elapsed < TARGET_FRAME)
        {
            std::this_thread::sleep_for(TARGET_FRAME - elapsed);
        }
    }

    network_->stop();
    shutdownSystems();
    return 0;
}

// ---- Init ---------------------------------------------------

bool Application::initSystems()
{
    // NFD
    if (NFD::Init() != NFD_OKAY)
    {
        std::cerr << "[App] NFD_Init failed\n";
        // Non-fatal — file picking will be unavailable
    }

    // Window
    WindowConfig wcfg;
    wcfg.title = "TX Doc";
    wcfg.width = 1280;
    wcfg.height = 720;

    window_ = std::make_unique<Window>(wcfg);
    if (!window_->init())
    {
        std::cerr << "[App] Window init failed\n";
        return false;
    }

    // Renderer (ImGui backends)
    renderer_ = std::make_unique<Renderer>(*window_);
    if (!renderer_->init())
    {
        std::cerr << "[App] Renderer init failed\n";
        return false;
    }

    // UI
    uiManager_ = std::make_unique<UIManager>(state_);


    // Network
    network_ = std::make_shared<NetworkClient>(state_);

    return true;
}

void Application::registerCallbacks()
{
    // ---- UIManager → Application ----
    uiManager_->setCreateRoomCallback([this]()
        {
            onCreateRoom();
        });

    uiManager_->setJoinRoomCallback([this](const std::string& code)
        {
            onJoinRoom(code);
        });

    uiManager_->setSendMessageCallback([this](const std::string& text)
        {
            onSendMessage(text);
        });

    uiManager_->setSendFileCallback([this](const std::string& path)
        {
            onSendFile(path);
        });

    // Browse (NFD) and Disconnect wired through UIManager passthrough setters
    uiManager_->setBrowseFileCallback([this]()
        {
            onBrowseFile();
        });

    uiManager_->setDisconnectUICallback([this]()
        {
            onDisconnect();
        });

    // ---- Network → Application (cross-thread) ----
    network_->setMessageCallback([this](const Message& msg)
        {
            onNetworkMessage(msg);
        });

    network_->setDisconnectCallback([this]()
        {
            onNetworkDisconnect();
        });
}

void Application::shutdownSystems()
{
    if (network_)
    {
        network_->stop();
        network_.reset();
    }
    renderer_.reset();
    window_.reset();
    NFD::Quit();
}

// ---- Main loop helpers --------------------------------------

void Application::processEvents()
{
    SDL_Event event;
    while (SDL_PollEvent(&event))
    {
        renderer_->processEvent(event);
        window_->handleEvent(event);
    }
}

void Application::drainCallbacks()
{
    // Move pending callbacks out under lock, then run them
    // without holding the lock (callbacks may re-enter post).
    std::vector<std::function<void()>> toRun;
    {
        std::lock_guard<std::mutex> lk(callbackMutex_);
        toRun = std::move(pendingCallbacks_);
        pendingCallbacks_.clear();
    }
    for (auto& cb : toRun) cb();
}

void Application::renderFrame()
{
    renderer_->beginFrame();
    uiManager_->render();
    renderer_->endFrame();
}

// ---- Application-level callback implementations -------------

void Application::onCreateRoom()
{
    // Main thread pe hai — safe
    state_.pushSystemMessage("Creating room...");
    state_.screen = AppScreen::Chat;
    network_->createRoom();
    scrollChatToBottom_ = true;
}

void Application::onJoinRoom(const std::string& code)
{
    state_.roomCode = code;   // show immediately in header
    state_.connected = true;   // optimistic — server will confirm
    state_.pushSystemMessage("Joining room " + code + "...");
    state_.screen = AppScreen::Chat;
    network_->joinRoom(code);
}

void Application::onSendMessage(const std::string& text)
{
    // Local echo already added by UIManager's wrapper;
    // forward to network.
    network_->sendMessage(text);
}

void Application::onSendFile(const std::string& path)
{
    state_.fileTransfer.filename = path;
    state_.pushSystemMessage("Sending file: " + path);
    network_->sendFile(path);
}

void Application::onBrowseFile()
{
    // NFD must be called from the main thread
    NFD::UniquePath outPath;
    nfdfilteritem_t filters[] = {
        { "All Files",  "*" },
        { "Images",     "png,jpg,jpeg,gif,bmp,webp" },
        { "Videos",     "mp4,mkv,avi,mov,webm" },
        { "Documents",  "pdf,doc,docx,txt,xlsx,pptx" },
        { "Archives",   "zip,rar,7z,tar,gz" }
    };

    nfdresult_t result = NFD::OpenDialog(
        outPath,
        filters,
        5,      // filter count
        nullptr // default path
    );

    if (result == NFD_OKAY)
    {
        state_.selectedFilePath = outPath.get();
    }
    else if (result == NFD_CANCEL)
    {
        // User cancelled — no action
    }
    else
    {
        std::cerr << "[App] NFD error: " << NFD::GetError() << '\n';
    }
}

void Application::onDisconnect()
{
    network_->disconnect();

    state_.screen = AppScreen::MainMenu;
    state_.connected = false;
    state_.roomCode.clear();
    state_.chatHistory.clear();
    state_.selectedFilePath.clear();
    state_.fileTransfer.reset();

    // Reconnect to server so Create/Join Room works again
    network_->connect(serverHost_, serverPort_);
}

// ---- Network callbacks (worker thread) ----------------------

void Application::postCallback(std::function<void()> cb)
{
    std::lock_guard<std::mutex> lk(callbackMutex_);
    pendingCallbacks_.push_back(std::move(cb));
}

void Application::onNetworkMessage(const Message& msg)
{
    // Called on the asio worker thread — copy msg by value
    // and post to main thread.
    postCallback([this, msg]()
        {
            auto type = static_cast<prtocol_type>(msg.header_.type);

            switch (type)
            {
            case prtocol_type::RoomCode:
            {
                std::string code = msg.getString();
                bool wasCreating = state_.roomCode.empty();
                state_.roomCode = code;
                state_.connected = true;
                // "Creating room" state se aaye toh creator, else joiner
                if (wasCreating)
                    state_.pushSystemMessage("Room created. Share code: " + code);
                else
                    state_.pushSystemMessage("Joined room: " + code);
                break;
            }

            case prtocol_type::TextMessage:
            {
                state_.pushRemoteMessage("Peer", msg.getString());
                break;
            }

            case prtocol_type::FileStart:
            {
                state_.fileTransfer.active = true;
                state_.fileTransfer.sending = false;
                state_.fileTransfer.bytesDone = 0;
                state_.pushSystemMessage("Peer is sending a file...");
                break;
            }

            case prtocol_type::FileEnd:
            {
                state_.fileTransfer.active = false;
                state_.pushSystemMessage("File saved to Downloads folder.");
                break;
            }

            case prtocol_type::Error:
            {
                state_.pushSystemMessage(
                    "Server error: " + msg.getString());
                break;
            }

            default:
                break;
            }
        });
}

void Application::onNetworkDisconnect()
{
    postCallback([this]()
        {
            state_.connected = false;
            state_.screen = AppScreen::MainMenu;
            state_.roomCode.clear();
            state_.fileTransfer.reset();
            state_.pushSystemMessage("Disconnected from server.");

            // Auto-reconnect so Create/Join Room works immediately
            network_->connect(serverHost_, serverPort_);
        });
}