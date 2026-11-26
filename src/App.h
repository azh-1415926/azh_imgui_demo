#pragma once
#include <glad/glad.h>
#ifndef GLFW_INCLUDE_NONE
#define GLFW_INCLUDE_NONE
#endif
#include <GLFW/glfw3.h>
#include <imgui.h>

#include "Module.h"
#include <memory>
#include <string>
#include <vector>


class App
{
public:
    bool Init();
    void Run();
    void Shutdown();

    // 供子模块调用的全局服务
    void RequestExit() { m_shouldExit = true; }
    void RequestReset();

private:
    void HandleShortcuts();
    void DrawMainMenuBar();
    void RegisterModule(std::unique_ptr<Module> mod);

    GLFWwindow *m_window = nullptr;
    bool m_shouldExit = false;
    ImFont *m_uiFont = nullptr; // ★ 保留中文字体指针，方便后续 PushFont / 动态字号
    std::vector<std::unique_ptr<Module>> m_modules;
};