#pragma once

#include <string>
#include <SDL3/SDL.h>

// ============================================================
//  Window.h
//  Owns the SDL3 window and its OpenGL context.
//  Renderer and ImGui backends attach themselves to the
//  SDL_Window* / SDL_GLContext that Window exposes.
//
//  Responsibilities:
//    - Create / destroy SDL window + GL context
//    - Handle SDL_EVENT_WINDOW_* events (resize, close)
//    - Expose raw handles for Renderer and ImGui
//    - Swap buffers (presentFrame)
// ============================================================

struct WindowConfig
{
    std::string title  = "TX Doc";
    int         width  = 1280;
    int         height = 720;
    bool        vsync  = true;
    bool        resizable = true;
};

class Window
{
public:
    explicit Window(const WindowConfig& cfg = {});
    ~Window();

    // Non-copyable, non-movable (owns raw SDL handles)
    Window(const Window&)            = delete;
    Window& operator=(const Window&) = delete;

    // ---- Lifecycle ----

    bool init();    // returns false on error
    void shutdown();

    // ---- Per-frame ----

    // Process a single SDL event; returns true if it was
    // a quit request (SDL_EVENT_QUIT or window-close).
    bool handleEvent(const SDL_Event& event);

    // Swap the back buffer to screen
    void presentFrame() const;

    // ---- Accessors ----

    SDL_Window*   sdlWindow()    const { return window_; }
    SDL_GLContext glContext()    const { return glContext_; }

    int  width()  const { return width_;  }
    int  height() const { return height_; }

    bool shouldClose() const { return shouldClose_; }

    // Called by the application when the viewport changes
    void onResize(int w, int h);

private:
    SDL_Window*   window_     = nullptr;
    SDL_GLContext glContext_   = nullptr;
    WindowConfig  config_;
    int           width_      = 0;
    int           height_     = 0;
    bool          shouldClose_= false;
};
