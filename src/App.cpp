#include "App.h"
#include "modules/ProductExportModule.h"

#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include <nfd.h>

#include <cstdio>
#include <filesystem>

namespace {

// 按优先级尝试多个系统字体路径，返回第一个加载成功的字体。
// 全部失败返回 nullptr，由调用方决定回退策略。
ImFont* LoadChineseFont(float sizePx) {
    ImGuiIO& io = ImGui::GetIO();

    // 常见系统中文字体路径（按优先级）
    const char* candidates[] = {
        // Windows
        "C:/Windows/Fonts/msyh.ttc",       // 微软雅黑
        "C:/Windows/Fonts/msyhbd.ttc",     // 微软雅黑 Bold
        "C:/Windows/Fonts/simhei.ttf",     // 黑体
        "C:/Windows/Fonts/simsun.ttc",     // 宋体
        // macOS
        "/System/Library/Fonts/PingFang.ttc",
        // Linux
        "/usr/share/fonts/truetype/wqy/wqy-microhei.ttc",
        "/usr/share/fonts/opentype/noto/NotoSansCJK-Regular.ttc",
        "/usr/share/fonts/truetype/noto/NotoSansCJK-Regular.ttc",
    };

    for (const char* path : candidates) {
        std::error_code ec;
        if (!std::filesystem::exists(path, ec) || ec) continue;

        // SimplifiedCommon ≈ 2500 常用汉字，体积小、启动快。
        // 如需完整 CJK 字形，改为 io.Fonts->GetGlyphRangesChineseFull()，
        // 但 30px 下会显著增大字体纹理与启动时间（可能超过单张 4096² 纹理），
        // 建议同时把字号降到 20~24，或确保 GPU 支持 8192/16384 纹理。
        ImFont* font = io.Fonts->AddFontFromFileTTF(
            path, sizePx, nullptr,
            io.Fonts->GetGlyphRangesChineseSimplifiedCommon());

        if (font) {
            std::fprintf(stderr, "[font] loaded: %s (%.0fpx)\n", path, sizePx);
            return font;
        }
        std::fprintf(stderr, "[font] failed to load: %s\n", path);
    }
    return nullptr;
}

} // namespace

bool App::Init() {
    if (!glfwInit()) {
        std::fprintf(stderr, "glfwInit failed\n");
        return false;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    m_window = glfwCreateWindow(1280, 800, "Tool Framework", nullptr, nullptr);
    if (!m_window) {
        std::fprintf(stderr, "glfwCreateWindow failed\n");
        glfwTerminate();
        return false;
    }
    glfwMakeContextCurrent(m_window);
    glfwSwapInterval(1);

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        std::fprintf(stderr, "gladLoadGLLoader failed\n");
        return false;
    }

    // ---------- ImGui 初始化 ----------
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

    // ★ 中文字体加载：必须在 ImGui_ImplOpenGL3_Init 之前完成，
    //   因为后端 Init 时会把字体 atlas 上传到 GPU。
    m_uiFont = LoadChineseFont(30.0f);
    if (!m_uiFont) {
        std::fprintf(stderr, "[font] no Chinese font found, falling back to default\n");
        m_uiFont = io.Fonts->AddFontDefault();
    }
    // 作为默认字体，保证所有窗口都用它渲染
    io.FontDefault = m_uiFont;

    ImGui_ImplGlfw_InitForOpenGL(m_window, true);
    ImGui_ImplOpenGL3_Init("#version 330");

    // ---------- NFD ----------
    if (NFD_Init() != NFD_OKAY) {
        std::fprintf(stderr, "NFD_Init failed: %s\n", NFD_GetError());
        return false;
    }

    // ★ 脚本式注册子界面：新增模块只需在这里加一行
    RegisterModule(std::make_unique<ProductExportModule>());

    return true;
}

void App::RegisterModule(std::unique_ptr<Module> mod) {
    if (mod) m_modules.push_back(std::move(mod));
}

void App::RequestReset() {
    for (auto& mod : m_modules) mod->Reset();
}

void App::HandleShortcuts() {
    ImGuiIO& io = ImGui::GetIO();

    // 用 IsKeyPressed(..., repeat=false) 只在按下那一帧触发，
    // 避免 glfwGetKey 按住时反复触发的问题。
    if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_Q, false)) {
        RequestExit();
    }
    if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_R, false)) {
        RequestReset();
    }
}

void App::DrawMainMenuBar() {
    if (ImGui::BeginMainMenuBar()) {
        if (ImGui::BeginMenu("文件")) {
            if (ImGui::MenuItem("复位", "Ctrl+R")) RequestReset();
            if (ImGui::MenuItem("退出", "Ctrl+Q")) RequestExit();
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("视图")) {
            for (auto& mod : m_modules) {
                bool visible = mod->IsVisible();
                if (ImGui::MenuItem(mod->GetName().c_str(), nullptr, &visible)) {
                    mod->SetVisible(visible);
                }
            }
            ImGui::EndMenu();
        }
        ImGui::EndMainMenuBar();
    }
}

void App::Run() {
    while (!m_shouldExit && !glfwWindowShouldClose(m_window)) {
        glfwPollEvents();

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        HandleShortcuts();   // 快捷键优先级最高
        DrawMainMenuBar();

        for (auto& mod : m_modules) {
            if (mod->IsVisible()) mod->Draw();
        }

        ImGui::Render();

        int w, h;
        glfwGetFramebufferSize(m_window, &w, &h);
        glViewport(0, 0, w, h);
        glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        glfwSwapBuffers(m_window);
    }
}

void App::Shutdown() {
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    NFD_Quit();

    if (m_window) {
        glfwDestroyWindow(m_window);
        m_window = nullptr;
    }
    glfwTerminate();
}