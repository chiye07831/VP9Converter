#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#endif

#include "imgui.h"
#include "backends/imgui_impl_glfw.h"
#include "backends/imgui_impl_opengl3.h"
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>
#include "gui/MainWindow.h"
#include "core/FFmpegDetector.h"
#include "util/FileDialog.h"
#include <vector>
#include <string>

#ifdef _WIN32
#include <dwmapi.h>
#pragma comment(lib, "dwmapi.lib")
#endif

static MainWindow* g_mainWindow = nullptr;

static void dropCallback(GLFWwindow* window, int count, const char** paths)
{
    (void)window;
    if (!g_mainWindow)
        return;

    std::vector<std::string> files;
    for (int i = 0; i < count; ++i)
    {
        if (paths[i])
            files.push_back(paths[i]);
    }
    if (!files.empty())
        g_mainWindow->addDroppedFiles(files);
}

int main()
{
    if (!glfwInit())
        return 1;

    const char* glsl_version = "#version 130";
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
    GLFWwindow* window = glfwCreateWindow(960, 640, "VP9Converter", nullptr, nullptr);
    if (!window)
    {
        glfwTerminate();
        return 1;
    }

    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);

#ifdef _WIN32
    {
        HWND hwnd = glfwGetWin32Window(window);
        int dark = 1;
        DwmSetWindowAttribute(hwnd, 20 /*DWMWA_USE_IMMERSIVE_DARK_MODE*/, &dark, sizeof(dark));
    }
#endif
    glfwShowWindow(window);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = nullptr;

    ImGui::StyleColorsDark();
    {
        ImGuiStyle& s = ImGui::GetStyle();
        s.WindowRounding = 0.0f;
        s.FrameRounding = 3.0f;
        s.GrabRounding = 3.0f;
        s.ScrollbarRounding = 0.0f;
        s.FramePadding = ImVec2(6, 4);
        s.ItemSpacing = ImVec2(8, 6);
        s.WindowPadding = ImVec2(12, 12);

        ImVec4 bg    = ImVec4(0.176f, 0.176f, 0.176f, 1.00f);
        ImVec4 frame = ImVec4(0.25f,  0.25f,  0.25f,  1.00f);
        ImVec4 hover = ImVec4(0.35f,  0.35f,  0.35f,  1.00f);
        ImVec4 active= ImVec4(0.45f,  0.45f,  0.45f,  1.00f);
        ImVec4 accent= ImVec4(0.50f,  0.72f,  0.93f,  1.00f);
        ImVec4 text  = ImVec4(1.00f,  1.00f,  1.00f,  1.00f);
        ImVec4 dim   = ImVec4(0.50f,  0.50f,  0.50f,  1.00f);
        ImVec4 border= ImVec4(0.12f,  0.12f,  0.12f,  1.00f);

        s.Colors[ImGuiCol_WindowBg]         = bg;
        s.Colors[ImGuiCol_ChildBg]          = bg;
        s.Colors[ImGuiCol_PopupBg]          = ImVec4(0.14f, 0.14f, 0.14f, 0.98f);
        s.Colors[ImGuiCol_Border]           = border;
        s.Colors[ImGuiCol_FrameBg]          = frame;
        s.Colors[ImGuiCol_FrameBgHovered]   = hover;
        s.Colors[ImGuiCol_FrameBgActive]    = active;
        s.Colors[ImGuiCol_TitleBg]          = ImVec4(0.12f, 0.12f, 0.12f, 1.0f);
        s.Colors[ImGuiCol_TitleBgActive]    = ImVec4(0.15f, 0.15f, 0.15f, 1.0f);
        s.Colors[ImGuiCol_MenuBarBg]        = ImVec4(0.14f, 0.14f, 0.14f, 1.0f);
        s.Colors[ImGuiCol_ScrollbarBg]      = ImVec4(0.08f, 0.08f, 0.08f, 1.0f);
        s.Colors[ImGuiCol_ScrollbarGrab]    = frame;
        s.Colors[ImGuiCol_ScrollbarGrabHovered]   = hover;
        s.Colors[ImGuiCol_ScrollbarGrabActive]    = active;
        s.Colors[ImGuiCol_CheckMark]        = accent;
        s.Colors[ImGuiCol_SliderGrab]       = accent;
        s.Colors[ImGuiCol_SliderGrabActive] = ImVec4(0.60f, 0.80f, 0.95f, 1.0f);
        s.Colors[ImGuiCol_Button]           = frame;
        s.Colors[ImGuiCol_ButtonHovered]    = hover;
        s.Colors[ImGuiCol_ButtonActive]     = active;
        s.Colors[ImGuiCol_Header]           = frame;
        s.Colors[ImGuiCol_HeaderHovered]    = hover;
        s.Colors[ImGuiCol_HeaderActive]     = active;
        s.Colors[ImGuiCol_Separator]        = border;
        s.Colors[ImGuiCol_SeparatorHovered] = accent;
        s.Colors[ImGuiCol_SeparatorActive]  = accent;
        s.Colors[ImGuiCol_ResizeGrip]       = frame;
        s.Colors[ImGuiCol_ResizeGripHovered]= hover;
        s.Colors[ImGuiCol_ResizeGripActive] = active;
        s.Colors[ImGuiCol_Tab]              = frame;
        s.Colors[ImGuiCol_TabHovered]       = hover;
        s.Colors[ImGuiCol_TabActive]        = active;
        s.Colors[ImGuiCol_PlotHistogram]    = accent;
        s.Colors[ImGuiCol_Text]             = text;
        s.Colors[ImGuiCol_TextDisabled]     = dim;
    }

    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init(glsl_version);

    bool ffmpegOk = FFmpegDetector::checkFFmpeg();
    FFmpegDetector::checkFFprobe();
    bool wannacriOk = FFmpegDetector::checkWannaCRI();
    MainWindow mainWindow(ffmpegOk, wannacriOk);
    g_mainWindow = &mainWindow;

    glfwSetDropCallback(window, dropCallback);

    while (!glfwWindowShouldClose(window))
    {
        glfwPollEvents();

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        mainWindow.render();

        ImGui::Render();
        int display_w, display_h;
        glfwGetFramebufferSize(window, &display_w, &display_h);
        glViewport(0, 0, display_w, display_h);
        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        glfwSwapBuffers(window);
    }

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    glfwDestroyWindow(window);
    glfwTerminate();

    return 0;
}
