#include "Renderer.h"

#include <glad/glad.h>
#include <imgui.h>
#include <backends/imgui_impl_sdl3.h>
#include <backends/imgui_impl_opengl3.h>

#include <iostream>

// ============================================================
//  Renderer.cpp
// ============================================================

Renderer::Renderer(Window& window)
    : window_(window)
{}

Renderer::~Renderer()
{
    shutdown();
}

bool Renderer::init()
{
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();

    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    // DockSpace support — useful for future panel layout
    //io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

    // ---- Fonts ----
    // Primary UI font — load from system if present, otherwise
    // ImGui's embedded ProggyClean is used as fallback.
    // To bundle a custom font, add it to assets/fonts/ and
    // replace the path below.
    io.Fonts->AddFontDefault();

    // ---- Backends ----
    if (!ImGui_ImplSDL3_InitForOpenGL(
            window_.sdlWindow(), window_.glContext()))
    {
        std::cerr << "[Renderer] ImGui_ImplSDL3_InitForOpenGL failed\n";
        return false;
    }

    if (!ImGui_ImplOpenGL3_Init("#version 330"))
    {
        std::cerr << "[Renderer] ImGui_ImplOpenGL3_Init failed\n";
        return false;
    }

    applyTheme();

    initialized_ = true;
    return true;
}

void Renderer::shutdown()
{
    if (!initialized_) return;

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplSDL3_Shutdown();
    ImGui::DestroyContext();

    initialized_ = false;
}

void Renderer::processEvent(const SDL_Event& event)
{
    ImGui_ImplSDL3_ProcessEvent(&event);
}

void Renderer::beginFrame()
{
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplSDL3_NewFrame();
    ImGui::NewFrame();

    // Clear GL back buffer (ImGui renders on top)
    glClearColor(0.09f, 0.09f, 0.12f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
}

void Renderer::endFrame()
{
    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    window_.presentFrame();
}

// ----------------------------------------------------------------
// applyTheme — bespoke dark palette inspired by terminal aesthetics
// Accent: electric cyan  #00D4FF
// Background layers: near-black stepped greys
// ----------------------------------------------------------------
void Renderer::applyTheme()
{
    ImGui::StyleColorsDark();

    ImGuiStyle& s  = ImGui::GetStyle();
    ImVec4*     c  = s.Colors;

    // ---- Geometry ----
    s.WindowRounding    = 6.0f;
    s.ChildRounding     = 4.0f;
    s.FrameRounding     = 4.0f;
    s.PopupRounding     = 4.0f;
    s.ScrollbarRounding = 4.0f;
    s.GrabRounding      = 4.0f;
    s.TabRounding       = 4.0f;

    s.WindowBorderSize  = 1.0f;
    s.FrameBorderSize   = 0.0f;
    s.PopupBorderSize   = 1.0f;

    s.WindowPadding     = { 12.0f, 12.0f };
    s.FramePadding      = {  8.0f,  5.0f };
    s.ItemSpacing       = {  8.0f,  6.0f };
    s.ItemInnerSpacing  = {  4.0f,  4.0f };
    s.ScrollbarSize     = 10.0f;

    // ---- Palette ----
    //   bg0  = #16161E  root background
    //   bg1  = #1E1E2A  window / panel background
    //   bg2  = #252535  child / input background
    //   bg3  = #2E2E40  hover states
    //   acc  = #00D4FF  electric cyan accent
    //   accD = #008FAA  darker accent / active
    //   txt  = #D0D0E0  primary text
    //   txtD = #707090  dim / placeholder text
    //   red  = #FF4060  error / destructive

    auto hex = [](float r, float g, float b, float a = 1.f) {
        return ImVec4(r, g, b, a);
    };

    c[ImGuiCol_WindowBg]          = hex(0.086f, 0.086f, 0.118f);
    c[ImGuiCol_ChildBg]           = hex(0.114f, 0.114f, 0.153f);
    c[ImGuiCol_PopupBg]           = hex(0.118f, 0.118f, 0.157f);

    c[ImGuiCol_Border]            = hex(0.18f,  0.18f,  0.25f);
    c[ImGuiCol_BorderShadow]      = hex(0.0f,   0.0f,   0.0f,  0.0f);

    c[ImGuiCol_FrameBg]           = hex(0.145f, 0.145f, 0.196f);
    c[ImGuiCol_FrameBgHovered]    = hex(0.18f,  0.18f,  0.24f);
    c[ImGuiCol_FrameBgActive]     = hex(0.2f,   0.2f,   0.27f);

    c[ImGuiCol_TitleBg]           = hex(0.08f,  0.08f,  0.11f);
    c[ImGuiCol_TitleBgActive]     = hex(0.08f,  0.08f,  0.11f);
    c[ImGuiCol_TitleBgCollapsed]  = hex(0.08f,  0.08f,  0.11f);

    c[ImGuiCol_MenuBarBg]         = hex(0.1f,   0.1f,   0.13f);

    c[ImGuiCol_ScrollbarBg]       = hex(0.08f,  0.08f,  0.11f);
    c[ImGuiCol_ScrollbarGrab]     = hex(0.22f,  0.22f,  0.30f);
    c[ImGuiCol_ScrollbarGrabHovered] = hex(0.27f, 0.27f, 0.36f);
    c[ImGuiCol_ScrollbarGrabActive]  = hex(0.0f,  0.83f, 1.0f);

    // Accent: electric cyan
    c[ImGuiCol_CheckMark]         = hex(0.0f,  0.83f,  1.0f);
    c[ImGuiCol_SliderGrab]        = hex(0.0f,  0.83f,  1.0f);
    c[ImGuiCol_SliderGrabActive]  = hex(0.0f,  0.65f,  0.80f);

    c[ImGuiCol_Button]            = hex(0.0f,  0.55f,  0.68f, 0.70f);
    c[ImGuiCol_ButtonHovered]     = hex(0.0f,  0.72f,  0.88f);
    c[ImGuiCol_ButtonActive]      = hex(0.0f,  0.55f,  0.68f);

    c[ImGuiCol_Header]            = hex(0.0f,  0.55f,  0.68f, 0.50f);
    c[ImGuiCol_HeaderHovered]     = hex(0.0f,  0.72f,  0.88f, 0.70f);
    c[ImGuiCol_HeaderActive]      = hex(0.0f,  0.55f,  0.68f);

    c[ImGuiCol_Separator]         = hex(0.20f,  0.20f,  0.28f);
    c[ImGuiCol_SeparatorHovered]  = hex(0.0f,  0.72f,  0.88f);
    c[ImGuiCol_SeparatorActive]   = hex(0.0f,  0.83f,  1.0f);

    c[ImGuiCol_ResizeGrip]        = hex(0.0f,  0.55f,  0.68f, 0.25f);
    c[ImGuiCol_ResizeGripHovered] = hex(0.0f,  0.72f,  0.88f, 0.67f);
    c[ImGuiCol_ResizeGripActive]  = hex(0.0f,  0.83f,  1.0f);

    c[ImGuiCol_Tab]               = hex(0.12f,  0.12f,  0.16f);
    c[ImGuiCol_TabHovered]        = hex(0.0f,  0.72f,  0.88f, 0.80f);
    c[ImGuiCol_TabSelected]       = hex(0.0f,  0.55f,  0.68f);
    c[ImGuiCol_TabDimmed]         = hex(0.09f,  0.09f,  0.12f);
    c[ImGuiCol_TabDimmedSelected] = hex(0.12f,  0.12f,  0.16f);

    c[ImGuiCol_Text]              = hex(0.82f,  0.82f,  0.88f);
    c[ImGuiCol_TextDisabled]      = hex(0.44f,  0.44f,  0.56f);

    c[ImGuiCol_PlotLines]         = hex(0.0f,  0.83f,  1.0f);
    c[ImGuiCol_PlotLinesHovered]  = hex(1.0f,  0.60f,  0.0f);
    c[ImGuiCol_PlotHistogram]     = hex(0.0f,  0.72f,  0.88f);
    c[ImGuiCol_PlotHistogramHovered] = hex(1.0f, 0.60f, 0.0f);

    c[ImGuiCol_TableHeaderBg]     = hex(0.12f,  0.12f,  0.17f);
    c[ImGuiCol_TableBorderStrong] = hex(0.20f,  0.20f,  0.28f);
    c[ImGuiCol_TableBorderLight]  = hex(0.15f,  0.15f,  0.22f);
    c[ImGuiCol_TableRowBg]        = hex(0.0f,   0.0f,   0.0f,  0.0f);
    c[ImGuiCol_TableRowBgAlt]     = hex(1.0f,   1.0f,   1.0f,  0.03f);

    c[ImGuiCol_TextSelectedBg]    = hex(0.0f,  0.55f,  0.68f, 0.40f);
    c[ImGuiCol_DragDropTarget]    = hex(0.0f,  0.83f,  1.0f);

    c[ImGuiCol_NavHighlight]      = hex(0.0f,  0.83f,  1.0f);
    c[ImGuiCol_NavWindowingHighlight] = hex(1.0f, 1.0f, 1.0f, 0.70f);
    c[ImGuiCol_NavWindowingDimBg] = hex(0.80f, 0.80f, 0.80f, 0.20f);
    c[ImGuiCol_ModalWindowDimBg]  = hex(0.0f,  0.0f,  0.0f,  0.55f);
}
