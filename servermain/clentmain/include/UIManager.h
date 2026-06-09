#pragma once

#include <memory>
#include <functional>

#include "AppState.h"

// Forward declarations — avoid header coupling
class MainMenuView;
class JoinRoomView;
class ChatView;

// Callbacks that UIManager fires back to Application
using CreateRoomCb  = std::function<void()>;
using JoinRoomCb    = std::function<void(const std::string& code)>;
using SendMessageCb = std::function<void(const std::string& text)>;
using SendFileCb    = std::function<void(const std::string& path)>;
using BrowseFileCb  = std::function<void()>;
using DisconnectUICb= std::function<void()>;

// ============================================================
//  UIManager.h  (ui/)
//
//  Owns all view objects and dispatches to the correct one
//  based on AppState::screen.
//
//  UIManager sits between Renderer (knows nothing about app
//  logic) and the individual view classes (know nothing about
//  networking).  Callbacks bridge UIManager → Application.
//
//  Usage per frame:
//      uiManager.render();          // draws the active view
// ============================================================

class UIManager
{
public:
    explicit UIManager(AppState& state);
    ~UIManager();

    UIManager(const UIManager&)            = delete;
    UIManager& operator=(const UIManager&) = delete;

    // ---- Callback registration ----
    // Call before the first render() call.
    void setCreateRoomCallback (CreateRoomCb  cb);
    void setJoinRoomCallback   (JoinRoomCb    cb);
    void setSendMessageCallback(SendMessageCb cb);
    void setSendFileCallback      (SendFileCb     cb);
    void setBrowseFileCallback    (BrowseFileCb   cb);
    void setDisconnectUICallback  (DisconnectUICb cb);

    // ---- Per-frame entry point ----
    // Must be called between Renderer::beginFrame() and
    // Renderer::endFrame().
    void render();

private:
    // Draws a full-screen DockSpace so child windows can anchor
    void beginDockspace();

    AppState& state_;

    std::unique_ptr<MainMenuView> mainMenuView_;
    std::unique_ptr<JoinRoomView> joinRoomView_;
    std::unique_ptr<ChatView>     chatView_;

    CreateRoomCb   onCreateRoom_;
    BrowseFileCb   onBrowseFile_;
    DisconnectUICb onDisconnectUI_;
    JoinRoomCb    onJoinRoom_;
    SendMessageCb onSendMessage_;
    SendFileCb    onSendFile_;
};
