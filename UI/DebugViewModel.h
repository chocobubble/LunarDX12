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
    bool m_heightMapEnabled = false;
    bool m_metallicMapEnabled = false;
    bool m_roughnessMapEnabled = false;
    bool m_albedoMapEnabled = false;
    bool m_iblDiffuseEnabled = false;
    bool m_iblSpecularEnabled = false;

    void UpdateFlags();
    void SyncFromConstants();
};

} // namespace Lunar
