// ============ src/Module.h ============
#pragma once
#include <string>
#include <utility>

class Module
{
public:
    explicit Module(std::string name)
        : m_name(std::move(name))
    {}
    virtual ~Module() = default;

    virtual void Draw() = 0;
    virtual void Reset() {}

    bool IsVisible() const { return m_visible; }
    void SetVisible(bool v) { m_visible = v; }
    const std::string &GetName() const { return m_name; }

protected:
    std::string m_name;
    bool m_visible = true;
};