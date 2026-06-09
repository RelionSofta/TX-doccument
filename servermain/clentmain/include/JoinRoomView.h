#pragma once

#include <functional>
#include <string>
#include "AppState.h"

// ============================================================
//  JoinRoomView.h
//
//  Screen 2 — enter a room code and press Join.
//  Also contains a Back button (returns to MainMenu).
// ============================================================

class JoinRoomView
{
public:
    using JoinCb = std::function<void(const std::string& code)>;
    using BackCb = std::function<void()>;

    explicit JoinRoomView(AppState& state);

    void setJoinCallback(JoinCb cb);
    void setBackCallback(BackCb cb);

    void render();

private:
    AppState& state_;
    JoinCb    onJoin_;
    BackCb    onBack_;

    char codeBuffer_[16] = {};
    bool showError_      = false;
};
