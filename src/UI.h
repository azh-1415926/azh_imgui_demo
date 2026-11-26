// ============ src/UI.h ============
#pragma once
#include <cstdio>
#include <imgui.h>
#include <string>


namespace UI
{

inline bool InputText(const char *label, std::string &value, ImGuiInputTextFlags flags = 0)
{
    ImGui::PushID(label);
    char buf[512];
    std::snprintf(buf, sizeof(buf), "%s", value.c_str());
    bool changed = ImGui::InputText(label, buf, sizeof(buf), flags);
    if (changed)
        value = buf;
    ImGui::PopID();
    return changed;
}

inline bool InputTextRequired(const char *label, std::string &value)
{
    bool changed = InputText(label, value);
    if (value.empty()) {
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "*必填");
    }
    return changed;
}

inline void ShowMessage(const char *title, const char *message, bool *openFlag)
{
    if (*openFlag) {
        ImGui::OpenPopup(title);
        *openFlag = false;
    }
    if (ImGui::BeginPopupModal(title, nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::TextUnformatted(message);
        ImGui::Spacing();
        if (ImGui::Button("OK", ImVec2(120, 0))) {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
}

} // namespace UI