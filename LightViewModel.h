#pragma once
#include <memory>
#include <DirectXMath.h>

namespace Lunar
{

class LunarGui;
class LightingSystem;
class SceneRenderer;

/*
 * Light Control UI Layout:
 *
 *   ┌──────────────────────────────┐
 *   │ Light Control                │
 *   ├──────────────────────────────┤
 *   │ Ambient Light:               │
 *   │ R: [████████] 0.60           │
 *   │ G: [████████] 0.60           │
 *   │ B: [████████] 0.60           │
 *   │                              │
 *   │ ┌─ Directional Light ──────┐ │
 *   │ │ [✓] Enabled              │ │
 *   │ │ Position: X[   ] Y[   ]  │ │
 *   │ │           Z[   ]         │ │
 *   │ │ Direction: X[   ] Y[   ] │ │
 *   │ │            Z[   ]        │ │
 *   │ │ Color: R[   ] G[   ]     │ │
 *   │ │        B[   ]            │ │
 *   │ └──────────────────────────┘ │
 *   │                              │
 *   │ ┌─ Point Light ────────────┐ │
 *   │ │ [✓] Enabled              │ │
 *   │ │ Position: X[   ] Y[   ]  │ │
 *   │ │           Z[   ]         │ │
 *   │ │ Color: R[   ] G[   ]     │ │
 *   │ │        B[   ]            │ │
 *   │ │ Range: [████████] 8.0    │ │
 *   │ └──────────────────────────┘ │
 *   │                              │
 *   │ ┌─ Spot Light ─────────────┐ │
 *   │ │ [✓] Enabled              │ │
 *   │ │ Position: X[   ] Y[   ]  │ │
 *   │ │           Z[   ]         │ │
 *   │ │ Direction: X[   ] Y[   ] │ │
 *   │ │            Z[   ]        │ │
 *   │ │ Color: R[   ] G[   ]     │ │
 *   │ │        B[   ]            │ │
 *   │ │ Range: [████████] 10.0   │ │
 *   │ │ Spot Power: [███] 16.0   │ │
 *   │ └──────────────────────────┘ │
 *   └──────────────────────────────┘
 */

class LightViewModel 
{
public:
    LightViewModel() = default;
    ~LightViewModel() = default;

    void Initialize(LunarGui* gui, LightingSystem* lightingSystem, SceneRenderer* sceneRenderer);

private:
    bool m_showLightWindow = true;
    
    // Ambient Light Controls
    DirectX::XMFLOAT4 m_ambientLight;
    
    // Directional Light Controls
    bool m_directionalEnabled = true;
    DirectX::XMFLOAT3 m_directionalPosition;
    DirectX::XMFLOAT3 m_directionalDirection;
    DirectX::XMFLOAT3 m_directionalColor;
    
    // Point Light Controls
    bool m_pointEnabled = true;
    DirectX::XMFLOAT3 m_pointPosition;
    DirectX::XMFLOAT3 m_pointColor;
    float m_pointRange;
    
    // Spot Light Controls
    bool m_spotEnabled = true;
    DirectX::XMFLOAT3 m_spotPosition;
    DirectX::XMFLOAT3 m_spotDirection;
    DirectX::XMFLOAT3 m_spotColor;
    float m_spotRange;
    float m_spotPower;
    
    LightingSystem* m_lightingSystem = nullptr;
    SceneRenderer* m_sceneRenderer = nullptr;
    
    void UpdateAmbientLight();
    void UpdateDirectionalLight();
    void UpdatePointLight();
    void UpdateSpotLight();
};

} // namespace Lunar
