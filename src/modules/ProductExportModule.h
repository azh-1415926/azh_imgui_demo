#pragma once
#include "Module.h"
#include <string>

class ProductExportModule : public Module
{
public:
    explicit ProductExportModule(std::string name = "子商品导出");

    void Draw() override;
    void Reset() override;

    // 可在外部指定模板路径；默认使用当前目录下的 template.xlsx
    void SetTemplatePath(std::string path) { m_templatePath = std::move(path); }
    const std::string &GetTemplatePath() const { return m_templatePath; }

protected:
    virtual bool OnExport(const std::string &savePath);

private:
    void DrawForm();
    void HandleExportClick();
    bool IsFormValid() const;

    std::string m_subProductName;
    std::string m_subProductCode;
    float m_weight = 0.0f;
    std::string m_owner;
    std::string m_imageUrl; // 图片地址（可为空）
    int m_rowCount = 1;     // 导出行数

    std::string m_templatePath = "template.xlsx";

    bool m_showSuccessPopup = false;
    bool m_showErrorPopup = false;
    std::string m_errorMessage;
    std::string m_lastSavePath;
};