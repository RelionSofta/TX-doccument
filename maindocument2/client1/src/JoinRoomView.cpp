#include "JoinRoomView.h"

#include <imgui.h>
#include <cstring>

// ============================================================
//  JoinRoomView.cpp
// ============================================================

JoinRoomView::JoinRoomView(AppState& state)
    : state_(state)
{}

void JoinRoomView::setJoinCallback(JoinCb cb) { onJoin_ = std::move(cb); }
void JoinRoomView::setBackCallback(BackCb cb) { onBack_ = std::move(cb); }

void JoinRoomView::render()
{
    const ImGuiIO& io = ImGui::GetIO();

    // ---- Full-screen host ----
    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(io.DisplaySize);
    ImGui::SetNextWindowBgAlpha(0.0f);

    ImGui::Begin("##JoinHost", nullptr,
        ImGuiWindowFlags_NoDecoration |
        ImGuiWindowFlags_NoMove       |
        ImGuiWindowFlags_NoNav);

    // ---- Card ----
    const float panelW = 400.0f;
    const float panelH = 280.0f;
    const float cx     = (io.DisplaySize.x - panelW) * 0.5f;
    const float cy     = (io.DisplaySize.y - panelH) * 0.5f;

    ImGui::SetNextWindowPos(ImVec2(cx, cy), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(panelW, panelH));

    ImGui::PushStyleColor(ImGuiCol_WindowBg,
        ImVec4(0.114f, 0.114f, 0.157f, 1.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 12.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(32.0f, 28.0f));

    ImGui::Begin("##JoinCard", nullptr,
        ImGuiWindowFlags_NoDecoration |
        ImGuiWindowFlags_NoMove       |
        ImGuiWindowFlags_NoNav);

    // Accent stripe
    {
        ImDrawList* dl  = ImGui::GetWindowDrawList();
        ImVec2      pos = ImGui::GetWindowPos();
        dl->AddRectFilled(
            pos,
            ImVec2(pos.x + panelW, pos.y + 4.0f),
            IM_COL32(0, 212, 255, 255),
            12.0f, ImDrawFlags_RoundCornersTop);
    }

    ImGui::Dummy(ImVec2(0, 8));

    // Title
    ImGui::PushStyleColor(ImGuiCol_Text,
        ImVec4(0.82f, 0.82f, 0.88f, 1.0f));
    ImGui::TextUnformatted("Join a Room");
    ImGui::PopStyleColor();

    ImGui::PushStyleColor(ImGuiCol_Text,
        ImVec4(0.44f, 0.44f, 0.56f, 1.0f));
    ImGui::TextUnformatted("Enter the room code shared by your peer.");
    ImGui::PopStyleColor();

    ImGui::Dummy(ImVec2(0, 16));

    // Code input
    const float inputW = panelW - 64.0f;

    ImGui::PushStyleColor(ImGuiCol_Text,
        ImVec4(0.60f, 0.60f, 0.72f, 1.0f));
    ImGui::TextUnformatted("Room Code");
    ImGui::PopStyleColor();

    ImGui::PushItemWidth(inputW);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 5.0f);

    bool enterPressed = ImGui::InputText(
        "##roomcode",
        codeBuffer_,
        sizeof(codeBuffer_),
        ImGuiInputTextFlags_EnterReturnsTrue |
        ImGuiInputTextFlags_CharsUppercase);

    ImGui::PopStyleVar();
    ImGui::PopItemWidth();

    // Auto-focus on first render
    if (ImGui::IsWindowAppearing())
        ImGui::SetKeyboardFocusHere(-1);

    if (showError_)
    {
        ImGui::PushStyleColor(ImGuiCol_Text,
            ImVec4(1.0f, 0.25f, 0.38f, 1.0f));
        ImGui::TextUnformatted("  Room code cannot be empty.");
        ImGui::PopStyleColor();
    }
    else
    {
        ImGui::Dummy(ImVec2(0, ImGui::GetTextLineHeightWithSpacing()));
    }

    ImGui::Dummy(ImVec2(0, 8));

    // Buttons
    const float halfW = (inputW - 8.0f) * 0.5f;

    // Back
    ImGui::PushStyleColor(ImGuiCol_Button,
        ImVec4(0.18f, 0.18f, 0.25f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered,
        ImVec4(0.24f, 0.24f, 0.32f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive,
        ImVec4(0.14f, 0.14f, 0.20f, 1.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 6.0f);

    if (ImGui::Button("<- Back", ImVec2(halfW, 38.0f)))
    {
        showError_ = false;
        std::memset(codeBuffer_, 0, sizeof(codeBuffer_));
        if (onBack_) onBack_();
    }

    ImGui::PopStyleColor(3);
    ImGui::PopStyleVar();

    ImGui::SameLine(0, 8.0f);

    // Join
    ImGui::PushStyleColor(ImGuiCol_Button,
        ImVec4(0.0f, 0.55f, 0.68f, 0.90f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered,
        ImVec4(0.0f, 0.72f, 0.88f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive,
        ImVec4(0.0f, 0.45f, 0.56f, 1.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 6.0f);

    bool joinPressed =
        ImGui::Button("Join ->", ImVec2(halfW, 38.0f)) || enterPressed;

    ImGui::PopStyleColor(3);
    ImGui::PopStyleVar();

    if (joinPressed)
    {
        if (codeBuffer_[0] == '\0')
        {
            showError_ = true;
        }
        else
        {
            showError_ = false;
            if (onJoin_) onJoin_(std::string(codeBuffer_));
        }
    }

    ImGui::End();             // card
    ImGui::PopStyleColor();   // WindowBg
    ImGui::PopStyleVar(2);    // WindowRounding, WindowPadding

    ImGui::End();             // host
}
