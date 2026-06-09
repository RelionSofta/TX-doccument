#pragma once

#include <functional>
#include "AppState.h"

// ============================================================
//  MainMenuView.h
//
//  Screen 1 — initial splash with two CTA buttons:
//    • Create Room
//    • Join Room
//
//  Fires callbacks; does NOT modify AppState directly.
//  State transitions are the Application's responsibility.
// ============================================================

class MainMenuView
{
public:
    using CreateRoomCb = std::function<void()>;
    using JoinRoomCb   = std::function<void()>;   // just navigates to Screen 2

    explicit MainMenuView(AppState& state);

    void setCreateRoomCallback(CreateRoomCb cb);
    void setJoinRoomCallback  (JoinRoomCb   cb);

    // Call between ImGui::NewFrame() and ImGui::Render()
    void render();

private:
    AppState&    state_;
    CreateRoomCb onCreateRoom_;
    JoinRoomCb   onJoinRoom_;
};
