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
#include <regex>
#include <string>
#include <utility>

using namespace OpenXLSX;

namespace
{

// ==================== 调试开关 ====================
constexpr bool kDebugLog = true;

void debugLog(const std::string &msg)
{
    if (kDebugLog)
        std::cerr << "[XLSX] " << msg << "\n";
}

std::string escapeForLog(const std::string &in)
{
    std::string out;
    for (unsigned char c : in) {
        if (c == '\t')
            out += "\\t";
        else if (c == '\n')
            out += "\\n";
        else if (c == '\r')
            out += "\\r";
        else if (c < 0x20) {
            char b[8];
            std::snprintf(b, sizeof(b), "\\x%02X", c);
            out += b;
        } else
            out += (char) c;
    }
    return out;
}

// ==================== 表头清理 ====================
std::string sanitizeHeader(std::string s)
{
    if (s.size() >= 3 && (unsigned char) s[0] == 0xEF && (unsigned char) s[1] == 0xBB
        && (unsigned char) s[2] == 0xBF)
        s = s.substr(3);

    std::string noFull;
    noFull.reserve(s.size());
    for (size_t i = 0; i + 2 < s.size();) {
        if ((unsigned char) s[i] == 0xE3 && (unsigned char) s[i + 1] == 0x80
            && (unsigned char) s[i + 2] == 0x80)
            i += 3;
        else
            noFull += s[i++];
    }
    s = std::move(noFull);

    auto isSpace = [](unsigned char c) {
        return c == ' ' || c == '\t' || c == '\r' || c == '\n' || c == '\f' || c == '\v';
    };
    size_t a = 0, b = s.size();
    while (a < b && isSpace((unsigned char) s[a]))
        ++a;
    while (b > a && isSpace((unsigned char) s[b - 1]))
        --b;
    return s.substr(a, b - a);
}

// ==================== 多类型读取 ====================
std::string readCellAsString(XLCellAssignable &&cell)
{
    if (cell.empty())
        return {};
    try {
        return cell.value().get<std::string>();
    } catch (...) {
    }
    try {
        return std::string(cell.value().get<std::string_view>());
    } catch (...) {
    }
    try {
        double d = cell.value().get<double>();
        std::string s = std::to_string(d);
        if (s.find('.') != std::string::npos) {
            while (!s.empty() && s.back() == '0')
                s.pop_back();
            if (!s.empty() && s.back() == '.')
                s.pop_back();
        }
        return s;
    } catch (...) {
    }
    try {
        return std::to_string(cell.value().get<int64_t>());
    } catch (...) {
    }
    try {
        return cell.value().get<bool>() ? "TRUE" : "FALSE";
    } catch (...) {
    }
    return {};
}

// ==================== 表头行探测 ====================
int detectHeaderRow(XLWorksheet &sheet, int maxScan = 200)
{
    for (int r = 1; r <= 10; ++r) {
        int n = 0;
        for (int c = 1; c <= maxScan; ++c) {
            auto cell = sheet.cell(r, c);
            if (!cell.empty() && !readCellAsString(std::move(cell)).empty())
                ++n;
        }
        if (n >= 3) {
            debugLog("表头行 = " + std::to_string(r) + " (非空列数 " + std::to_string(n) + ")");
            return r;
        }
    }
    debugLog("未探测到表头行，回退到第 1 行");
    return 1;
}

// ==================== 表头打印 ====================
void logHeaderRow(XLWorksheet &sheet, int headerRow, int maxScan)
{
    if (!kDebugLog)
        return;
    debugLog("===== 表头第 " + std::to_string(headerRow) + " 行 =====");
    for (int c = 1; c <= maxScan; ++c) {
        auto cell = sheet.cell(headerRow, c);
        if (cell.empty())
            continue;
        std::string raw = readCellAsString(std::move(cell));
        if (raw.empty())
            continue;
        std::cerr << "[XLSX]   col " << c << " raw=\"" << escapeForLog(raw) << "\""
                  << " sanitized=\"" << escapeForLog(sanitizeHeader(raw)) << "\"\n";
    }
    debugLog("==========================");
}

// ==================== 列定位 ====================
int findColumnByHeader(
    XLWorksheet &sheet, const std::string &header, int headerRow, int maxScan = 200)
{
    const std::string target = sanitizeHeader(header);
    for (int col = 1; col <= maxScan; ++col) {
        auto cell = sheet.cell(headerRow, col);
        std::string raw = readCellAsString(std::move(cell));
        if (raw.empty())
            continue;
        if (sanitizeHeader(raw) == target) {
            debugLog("定位列: \"" + header + "\" -> col " + std::to_string(col));
            return col;
        }
    }
    std::cerr << "[XLSX] 定位失败: \"" << escapeForLog(header) << "\"\n";
    return -1;
}

// ==================== 编码生成 ====================
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

// ==================== 主导出 ====================
bool generateCombinedProductXlsx(
    const std::string &templatePath,
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
    if (!std::filesystem::exists(templatePath)) {
        errorMsg = "模板不存在: " + templatePath;
        return false;
    }

    try {
        std::filesystem::copy_file(
            templatePath, outputPath, std::filesystem::copy_options::overwrite_existing);

        XLDocument doc;
        doc.open(outputPath);
        auto sheet = doc.workbook().worksheet("组合商品信息");

        // ---- 1. 表头行探测 + 打印 ----
        const int headerRow = detectHeaderRow(sheet, 200);
        logHeaderRow(sheet, headerRow, 200);

        // ---- 2. 按表头文字定位列 ----
        int colComboCode = findColumnByHeader(sheet, "<必填>组合商品编码", headerRow);
        int colComboName = findColumnByHeader(sheet, "<必填>组合商品名称", headerRow);
        int colBrand = findColumnByHeader(sheet, "品牌", headerRow);
        int colCategory = findColumnByHeader(sheet, "分类", headerRow);
        int colWeight = findColumnByHeader(sheet, "重量(kg)", headerRow);
        int colImage1 = findColumnByHeader(sheet, "图片1", headerRow);
        int colSubCode = findColumnByHeader(sheet, "子商品编码", headerRow);
        int colSubName = findColumnByHeader(sheet, "子商品名称", headerRow);
        int colSubQty = findColumnByHeader(sheet, "<必填>子商品数量", headerRow);

        // 只有组合编码是必需的（其他列定位失败会跳过写入，不会崩）
        if (colComboCode < 0) {
            errorMsg = "关键列定位失败: <必填>组合商品编码";
            doc.close();
            return false;
        }

        const int firstDataRow = headerRow + 1;
        const int maxCol = 32; // A ~ AF

        // ---- 3. 探测模板数据行数 ----
        int lastUsedRow = headerRow;
        for (int row = firstDataRow; row <= headerRow + 1000; ++row) {
            bool has = false;
            for (int col = 1; col <= 100; ++col)
                if (!sheet.cell(row, col).empty()) {
                    has = true;
                    break;
                }
            if (has)
                lastUsedRow = row;
        }
        const int templateDataRows = std::max(0, lastUsedRow - headerRow);
        debugLog("模板数据行数 = " + std::to_string(templateDataRows));

        // 由本函数静态覆盖的列集合（扩展/清空时特殊处理）
        auto isOverridden = [&](int col) {
            return col == colComboCode || col == colComboName || col == colBrand
                   || col == colCategory || col == colWeight || col == colImage1
                   || col == colSubCode || col == colSubName || col == colSubQty;
        };

        // ---- 4. 行数 > 模板：扩展行（非覆盖列复制第 2 行值）----
        if (rowCount > templateDataRows) {
            for (int r = headerRow + templateDataRows + 1; r <= headerRow + rowCount; ++r) {
                for (int col = 1; col <= maxCol; ++col) {
                    if (isOverridden(col))
                        continue;
                    auto src = sheet.cell(firstDataRow, col);
                    XLCellValue v = src.value();
                    sheet.cell(r, col).value() = v;
                }
            }
        }

        // ---- 5. 行数 < 模板：清空多余行（值 + 公式）----
        for (int row = firstDataRow + rowCount; row <= headerRow + templateDataRows; ++row) {
            for (int col = 1; col <= maxCol; ++col) {
                auto cell = sheet.cell(row, col);
                try {
                    if (!cell.formula().get().empty())
                        cell.formula() = "";
                } catch (...) {
                }
                if (!cell.empty())
                    cell.value() = "";
            }
        }

        // =========================================================
        // 6. 对所有数据行：静态写入每列最终值
        //    —— 不依赖 Excel 公式重算，跨 Excel/WPS 都一致
        //    —— 每列一个独立 block，注释掉整段即可禁用该列写入
        // =========================================================

        auto clearFormula = [&](int row, int col) {
            if (col < 0)
                return;
            try {
                sheet.cell(row, col).formula() = "";
            } catch (...) {
            }
        };

        std::string baseCode = generateFirstCombinationCode();
        debugLog("基准组合编码 = " + baseCode);

        for (int i = 0; i < rowCount; ++i) {
            const int row = firstDataRow + i;
            const int qty = i + 1; // 子商品数量：1,2,3,...

            // 重量：基础重量 × 数量，四舍五入到 3 位小数
            //   —— 先乘后 round，避免 0.1*3 = 0.30000000000000004
            const double rowWeight = std::round(productWeight * static_cast<double>(qty) * 1000.0)
                                     / 1000.0;

            // ====== 组合商品编码（A）======
            // 禁用：注释掉整个 block
            {
                clearFormula(row, colComboCode);
                sheet.cell(row, colComboCode).value() = incrementTrailingNumber(baseCode, i);
            }

            // ====== 组合商品名称（C）= 商品名称*数量 ======
            if (colComboName >= 0) {
                clearFormula(row, colComboName);
                sheet.cell(row, colComboName).value() = productName + "*" + std::to_string(qty);
            }

            // ====== 品牌（D）= 货主 ======
            if (colBrand >= 0) {
                clearFormula(row, colBrand);
                sheet.cell(row, colBrand).value() = ownerName;
            }

            // ====== 分类（E）= 货主（原模板 =D）======
            // 不想动分类列，注释掉整段
            if (colCategory >= 0) {
                clearFormula(row, colCategory);
                sheet.cell(row, colCategory).value() = ownerName;
            }

            // ====== 重量(kg)（K）= 基础重量 × 数量，3 位小数 ======
            if (colWeight >= 0) {
                clearFormula(row, colWeight);
                sheet.cell(row, colWeight).value() = rowWeight;

                // 可选：强制显示 3 位小数（0.180 / 0.540）。
                // 不需要固定显示就保持注释。若编译不过，把 setFormatCode
                // 换成 numberFormat().formatCode() = "0.000"。
                // try {
                //     sheet.cell(row, colWeight)
                //          .numberFormat().setFormatCode("0.000");
                // } catch (...) {}
            }

            // ====== 图片1（M）= 图片地址（可为空）======
            if (colImage1 >= 0) {
                clearFormula(row, colImage1);
                sheet.cell(row, colImage1).value() = imageUrl;
            }

            // ====== 子商品编码（R）= 输入编码 ======
            if (colSubCode >= 0) {
                clearFormula(row, colSubCode);
                sheet.cell(row, colSubCode).value() = productCode;
            }

            // ====== 子商品名称（S）= 输入名称 ======
            if (colSubName >= 0) {
                clearFormula(row, colSubName);
                sheet.cell(row, colSubName).value() = productName;
            }

            // ====== 子商品数量（T）= 1,2,3,... ======
            if (colSubQty >= 0) {
                clearFormula(row, colSubQty);
                sheet.cell(row, colSubQty).value() = qty;
            }

            debugLog(
                "写入 row=" + std::to_string(row) + " qty=" + std::to_string(qty)
                + " weight=" + std::to_string(rowWeight));
        }

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
    if (ImGui::Button("导出", ImVec2(160, 0)))
        HandleExportClick();
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
    ImGui::SetNextItemWidth(200);
    UI::InputTextRequired("子商品名称", m_subProductName);

    ImGui::SetNextItemWidth(200);
    UI::InputTextRequired("子商品编码", m_subProductCode);

    ImGui::SetNextItemWidth(200);
    ImGui::InputFloat("重量 (kg)", &m_weight, 0.0f, 0.0f, "%.2f");
    if (m_weight < 0.0f)
        m_weight = 0.0f;

    ImGui::SetNextItemWidth(200);
    UI::InputTextRequired("货主", m_owner);

    ImGui::SetNextItemWidth(200);
    UI::InputText("图片地址（可选）", m_imageUrl);

    ImGui::SetNextItemWidth(200);
    ImGui::InputInt("导出行数", &m_rowCount);
    if (m_rowCount < 1)
        m_rowCount = 1;
    if (m_rowCount > 100000)
        m_rowCount = 100000;

    ImGui::Spacing();
    ImGui::TextDisabled("模板：%s", m_templatePath.c_str());
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
        m_templatePath,
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
    m_weight = 0.0f;
    m_owner.clear();
    m_imageUrl.clear();
    m_rowCount = 1;

    m_showSuccessPopup = false;
    m_showErrorPopup = false;
    m_errorMessage.clear();
    m_lastSavePath.clear();
}