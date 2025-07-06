#pragma once

#include <memory>

namespace Lunar
{

class LunarGui;
struct BasicConstants;

class DebugViewModel
{
public:
    DebugViewModel() = default;
    ~DebugViewModel() = default;

    void Initialize(LunarGui* gui, BasicConstants& constants);

private:
    bool m_showDebugWindow = false;
    BasicConstants* m_constants = nullptr;
    
    // Debug flags in order
    bool m_showNormals = false;
    bool m_normalMapEnabled = false;
    bool m_showUVs = false;
    bool m_pbrEnabled = true;
    bool m_shadowsEnabled = true;
    bool m_wireframe = false;
    bool m_aoEnabled = false;
    bool m_iblEnabled = false;
    
    void UpdateFlags();
    void SyncFromConstants();
};

} // namespace Lunar
