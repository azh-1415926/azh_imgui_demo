#include "ProductExportModule.h"
#include "UI.h"

#include <OpenXLSX.hpp>
#include <nfd.h>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <iostream>
#include <random>
#include <string>
#include <utility>

using namespace OpenXLSX;

// ============================================================================
//  内嵌模板定义 + 导出实现（不依赖外部 template.xlsx）
// ============================================================================
namespace
{

// -------------------- 调试开关 --------------------
constexpr bool kDebugLog = true;

void debugLog(const std::string &msg)
{
    if (kDebugLog)
        std::cerr << "[XLSX] " << msg << "\n";
}

// -------------------- 表头定义（32 列，顺序与原始模板完全一致）--------------------
// A ~ AF
const char *const kHeaders[] = {
    "<必填>组合商品编码", // A
    "条码",               // B
    "<必填>组合商品名称", // C
    "品牌",               // D
    "分类",               // E
    "标准售价",           // F
    "批发价",             // G
    "参考进价",           // H
    "吊牌价",             // I
    "商品简称",           // J
    "重量(kg)",           // K
    "体积(m³)",           // L
    "图片1",              // M
    "图片2",              // N
    "图片3",              // O
    "图片4",              // P
    "价格分摊",           // Q
    "<必填>子商品编码",   // R
    "子商品名称",         // S
    "<必填>子商品数量",   // T
    "是否进口",           // U
    "是否量贩装",         // V
    "产地",               // W
    "规格",               // X
    "适用人群",           // Y
    "保健用品产品名称",   // Z
    "消字号",             // AA
    "货号",               // AB
    "箱装数量",           // AC
    "产品类别",           // AD
    "功效",               // AE
    "护理部位",           // AF
};
constexpr int kHeaderCount = sizeof(kHeaders) / sizeof(kHeaders[0]);

// -------------------- 需要写值的列号（1 起，A=1）--------------------
constexpr int kColComboCode = 1; // A  组合商品编码
constexpr int kColComboName = 3; // C  组合商品名称
constexpr int kColBrand = 4;     // D  品牌
constexpr int kColCategory = 5;  // E  分类
constexpr int kColWeight = 11;   // K  重量(kg)
constexpr int kColImage1 = 13;   // M  图片1
constexpr int kColSubCode = 18;  // R  子商品编码
constexpr int kColSubName = 19;  // S  子商品名称
constexpr int kColSubQty = 20;   // T  子商品数量

// -------------------- 写表头 --------------------
void writeHeaders(XLWorksheet &sheet)
{
    for (int i = 0; i < kHeaderCount; ++i) {
        sheet.cell(1, i + 1).value() = kHeaders[i];
    }
    debugLog("表头已写入: " + std::to_string(kHeaderCount) + " 列");
}

// -------------------- 组合商品编码 --------------------
std::string generateFirstCombinationCode()
{
    static std::mt19937_64 rng(std::random_device{}());
    std::uniform_int_distribution<int> dist(50000, 59999);
    int n = dist(rng);
    std::string digits = std::to_string(n);
    while (digits.size() < 4)
        digits = "0" + digits;
    return "2c7" + digits;
}

std::string incrementTrailingNumber(const std::string &code, int delta)
{
    size_t end = code.size(), start = end;
    while (start > 0 && std::isdigit((unsigned char) code[start - 1]))
        --start;
    if (start == end)
        return code;
    std::string prefix = code.substr(0, start);
    std::string numStr = code.substr(start);
    long long num = std::stoll(numStr) + delta;
    std::string newNum = std::to_string(num);
    while (newNum.size() < numStr.size())
        newNum = "0" + newNum;
    return prefix + newNum;
}

// -------------------- 主导出（无外部模板）--------------------
bool generateCombinedProductXlsx(
    const std::string &outputPath,
    const std::string &ownerName,
    const std::string &productName,
    const std::string &productCode,
    double productWeight,
    const std::string &imageUrl,
    int rowCount,
    std::string &errorMsg)
{
    if (rowCount <= 0) {
        errorMsg = "导出行数必须 >= 1";
        return false;
    }
    if (rowCount > 100000) {
        errorMsg = "导出行数过大（上限 100000）";
        return false;
    }

    try {
        // 若目标文件已存在，OpenXLSX 的 create() 会抛异常，先删掉
        if (std::filesystem::exists(outputPath)) {
            std::filesystem::remove(outputPath);
        }

        // 1. 新建工作簿（不用外部模板）
        XLDocument doc;
        doc.create(outputPath);
        // 把默认 Sheet1 改名为「组合商品信息」
        doc.workbook().worksheet(1).setName("组合商品信息");
        auto sheet = doc.workbook().worksheet("组合商品信息");

        // 2. 写表头
        writeHeaders(sheet);

        // 3. 逐行静态写入所有值（无公式，无共享公式，无引用不更新）
        std::string baseCode = generateFirstCombinationCode();
        debugLog("基准组合编码 = " + baseCode);

        for (int i = 0; i < rowCount; ++i) {
            const int row = 2 + i; // 表头在第 1 行，数据从第 2 行开始
            const int qty = i + 1; // 子商品数量：1,2,3,...

            const double rowWeight = std::round(productWeight * static_cast<double>(qty) * 1000.0)
                                     / 1000.0;

            // ====== 组合商品编码（A）======
            // 禁用本列写入：注释掉下面两行
            sheet.cell(row, kColComboCode).value() = incrementTrailingNumber(baseCode, i);

            // ====== 组合商品名称（C）= 商品名称*数量 ======
            // 禁用：注释掉下面一行
            sheet.cell(row, kColComboName).value() = productName + "*" + std::to_string(qty);

            // ====== 品牌（D）= 货主 ======
            // 禁用：注释掉下面一行
            sheet.cell(row, kColBrand).value() = ownerName;

            // ====== 分类（E）= 货主（原模板 =D，这里直接静态写值）======
            // 禁用：注释掉下面一行
            sheet.cell(row, kColCategory).value() = ownerName;

            // ====== 重量(kg)（K）= 基础重量 × 数量，3 位小数 ======
            // 禁用：注释掉下面一行
            sheet.cell(row, kColWeight).value() = rowWeight;

            // 可选：强制显示 3 位小数（0.180 / 0.540）。
            // 不需要固定显示就保持注释；若编译不过，把 setFormatCode 换成
            // .numberFormat().formatCode() = "0.000";
            // try {
            //     sheet.cell(row, kColWeight)
            //          .numberFormat().setFormatCode("0.000");
            // } catch (...) {}

            // ====== 图片1（M）= 图片地址（可为空）======
            // 禁用：注释掉下面一行
            sheet.cell(row, kColImage1).value() = imageUrl;

            // ====== 子商品编码（R）= 输入编码 ======
            // 禁用：注释掉下面一行
            sheet.cell(row, kColSubCode).value() = productCode;

            // ====== 子商品名称（S）= 输入名称 ======
            // 禁用：注释掉下面一行
            sheet.cell(row, kColSubName).value() = productName;

            // ====== 子商品数量（T）= 1,2,3,... ======
            // 禁用：注释掉下面一行
            sheet.cell(row, kColSubQty).value() = qty;

            // ====== 其他列（默认留空，按需追加）======
            // 原模板中 F/G/H/I 价格类列是 0，如需写入取消注释：
            // sheet.cell(row, 6).value() = 0;   // F 标准售价
            // sheet.cell(row, 7).value() = 0;   // G 批发价
            // sheet.cell(row, 8).value() = 0;   // H 参考进价
            // sheet.cell(row, 9).value() = 0;   // I 吊牌价

            debugLog(
                "写入 row=" + std::to_string(row) + " qty=" + std::to_string(qty)
                + " weight=" + std::to_string(rowWeight));
        }

        // 4. 保存
        doc.save();
        doc.close();
        debugLog("已保存: " + outputPath);
        return true;
    } catch (const std::exception &e) {
        errorMsg = std::string("导出异常: ") + e.what();
        return false;
    }
}

} // namespace

// ============================================================================
//  ProductExportModule 实现
// ============================================================================

ProductExportModule::ProductExportModule(std::string name)
    : Module(std::move(name))
{}

bool ProductExportModule::IsFormValid() const
{
    return !m_subProductName.empty() && !m_subProductCode.empty() && !m_owner.empty()
           && m_weight > 0.0f && m_rowCount >= 1;
}

void ProductExportModule::Draw()
{
    ImGui::SetNextWindowPos(ImVec2(50, 50), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(460, 340), ImGuiCond_FirstUseEver);

    ImGui::Begin(m_name.c_str());

    DrawForm();

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    const bool valid = IsFormValid();

    if (!valid)
        ImGui::BeginDisabled();
    if (ImGui::Button("导出", ImVec2(160, 0))) {
        HandleExportClick();
    }
    if (!valid)
        ImGui::EndDisabled();

    if (!valid) {
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(1.0f, 0.6f, 0.2f, 1.0f), "请填写全部必填项且重量大于 0、行数 >= 1");
    }

    UI::ShowMessage("导出成功", "文件已成功导出。", &m_showSuccessPopup);
    UI::ShowMessage("导出失败", m_errorMessage.c_str(), &m_showErrorPopup);

    ImGui::End();
}

void ProductExportModule::DrawForm()
{
    ImGui::PushItemWidth(400.0f);

    UI::InputTextRequired("子商品名称", m_subProductName);
    UI::InputTextRequired("子商品编码", m_subProductCode);

    ImGui::InputFloat("重量 (kg)", &m_weight, 0.0f, 0.0f, "%.2f");
    if (m_weight < 0.0f)
        m_weight = 0.0f;

    UI::InputTextRequired("货主", m_owner);
    UI::InputText("图片地址（可选）", m_imageUrl);

    ImGui::InputInt("导出行数", &m_rowCount);
    if (m_rowCount < 1)
        m_rowCount = 1;
    if (m_rowCount > 100000)
        m_rowCount = 100000;

    ImGui::PopItemWidth();
}

void ProductExportModule::HandleExportClick()
{
    if (!IsFormValid())
        return;

    nfdu8char_t *rawPath = nullptr;

    const nfdu8filteritem_t filters[] = {{"Excel 文件", "xlsx"}};
    const std::string defaultName = m_subProductCode + ".xlsx";

    const nfdresult_t result = NFD_SaveDialogU8(&rawPath, filters, 1, nullptr, defaultName.c_str());

    if (result == NFD_CANCEL)
        return;
    if (result != NFD_OKAY) {
        std::fprintf(stderr, "NFD_SaveDialogU8 failed: %s\n", NFD_GetError());
        return;
    }

    std::string savePath = rawPath;
    NFD_FreePathU8(rawPath);

    if (OnExport(savePath)) {
        m_lastSavePath = savePath;
        m_showSuccessPopup = true;
    }
}

bool ProductExportModule::OnExport(const std::string &savePath)
{
    std::string error;
    const bool ok = generateCombinedProductXlsx(
        savePath,
        m_owner,
        m_subProductName,
        m_subProductCode,
        static_cast<double>(m_weight),
        m_imageUrl,
        m_rowCount,
        error);

    if (!ok) {
        m_errorMessage = error.empty() ? "导出失败" : error;
        m_showErrorPopup = true;
    }
    return ok;
}

void ProductExportModule::Reset()
{
    m_subProductName.clear();
    m_subProductCode.clear();
    m_weight = 0.1f;
    m_owner.clear();
    m_imageUrl.clear();
    m_rowCount = 5;

    m_showSuccessPopup = false;
    m_showErrorPopup = false;
    m_errorMessage.clear();
    m_lastSavePath.clear();
}