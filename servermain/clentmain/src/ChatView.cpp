#include "ChatView.h"

#include <imgui.h>
#include <cstring>
#include <cstdio>
#include <chrono>
#include <ctime>

// ============================================================
//  ChatView.cpp
// ============================================================

ChatView::ChatView(AppState& state)
    : state_(state)
{
}

void ChatView::setSendMessageCallback(SendMessageCb  cb) { onSendMessage_ = std::move(cb); }
void ChatView::setSendFileCallback(SendFileCb     cb) { onSendFile_ = std::move(cb); }
void ChatView::setBrowseFileCallback(BrowseFileCb   cb) { onBrowseFile_ = std::move(cb); }
void ChatView::setDisconnectCallback(DisconnectCb   cb) { onDisconnect_ = std::move(cb); }

// ---- Top-level render ---------------------------------------

void ChatView::render()
{
    const ImGuiIO& io = ImGui::GetIO();

    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(io.DisplaySize);

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::PushStyleColor(ImGuiCol_WindowBg,
        ImVec4(0.086f, 0.086f, 0.118f, 1.0f));

    ImGui::Begin("##ChatRoot", nullptr,
        ImGuiWindowFlags_NoDecoration |
        ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoNav);

    ImGui::PopStyleVar();   // WindowPadding=0
    ImGui::PopStyleColor(); // WindowBg

    renderHeader();
    renderChatHistory();
    renderMessageInput();
    renderFileTransferPanel();

    ImGui::End();
}

// ---- Header bar ---------------------------------------------

void ChatView::renderHeader()
{
    ImGui::PushStyleColor(ImGuiCol_ChildBg,
        ImVec4(0.08f, 0.08f, 0.11f, 1.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(12, 0));

    ImGui::BeginChild("##Header",
        ImVec2(0, 44),
        ImGuiChildFlags_None,
        ImGuiWindowFlags_NoScrollbar);

    // Room code badge
    ImGui::SetCursorPosY(
        (44.0f - ImGui::GetTextLineHeight()) * 0.5f);

    ImGui::PushStyleColor(ImGuiCol_Text,
        ImVec4(0.44f, 0.44f, 0.56f, 1.0f));
    ImGui::TextUnformatted("Room");
    ImGui::PopStyleColor();

    ImGui::SameLine(0, 6);

    ImGui::PushStyleColor(ImGuiCol_Text,
        ImVec4(0.0f, 0.83f, 1.0f, 1.0f));

    const std::string& code = state_.roomCode;
    ImGui::TextUnformatted(code.empty() ? "—" : code.c_str());

    ImGui::PopStyleColor();

    // Connection indicator dot
    ImGui::SameLine(0, 12);
    {
        ImDrawList* dl = ImGui::GetWindowDrawList();
        ImVec2 cp = ImGui::GetCursorScreenPos();
        float r = 5.0f;
        float cy = cp.y + ImGui::GetTextLineHeight() * 0.5f;
        ImU32 col = state_.connected
            ? IM_COL32(0, 220, 120, 255)
            : IM_COL32(255, 64, 96, 255);
        dl->AddCircleFilled(ImVec2(cp.x + r, cy), r, col);
        ImGui::Dummy(ImVec2(r * 2 + 4, 0));
    }

    // Disconnect button — right-aligned
    const char* discLabel = "Disconnect";
    float discW = ImGui::CalcTextSize(discLabel).x + 20.0f;
    float avail = ImGui::GetContentRegionAvail().x;

    ImGui::SameLine(avail - discW);
    ImGui::SetCursorPosY(
        (44.0f - ImGui::GetFrameHeight()) * 0.5f);

    ImGui::PushStyleColor(ImGuiCol_Button,
        ImVec4(0.55f, 0.08f, 0.14f, 0.70f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered,
        ImVec4(0.75f, 0.10f, 0.18f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive,
        ImVec4(0.45f, 0.06f, 0.10f, 1.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 4.0f);

    if (ImGui::Button(discLabel))
    {
        if (onDisconnect_) onDisconnect_();
    }

    ImGui::PopStyleColor(3);
    ImGui::PopStyleVar();

    ImGui::EndChild();
    ImGui::PopStyleColor(); // ChildBg
    ImGui::PopStyleVar();   // WindowPadding

    // Separator line
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2      wpos = ImGui::GetWindowPos();
    float       y = ImGui::GetCursorScreenPos().y;
    dl->AddLine(
        ImVec2(wpos.x, y),
        ImVec2(wpos.x + ImGui::GetWindowWidth(), y),
        IM_COL32(40, 40, 56, 255));
}

// ---- Chat history -------------------------------------------

void ChatView::renderChatHistory()
{
    // Reserve space for message input + file panel at the bottom
    const float bottomReserve = 44.0f + 90.0f + 8.0f;
    const float historyH =
        ImGui::GetContentRegionAvail().y - bottomReserve;

    ImGui::PushStyleColor(ImGuiCol_ChildBg,
        ImVec4(0.086f, 0.086f, 0.118f, 1.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,
        ImVec2(14, 10));

    ImGui::BeginChild("##History",
        ImVec2(0, historyH),
        ImGuiChildFlags_None,
        ImGuiWindowFlags_NoScrollbar);

    for (const auto& entry : state_.chatHistory)
    {
        // Timestamp
        auto tt = std::chrono::system_clock::to_time_t(
            entry.timestamp);
        char tsBuf[10];
        std::strftime(tsBuf, sizeof(tsBuf), "%H:%M", std::localtime(&tt));

        bool isLocal = (entry.kind == ChatEntry::Kind::Local);
        bool isSystem = (entry.kind == ChatEntry::Kind::System);

        if (isSystem)
        {
            // Centred dim system note
            ImGui::PushStyleColor(ImGuiCol_Text,
                ImVec4(0.35f, 0.35f, 0.48f, 1.0f));
            float tw = ImGui::CalcTextSize(entry.text.c_str()).x;
            float cx = (ImGui::GetContentRegionAvail().x - tw) * 0.5f;
            if (cx > 0) ImGui::SetCursorPosX(
                ImGui::GetCursorPosX() + cx);
            ImGui::TextUnformatted(entry.text.c_str());
            ImGui::PopStyleColor();
            ImGui::Dummy(ImVec2(0, 4));
            continue;
        }

        // Sender + timestamp label
        {
            ImGui::PushStyleColor(ImGuiCol_Text,
                isLocal
                ? ImVec4(0.0f, 0.83f, 1.0f, 0.80f)
                : ImVec4(0.75f, 0.60f, 1.0f, 0.80f));

            ImGui::TextUnformatted(entry.sender.c_str());
            ImGui::SameLine(0, 6);
            ImGui::PopStyleColor();

            ImGui::PushStyleColor(ImGuiCol_Text,
                ImVec4(0.28f, 0.28f, 0.40f, 1.0f));
            ImGui::TextUnformatted(tsBuf);
            ImGui::PopStyleColor();
        }

        // Message bubble
        ImGui::PushStyleColor(ImGuiCol_Text,
            ImVec4(0.82f, 0.82f, 0.88f, 1.0f));

        float wrapW = ImGui::GetContentRegionAvail().x * 0.80f;
        ImGui::PushTextWrapPos(
            ImGui::GetCursorPosX() + wrapW);
        ImGui::TextUnformatted(entry.text.c_str());
        ImGui::PopTextWrapPos();
        ImGui::PopStyleColor();

        ImGui::Dummy(ImVec2(0, 6));
    }

    // Auto-scroll — only when explicitly requested or already at bottom
    float scrollY = ImGui::GetScrollY();
    float scrollMaxY = ImGui::GetScrollMaxY();
    bool  atBottom = scrollMaxY <= 0.0f ||
        (scrollY >= scrollMaxY - 4.0f);

    if (scrollToBottom_ || atBottom)
        ImGui::SetScrollHereY(1.0f);

    scrollToBottom_ = false;

    ImGui::EndChild();
    ImGui::PopStyleColor();
    ImGui::PopStyleVar();
}

// ---- Message input ------------------------------------------

void ChatView::renderMessageInput()
{
    ImGui::PushStyleColor(ImGuiCol_ChildBg,
        ImVec4(0.10f, 0.10f, 0.14f, 1.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,
        ImVec2(10, 8));

    ImGui::BeginChild("##MsgInput",
        ImVec2(0, 44),
        ImGuiChildFlags_None,
        ImGuiWindowFlags_NoScrollbar);

    const float sendBtnW = 72.0f;
    const float inputW =
        ImGui::GetContentRegionAvail().x - sendBtnW - 8.0f;

    ImGui::SetNextItemWidth(inputW);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 5.0f);

    bool enterPressed = ImGui::InputText(
        "##chatinput",
        state_.chatInputBuf,
        sizeof(state_.chatInputBuf),
        ImGuiInputTextFlags_EnterReturnsTrue);

    ImGui::PopStyleVar();

    ImGui::SameLine(0, 8);

    ImGui::PushStyleColor(ImGuiCol_Button,
        ImVec4(0.0f, 0.55f, 0.68f, 0.90f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered,
        ImVec4(0.0f, 0.72f, 0.88f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive,
        ImVec4(0.0f, 0.45f, 0.56f, 1.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 5.0f);

    bool sendClicked = ImGui::Button("Send",
        ImVec2(sendBtnW, ImGui::GetFrameHeight()));

    ImGui::PopStyleColor(3);
    ImGui::PopStyleVar();

    if ((enterPressed || sendClicked) &&
        state_.chatInputBuf[0] != '\0')
    {
        scrollToBottom_ = true;
        if (onSendMessage_)
            onSendMessage_(std::string(state_.chatInputBuf));
        std::memset(state_.chatInputBuf, 0,
            sizeof(state_.chatInputBuf));
        ImGui::SetKeyboardFocusHere(-1);
    }

    ImGui::EndChild();
    ImGui::PopStyleColor();
    ImGui::PopStyleVar();
}

// ---- File transfer panel ------------------------------------

void ChatView::renderFileTransferPanel()
{
    ImGui::PushStyleColor(ImGuiCol_ChildBg,
        ImVec4(0.09f, 0.09f, 0.13f, 1.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,
        ImVec2(10, 8));

    ImGui::BeginChild("##FilePanel",
        ImVec2(0, 0),
        ImGuiChildFlags_None);

    ImGui::PushStyleColor(ImGuiCol_Text,
        ImVec4(0.44f, 0.44f, 0.56f, 1.0f));
    ImGui::TextUnformatted("File Transfer");
    ImGui::PopStyleColor();

    ImGui::SameLine();

    // Separator to the right
    {
        ImVec2 p = ImGui::GetCursorScreenPos();
        float  y = p.y + ImGui::GetTextLineHeight() * 0.5f;
        float  x2 = ImGui::GetWindowPos().x +
            ImGui::GetWindowWidth() - 10.0f;
        ImGui::GetWindowDrawList()->AddLine(
            ImVec2(p.x + 4, y),
            ImVec2(x2, y),
            IM_COL32(40, 40, 60, 255));
    }

    ImGui::Dummy(ImVec2(0, 2));

    // Row 1: Browse | path field
    const float browseBtnW = 80.0f;
    const float sendFileBtnW = 90.0f;
    const float pathW =
        ImGui::GetContentRegionAvail().x
        - browseBtnW - sendFileBtnW - 16.0f;

    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 5.0f);
    ImGui::PushStyleColor(ImGuiCol_Button,
        ImVec4(0.18f, 0.18f, 0.26f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered,
        ImVec4(0.24f, 0.24f, 0.33f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive,
        ImVec4(0.14f, 0.14f, 0.20f, 1.0f));

    if (ImGui::Button("Browse", ImVec2(browseBtnW, 0)))
    {
        if (onBrowseFile_) onBrowseFile_();
    }

    ImGui::PopStyleColor(3);
    ImGui::PopStyleVar();

    ImGui::SameLine(0, 6);

    // Read-only path display
    ImGui::PushStyleColor(ImGuiCol_FrameBg,
        ImVec4(0.12f, 0.12f, 0.17f, 1.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 5.0f);
    ImGui::SetNextItemWidth(pathW);

    const std::string& fp = state_.selectedFilePath;
    char pathBuf[512] = {};
    if (!fp.empty())
        std::snprintf(pathBuf, sizeof(pathBuf), "%s", fp.c_str());

    ImGui::InputText("##filepath", pathBuf, sizeof(pathBuf),
        ImGuiInputTextFlags_ReadOnly);

    ImGui::PopStyleColor();
    ImGui::PopStyleVar();

    ImGui::SameLine(0, 6);

    // Send File button
    bool canSend = !fp.empty() &&
        !state_.fileTransfer.active;

    if (!canSend)
    {
        ImGui::PushStyleColor(ImGuiCol_Button,
            ImVec4(0.12f, 0.12f, 0.17f, 0.5f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered,
            ImVec4(0.12f, 0.12f, 0.17f, 0.5f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive,
            ImVec4(0.12f, 0.12f, 0.17f, 0.5f));
        ImGui::PushStyleColor(ImGuiCol_Text,
            ImVec4(0.35f, 0.35f, 0.48f, 1.0f));
    }
    else
    {
        ImGui::PushStyleColor(ImGuiCol_Button,
            ImVec4(0.0f, 0.42f, 0.20f, 0.80f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered,
            ImVec4(0.0f, 0.58f, 0.28f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive,
            ImVec4(0.0f, 0.34f, 0.16f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_Text,
            ImVec4(0.82f, 0.82f, 0.88f, 1.0f));
    }

    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 5.0f);

    if (ImGui::Button("Send File",
        ImVec2(sendFileBtnW, 0)) && canSend)
    {
        if (onSendFile_) onSendFile_(fp);
    }

    ImGui::PopStyleColor(4);
    ImGui::PopStyleVar();

    // Progress bar (only when transferring)
    if (state_.fileTransfer.active)
    {
        ImGui::Dummy(ImVec2(0, 4));

        float progress = state_.fileTransfer.progress();
        char progressLabel[64];
        uint64_t done = state_.fileTransfer.bytesDone.load();
        uint64_t total = state_.fileTransfer.bytesTotal.load();
        double doneMB = done / (1024.0 * 1024.0);
        double totalMB = total / (1024.0 * 1024.0);
        std::snprintf(progressLabel, sizeof(progressLabel),
            "%s  %.1f / %.1f MB",
            state_.fileTransfer.sending ? "Sending" : "Receiving",
            doneMB, totalMB);

        ImGui::PushStyleColor(ImGuiCol_PlotHistogram,
            ImVec4(0.0f, 0.72f, 0.88f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_FrameBg,
            ImVec4(0.12f, 0.12f, 0.17f, 1.0f));

        ImGui::ProgressBar(progress,
            ImVec2(ImGui::GetContentRegionAvail().x, 16.0f),
            progressLabel);

        ImGui::PopStyleColor(2);
    }

    ImGui::EndChild();
    ImGui::PopStyleColor(); // ChildBg
    ImGui::PopStyleVar();   // WindowPadding
}