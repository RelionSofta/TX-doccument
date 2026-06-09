#pragma once

#include <memory>
#include <functional>
#include <mutex>
#include <vector>
#include <string>

#include "AppState.h"

// Forward declarations
class Window;
class Renderer;
class UIManager;
class NetworkClient;

// ============================================================
//  Application.h  (app/)
//
//  Application owns every subsystem and wires them together.
//
//  Responsibilities:
//    - Create Window, Renderer, UIManager, NetworkClient
//    - Run the main loop (run())
//    - Register UIManager callbacks and route them to the
//      network layer
//    - Receive network callbacks and marshal them back to the
//      main thread via a pending-callback queue
//    - Open the NFD file picker on Browse press
//
//  Architecture decision — callback threading:
//    Network callbacks fire on the asio worker thread.
//    ImGui can only be used from the render (main) thread.
//    Application maintains a mutex-guarded queue of
//    std::function<void()> that is drained each frame before
//    UIManager::render().
// ============================================================

class Application
{
public:
    Application();
    ~Application();

    Application(const Application&)            = delete;
    Application& operator=(const Application&) = delete;

    // Entry point — blocks until the window is closed.
    // Returns 0 on success, non-zero on startup failure.
    int run();

private:
    // ---- Init helpers ----
    bool initSystems();
    void registerCallbacks();
    void shutdownSystems();

    // ---- Main loop helpers ----
    void processEvents();
    void drainCallbacks();
    void renderFrame();

    // ---- Callback implementations ----
    //  These are called from the render thread, posted from
    //  the worker thread via pendingCallbacks_.
    void onCreateRoom();
    void onJoinRoom(const std::string& code);
    void onSendMessage(const std::string& text);
    void onSendFile(const std::string& path);
    void onBrowseFile();
    void onDisconnect();

    // Network → main thread marshal
    void postCallback(std::function<void()> cb);
    void onNetworkMessage(const struct Message& msg);
    void onNetworkDisconnect();

    // ---- Members ----
    AppState state_;

    std::unique_ptr<Window>         window_;
    std::unique_ptr<Renderer>       renderer_;
    std::unique_ptr<UIManager>      uiManager_;
    std::shared_ptr<NetworkClient>  network_;

    // Pending callbacks from network thread → render thread
    std::mutex                       callbackMutex_;
    std::vector<std::function<void()>> pendingCallbacks_;
    bool scrollChatToBottom_ = false;

    // Server connection config (could come from a config file)
    std::string serverHost_ = "187.127.140.121";
    std::string serverPort_ = "9000";
};
