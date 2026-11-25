#include "ProductExportModule.h"
#include "UI.h"

#include <nfd.h>
#include <fstream>
#include <utility>

ProductExportModule::ProductExportModule(std::string name)
    : Module(std::move(name)) {}

bool ProductExportModule::IsFormValid() const {
    return !m_subProductName.empty()
        && !m_subProductCode.empty()
        && !m_owner.empty()
        && m_weight > 0.0f;
}

void ProductExportModule::Draw() {
    // 首次出现时避开主菜单栏，之后用户可自由拖动
    ImGui::SetNextWindowPos(ImVec2(50, 50), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(460, 280), ImGuiCond_FirstUseEver);

    ImGui::Begin(m_name.c_str());

    DrawForm();

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    const bool valid = IsFormValid();

    if (!valid) ImGui::BeginDisabled();
    if (ImGui::Button("导出", ImVec2(160, 0))) {
        HandleExportClick();
    }
    if (!valid) ImGui::EndDisabled();

    if (!valid) {
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(1.0f, 0.6f, 0.2f, 1.0f),
                           "请填写全部字段且重量大于 0");
    }

    // 成功提示由 bool 标志驱动，下一帧自动弹出
    UI::ShowMessage("导出成功", "文件已成功导出。", &m_showSuccessPopup);

    ImGui::End();
}

void ProductExportModule::DrawForm() {
    ImGui::SetNextItemWidth(200);
    UI::InputTextRequired("子商品名称", m_subProductName);

    ImGui::SetNextItemWidth(200);
    UI::InputTextRequired("子商品编码", m_subProductCode);

    ImGui::SetNextItemWidth(200);
    ImGui::InputFloat("重量 (kg)", &m_weight, 0.0f, 0.0f, "%.2f");
    if (m_weight < 0.0f) m_weight = 0.0f;

    ImGui::SetNextItemWidth(200);
    UI::InputTextRequired("货主", m_owner);
}

void ProductExportModule::HandleExportClick() {
    // 防御性再校验一次（按钮理论上已被禁用）
    if (!IsFormValid()) return;

    nfdu8char_t* rawPath = nullptr;

    const nfdu8filteritem_t filters[] = { { "CSV 文件", "csv" } };
    const std::string defaultName = m_subProductCode + ".csv";

    // NFD_SaveDialogU8 共 5 个参数：
    //   1. 输出路径
    //   2. 过滤器数组
    //   3. 过滤器数量
    //   4. 默认目录（nullptr = 系统默认）
    //   5. 默认文件名
    const nfdresult_t result = NFD_SaveDialogU8(
        &rawPath,
        filters, 1,
        nullptr,                    // ★ 默认目录，补上这个参数
        defaultName.c_str()
    );

    if (result == NFD_CANCEL) return;   // 用户取消，静默返回
    if (result != NFD_OKAY) {
        std::fprintf(stderr, "NFD_SaveDialogU8 failed: %s\n", NFD_GetError());
        return;
    }

    std::string savePath = rawPath;
    NFD_FreePathU8(rawPath);   // NFD 分配的内存必须由 NFD 释放

    // 执行用户自定义代码（子类可重写 OnExport）
    if (OnExport(savePath)) {
        m_lastSavePath = savePath;
        m_showSuccessPopup = true;  // 下一帧弹成功模态框
    }
}

bool ProductExportModule::OnExport(const std::string& savePath) {
    std::ofstream out(savePath, std::ios::binary);
    if (!out.is_open()) return false;

    out << "sub_product_name,sub_product_code,weight,owner\n";
    out << m_subProductName << ","
        << m_subProductCode << ","
        << m_weight << ","
        << m_owner << "\n";

    return out.good();
}

void ProductExportModule::Reset() {
    m_subProductName.clear();
    m_subProductCode.clear();
    m_weight = 0.0f;
    m_owner.clear();
    m_showSuccessPopup = false;
    m_lastSavePath.clear();
}