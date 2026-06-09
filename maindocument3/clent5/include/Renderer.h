#pragma once

#include "Window.h"

// ============================================================
//  Renderer.h
//  Owns the ImGui SDL3 + OpenGL3 backends.
//
//  Responsibilities:
//    - Init / shutdown ImGui context and backends
//    - Begin / end ImGui frame
//    - Clear the OpenGL back buffer
//    - Apply the custom dark theme
//
//  The Renderer does NOT know about application state.
//  UIManager calls Renderer::beginFrame() / endFrame()
//  and draws its own ImGui windows in between.
// ============================================================

class Renderer
{
public:
    explicit Renderer(Window& window);
    ~Renderer();

    Renderer(const Renderer&)            = delete;
    Renderer& operator=(const Renderer&) = delete;

    // ---- Lifecycle ----

    bool init();
    void shutdown();

    // ---- Per-frame ----

    // Forward an SDL event to the ImGui backend
    void processEvent(const SDL_Event& event);

    // Begin a new ImGui frame (also clears the GL buffer)
    void beginFrame();

    // Render the ImGui draw data and swap buffers
    void endFrame();

    // ---- Theme ----

    // Applies the custom dark P2P-client colour palette.
    // Called once during init; can be recalled to hot-reload.
    static void applyTheme();

private:
    Window& window_;
    bool    initialized_ = false;
};
