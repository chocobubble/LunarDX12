#pragma once
#include <DirectXMath.h>
#include <string>
#include <vector>

namespace Lunar
{
class LunarGui; 
class SceneRenderer;
class MaterialManager;
class LightingSystem;

class SceneViewModel
{
public:
    SceneViewModel() = default;
    ~SceneViewModel() = default;

    void Initialize(LunarGui* gui, SceneRenderer* sceneRenderer);

private:
    std::string              m_selectedGeometryName = "default";  
    int                      m_selectedGeometryIndex = 0;
    std::vector<std::string> m_geometryNames;
    DirectX::XMFLOAT3        m_geometryLocation = {0.0f, 0.0f, 0.0f};
    std::string              m_selectedMaterialName = "default";
    std::vector<std::string> m_materialNames;
    int                      m_selectedLightIndex = 0;
    std::vector<int>         m_lights;
};
} // namespace Lunar
