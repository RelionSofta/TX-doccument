#pragma once
#include <atomic>

#include <string>
#include <vector>
#include <chrono>

// ============================================================
//  AppState.h
//  Central state definitions for the entire application.
//  No logic lives here — pure data aggregates only.
//  UIManager, Application and NetworkClient all share
//  a reference to a single AppState instance.
// ============================================================

enum class AppScreen
{
    MainMenu,   // Initial splash: Create Room / Join Room
    JoinRoom,   // Input a room code and hit Join
    Chat        // Active session: text + file transfer
};

// ----------------------------------------------------------------
// A single chat entry displayed in the chat history area
// ----------------------------------------------------------------
struct ChatEntry
{
    enum class Kind { Local, Remote, System };

    Kind        kind = Kind::System;
    std::string sender;   // display name or "You" / "Peer"
    std::string text;
    std::chrono::system_clock::time_point timestamp =
        std::chrono::system_clock::now();
};

// ----------------------------------------------------------------
// File-transfer progress visible to the UI
// ----------------------------------------------------------------
struct FileTransferState
{
    bool        active = false;
    bool        sending = false;
    std::string filename;

    // Atomic — IO thread writes, UI thread reads safely
    std::atomic<uint64_t> bytesDone{ 0 };
    std::atomic<uint64_t> bytesTotal{ 0 };

    float progress() const
    {
        uint64_t total = bytesTotal.load();
        if (total == 0) return 0.f;
        return static_cast<float>(bytesDone.load()) /
            static_cast<float>(total);
    }

    // Reset all fields to default — use instead of = {}
    // because std::atomic is non-copyable / non-assignable.
    void reset()
    {
        active = false;
        sending = false;
        filename.clear();
        bytesDone.store(0);
        bytesTotal.store(0);
    }
};

// ----------------------------------------------------------------
// Top-level application state — one instance, passed by reference
// ----------------------------------------------------------------
struct AppState
{
    // --- navigation ---
    AppScreen screen = AppScreen::MainMenu;

    // --- connection meta ---
    bool        connected = false;
    std::string roomCode;          // populated after CreateRoom/JoinRoom

    // --- chat ---
    std::vector<ChatEntry> chatHistory;

    // --- file transfer ---
    std::string          selectedFilePath;   // chosen via NFD
    FileTransferState    fileTransfer;

    // --- UI scratch buffers (owned here, not in view classes) ---
    char joinRoomCodeBuf[16] = {};
    char chatInputBuf[512] = {};

    // Convenience helpers
    void pushSystemMessage(const std::string& text)
    {
        chatHistory.push_back({ ChatEntry::Kind::System, "System", text });
    }

    void pushLocalMessage(const std::string& text)
    {
        chatHistory.push_back({ ChatEntry::Kind::Local, "You", text });
    }

    void pushRemoteMessage(const std::string& sender,
        const std::string& text)
    {
        chatHistory.push_back({ ChatEntry::Kind::Remote, sender, text });
    }
};