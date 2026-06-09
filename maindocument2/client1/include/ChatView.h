#pragma once

#include <functional>
#include <string>
#include "AppState.h"

// ============================================================
//  ChatView.h
//
//  Screen 3 — active session layout:
//
//   ┌──────────────────────────────────────┐
//   │  Room: XXXXXX          [Disconnect]  │  <- header bar
//   ├──────────────────────────────────────┤
//   │                                      │
//   │         Chat History                 │  <- scrollable
//   │                                      │
//   ├──────────────────────────────────────┤
//   │  [Message input field ]  [ Send ]    │
//   ├──────────────────────────────────────┤
//   │  [Browse]  [file path............]   │
//   │            [Send File]  [progress]   │
//   └──────────────────────────────────────┘
// ============================================================

class ChatView
{
public:
    using SendMessageCb  = std::function<void(const std::string&)>;
    using SendFileCb     = std::function<void(const std::string&)>;
    using BrowseFileCb   = std::function<void()>;   // opens NFD
    using DisconnectCb   = std::function<void()>;

    explicit ChatView(AppState& state);

    void setSendMessageCallback (SendMessageCb  cb);
    void setSendFileCallback    (SendFileCb     cb);
    void setBrowseFileCallback  (BrowseFileCb   cb);
    void setDisconnectCallback  (DisconnectCb   cb);

    void render();

private:
    void renderHeader();
    void renderChatHistory();
    void renderMessageInput();
    void renderFileTransferPanel();

    AppState& state_;

    SendMessageCb  onSendMessage_;
    SendFileCb     onSendFile_;
    BrowseFileCb   onBrowseFile_;
    DisconnectCb   onDisconnect_;

    bool scrollToBottom_ = true;
};
