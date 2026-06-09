#include "UIManager.h"

#include "MainMenuView.h"
#include "JoinRoomView.h"
#include "ChatView.h"

#include <imgui.h>

// ============================================================
//  UIManager.cpp
// ============================================================

UIManager::UIManager(AppState& state)
    : state_(state)
    , mainMenuView_(std::make_unique<MainMenuView>(state))
    , joinRoomView_(std::make_unique<JoinRoomView>(state))
    , chatView_    (std::make_unique<ChatView>    (state))
{}

UIManager::~UIManager() = default;

// ---- Callback wiring ----------------------------------------

void UIManager::setCreateRoomCallback(CreateRoomCb cb)
{
    onCreateRoom_ = cb;

    mainMenuView_->setCreateRoomCallback([this]()
    {
        if (onCreateRoom_) onCreateRoom_();
    });
}

void UIManager::setJoinRoomCallback(JoinRoomCb cb)
{
    onJoinRoom_ = cb;

    // MainMenuView just navigates to JoinRoom screen
    mainMenuView_->setJoinRoomCallback([this]()
    {
        state_.screen = AppScreen::JoinRoom;
    });

    // JoinRoomView fires the actual join with the code
    joinRoomView_->setJoinCallback([this](const std::string& code)
    {
        if (onJoinRoom_) onJoinRoom_(code);
    });

    joinRoomView_->setBackCallback([this]()
    {
        state_.screen = AppScreen::MainMenu;
    });
}

void UIManager::setSendMessageCallback(SendMessageCb cb)
{
    onSendMessage_ = cb;

    chatView_->setSendMessageCallback([this](const std::string& text)
    {
        state_.pushLocalMessage(text);
        if (onSendMessage_) onSendMessage_(text);
    });
}

void UIManager::setSendFileCallback(SendFileCb cb)
{
    onSendFile_ = cb;

    chatView_->setSendFileCallback([this](const std::string& path)
    {
        if (onSendFile_) onSendFile_(path);
    });
}

void UIManager::setBrowseFileCallback(BrowseFileCb cb)
{
    onBrowseFile_ = std::move(cb);
    chatView_->setBrowseFileCallback([this]()
    {
        if (onBrowseFile_) onBrowseFile_();
    });
}

void UIManager::setDisconnectUICallback(DisconnectUICb cb)
{
    onDisconnectUI_ = std::move(cb);
    chatView_->setDisconnectCallback([this]()
    {
        if (onDisconnectUI_) onDisconnectUI_();
    });
}

// ---- Per-frame render ---------------------------------------

void UIManager::render()
{
    switch (state_.screen)
    {
    case AppScreen::MainMenu:
        mainMenuView_->render();
        break;

    case AppScreen::JoinRoom:
        joinRoomView_->render();
        break;

    case AppScreen::Chat:
        chatView_->render();
        break;
    }
}
