#include "MainMenuView.h"

#include <imgui.h>
#include <string>

// ============================================================
//  MainMenuView.cpp
//
//  Design notes:
//    - Centred in the OS window
//    - Logo / app name at top
//    - Two full-width accent buttons with subtle divider
//    - Connection status badge at bottom when relevant
// ============================================================

MainMenuView::MainMenuView(AppState& state)
    : state_(state)
{
}

void MainMenuView::setCreateRoomCallback(CreateRoomCb cb)
{
    onCreateRoom_ = std::move(cb);
}

void MainMenuView::setJoinRoomCallback(JoinRoomCb cb)
{
    onJoinRoom_ = std::move(cb);
}

void MainMenuView::render()
{
    // ---- Full-screen invisible host window ----
    const ImGuiIO& io = ImGui::GetIO();

    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(io.DisplaySize);
    ImGui::SetNextWindowBgAlpha(0.0f);

    ImGuiWindowFlags hostFlags =
        ImGuiWindowFlags_NoDecoration |
        ImGuiWindowFlags_NoMove       |
        ImGuiWindowFlags_NoNav;

    ImGui::Begin("##MainHost", nullptr, hostFlags);

    // ---- Centred card panel ----
    const float panelW = 360.0f;
    const float panelH = 340.0f;
    const float cx     = (io.DisplaySize.x - panelW) * 0.5f;
    const float cy     = (io.DisplaySize.y - panelH) * 0.5f;

    ImGui::SetNextWindowPos(ImVec2(cx, cy), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(panelW, panelH));

    ImGuiWindowFlags cardFlags =
        ImGuiWindowFlags_NoDecoration |
        ImGuiWindowFlags_NoMove       |
        ImGuiWindowFlags_NoNav;

    ImGui::PushStyleColor(ImGuiCol_WindowBg,
        ImVec4(0.114f, 0.114f, 0.157f, 1.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 12.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(32.0f, 32.0f));

    ImGui::Begin("##Card", nullptr, cardFlags);

    // ---- Accent stripe at top ----
    {
        ImDrawList* dl  = ImGui::GetWindowDrawList();
        ImVec2      pos = ImGui::GetWindowPos();
        dl->AddRectFilled(
            pos,
            ImVec2(pos.x + panelW, pos.y + 4.0f),
            IM_COL32(0, 212, 255, 255),   // electric cyan
            12.0f,
            ImDrawFlags_RoundCornersTop);
    }

    ImGui::Dummy(ImVec2(0, 8));

    // ---- App title ----
    ImGui::PushStyleColor(ImGuiCol_Text,
        ImVec4(0.0f, 0.83f, 1.0f, 1.0f));

    const char* title = "TX Doc";
    float tw = ImGui::CalcTextSize(title).x;
    ImGui::SetCursorPosX((panelW - tw) * 0.5f);
    ImGui::TextUnformatted(title);

    ImGui::PopStyleColor();

    // ---- Sub-title ----
    ImGui::PushStyleColor(ImGuiCol_Text,
        ImVec4(0.44f, 0.44f, 0.56f, 1.0f));
    const char* sub = "Secure peer-to-peer file & chat";
    float sw = ImGui::CalcTextSize(sub).x;
    ImGui::SetCursorPosX((panelW - sw) * 0.5f);
    ImGui::TextUnformatted(sub);
    ImGui::PopStyleColor();

    ImGui::Dummy(ImVec2(0, 24));
    ImGui::Separator();
    ImGui::Dummy(ImVec2(0, 20));

    // ---- Buttons ----
    const float btnW = panelW - 64.0f;   // respects WindowPadding * 2
    const float btnH = 42.0f;

    // Create Room
    ImGui::PushStyleColor(ImGuiCol_Button,
        ImVec4(0.0f, 0.55f, 0.68f, 0.90f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered,
        ImVec4(0.0f, 0.72f, 0.88f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive,
        ImVec4(0.0f, 0.45f, 0.56f, 1.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 6.0f);

    if (ImGui::Button("  +  Create Room", ImVec2(btnW, btnH)))
    {
        if (onCreateRoom_) onCreateRoom_();
    }

    ImGui::PopStyleColor(3);
    ImGui::PopStyleVar();

    ImGui::Dummy(ImVec2(0, 8));

    // Join Room (outline-style — slightly transparent fill)
    ImGui::PushStyleColor(ImGuiCol_Button,
        ImVec4(0.0f, 0.55f, 0.68f, 0.20f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered,
        ImVec4(0.0f, 0.55f, 0.68f, 0.45f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive,
        ImVec4(0.0f, 0.55f, 0.68f, 0.60f));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 6.0f);

    if (ImGui::Button("  ->  Join Room", ImVec2(btnW, btnH)))
    {
        if (onJoinRoom_) onJoinRoom_();
    }

    ImGui::PopStyleColor(3);
    ImGui::PopStyleVar();

    // ---- Version badge ----
    ImGui::Dummy(ImVec2(0, 16));
    ImGui::PushStyleColor(ImGuiCol_Text,
        ImVec4(0.30f, 0.30f, 0.40f, 1.0f));
    const char* ver = "v1.0";
    float vw = ImGui::CalcTextSize(ver).x;
    ImGui::SetCursorPosX((panelW - vw) * 0.5f);
    ImGui::TextUnformatted(ver);
    ImGui::PopStyleColor();

    ImGui::End(); // card

    ImGui::PopStyleColor(); // WindowBg
    ImGui::PopStyleVar(2);  // WindowRounding, WindowPadding

    ImGui::End(); // host
}
