// ============ src/modules/ProductExportModule.h ============
#pragma once
#include "Module.h"
#include <string>

class ProductExportModule : public Module {
public:
    explicit ProductExportModule(std::string name = "子商品导出");

    void Draw() override;
    void Reset() override;

protected:
    virtual bool OnExport(const std::string& savePath);

private:
    void DrawForm();
    void HandleExportClick();
    bool IsFormValid() const;

    std::string m_subProductName;
    std::string m_subProductCode;
    float m_weight = 0.0f;
    std::string m_owner;

    bool m_showSuccessPopup = false;
    std::string m_lastSavePath;
};