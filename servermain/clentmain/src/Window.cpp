#include "Window.h"

#include <glad/glad.h>
#include <iostream>

// ============================================================
//  Window.cpp
// ============================================================

Window::Window(const WindowConfig& cfg)
    : config_(cfg)
    , width_(cfg.width)
    , height_(cfg.height)
{
}

Window::~Window()
{
    shutdown();
}

bool Window::init()
{
    // ---- SDL init ----
    if (!SDL_Init(SDL_INIT_VIDEO))
    {
        std::cerr << "[Window] SDL_Init failed: "
            << SDL_GetError() << '\n';
        return false;
    }

    // ---- OpenGL attributes ----
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, 0);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK,
        SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
    SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);

    // ---- Create window ----
    SDL_WindowFlags flags = SDL_WINDOW_OPENGL;
    if (config_.resizable)
        flags |= SDL_WINDOW_RESIZABLE;

    window_ = SDL_CreateWindow(
        config_.title.c_str(),
        config_.width,
        config_.height,
        flags);

    if (!window_)
    {
        std::cerr << "[Window] SDL_CreateWindow failed: "
            << SDL_GetError() << '\n';
        return false;
    }

    // ---- GL context ----
    glContext_ = SDL_GL_CreateContext(window_);
    if (!glContext_)
    {
        std::cerr << "[Window] SDL_GL_CreateContext failed: "
            << SDL_GetError() << '\n';
        return false;
    }

    SDL_GL_MakeCurrent(window_, glContext_);

    if (config_.vsync)
    {
        // Try adaptive vsync first (-1), fallback to regular (1)
        if (!SDL_GL_SetSwapInterval(-1))
            SDL_GL_SetSwapInterval(1);
    }
    else
    {
        SDL_GL_SetSwapInterval(0);
    }

    // ---- GLAD ----
    if (!gladLoadGLLoader(
        reinterpret_cast<GLADloadproc>(SDL_GL_GetProcAddress)))
    {
        std::cerr << "[Window] gladLoadGLLoader failed\n";
        return false;
    }

    std::cout << "[Window] OpenGL "
        << glGetString(GL_VERSION) << '\n';

    // ---- Window Icon (logo) ----
    // logo.bmp exe ke saath same folder mein rakho
    // File nahi mili toh silently skip — koi crash nahi
    SDL_Surface* icon = SDL_LoadBMP("logo.bmp");
    if (icon)
    {
        SDL_SetWindowIcon(window_, icon);
        SDL_DestroySurface(icon);
    }

    return true;
}

void Window::shutdown()
{
    if (glContext_)
    {
        SDL_GL_DestroyContext(glContext_);
        glContext_ = nullptr;
    }
    if (window_)
    {
        SDL_DestroyWindow(window_);
        window_ = nullptr;
    }
    SDL_Quit();
}

bool Window::handleEvent(const SDL_Event& event)
{
    if (event.type == SDL_EVENT_QUIT)
    {
        shouldClose_ = true;
        return true;
    }

    if (event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED &&
        event.window.windowID == SDL_GetWindowID(window_))
    {
        shouldClose_ = true;
        return true;
    }

    if (event.type == SDL_EVENT_WINDOW_RESIZED)
    {
        onResize(event.window.data1, event.window.data2);
    }

    return false;
}

void Window::presentFrame() const
{
    SDL_GL_SwapWindow(window_);
}

void Window::onResize(int w, int h)
{
    width_ = w;
    height_ = h;
    glViewport(0, 0, w, h);
}