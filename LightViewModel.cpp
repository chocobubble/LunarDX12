#include "LightViewModel.h"
#include "LunarGUI.h"
#include "LightingSystem.h"
#include "Logger.h"

using namespace std;
using namespace DirectX;

namespace Lunar
{

void LightViewModel::Initialize(LunarGui* gui, LightingSystem* lightingSystem)
{
    LOG_FUNCTION_ENTRY();
    
    if (!gui || !lightingSystem)
    {
        LOG_ERROR("LightViewModel: Invalid gui or lightingSystem pointer");
        return;
    }
    
    m_lightingSystem = lightingSystem;
    
    auto* dirLight = m_lightingSystem->GetLight("SunLight");
    auto* pointLight = m_lightingSystem->GetLight("RoomLight");
    auto* spotLight = m_lightingSystem->GetLight("FlashLight");
    
    if (dirLight) 
    {
        m_directionalPosition = dirLight->Position;
        m_directionalDirection = dirLight->Direction;
        m_directionalColor = dirLight->Strength;
    }
    
    if (pointLight) 
    {
        m_pointPosition = pointLight->Position;
        m_pointColor = pointLight->Strength;
        m_pointRange = pointLight->FalloffEnd;
    }
    
    if (spotLight) 
    {
        m_spotPosition = spotLight->Position;
        m_spotDirection = spotLight->Direction;
        m_spotColor = spotLight->Strength;
        m_spotRange = spotLight->FalloffEnd;
        m_spotPower = spotLight->SpotPower;
    }
    
    m_ambientLight = m_lightingSystem->GetAmbientLight();
    
    gui->BindCheckbox("Show Light Window", &m_showLightWindow);
    
    // Ambient Light Controls
    gui->BindSliderFloat("Ambient R", &m_ambientLight.x, 0.0f, 2.0f, "%.2f");
    gui->BindSliderFloat("Ambient G", &m_ambientLight.y, 0.0f, 2.0f, "%.2f");
    gui->BindSliderFloat("Ambient B", &m_ambientLight.z, 0.0f, 2.0f, "%.2f");
    gui->BindCallback("Ambient Light Update", [this]() { UpdateAmbientLight(); });
    
    // Directional Light Controls
    gui->BindCheckbox("Directional Enabled", &m_directionalEnabled);
    gui->BindSliderFloat("Dir Position X", &m_directionalPosition.x, -10.0f, 10.0f, "%.2f");
    gui->BindSliderFloat("Dir Position Y", &m_directionalPosition.y, -10.0f, 10.0f, "%.2f");
    gui->BindSliderFloat("Dir Position Z", &m_directionalPosition.z, -10.0f, 10.0f, "%.2f");
    gui->BindSliderFloat("Dir Direction X", &m_directionalDirection.x, -1.0f, 1.0f, "%.3f");
    gui->BindSliderFloat("Dir Direction Y", &m_directionalDirection.y, -1.0f, 1.0f, "%.3f");
    gui->BindSliderFloat("Dir Direction Z", &m_directionalDirection.z, -1.0f, 1.0f, "%.3f");
    gui->BindSliderFloat("Dir Color R", &m_directionalColor.x, 0.0f, 3.0f, "%.2f");
    gui->BindSliderFloat("Dir Color G", &m_directionalColor.y, 0.0f, 3.0f, "%.2f");
    gui->BindSliderFloat("Dir Color B", &m_directionalColor.z, 0.0f, 3.0f, "%.2f");
    gui->BindCallback("Directional Light Update", [this]() { UpdateDirectionalLight(); });
    
    // Point Light Controls
    gui->BindCheckbox("Point Enabled", &m_pointEnabled);
    gui->BindSliderFloat("Point Position X", &m_pointPosition.x, -10.0f, 10.0f, "%.2f");
    gui->BindSliderFloat("Point Position Y", &m_pointPosition.y, -10.0f, 10.0f, "%.2f");
    gui->BindSliderFloat("Point Position Z", &m_pointPosition.z, -10.0f, 10.0f, "%.2f");
    gui->BindSliderFloat("Point Color R", &m_pointColor.x, 0.0f, 3.0f, "%.2f");
    gui->BindSliderFloat("Point Color G", &m_pointColor.y, 0.0f, 3.0f, "%.2f");
    gui->BindSliderFloat("Point Color B", &m_pointColor.z, 0.0f, 3.0f, "%.2f");
    gui->BindSliderFloat("Point Range", &m_pointRange, 1.0f, 20.0f, "%.1f");
    gui->BindCallback("Point Light Update", [this]() { UpdatePointLight(); });
    
    // Spot Light Controls
    gui->BindCheckbox("Spot Enabled", &m_spotEnabled);
    gui->BindSliderFloat("Spot Position X", &m_spotPosition.x, -10.0f, 10.0f, "%.2f");
    gui->BindSliderFloat("Spot Position Y", &m_spotPosition.y, -10.0f, 10.0f, "%.2f");
    gui->BindSliderFloat("Spot Position Z", &m_spotPosition.z, -10.0f, 10.0f, "%.2f");
    gui->BindSliderFloat("Spot Direction X", &m_spotDirection.x, -1.0f, 1.0f, "%.3f");
    gui->BindSliderFloat("Spot Direction Y", &m_spotDirection.y, -1.0f, 1.0f, "%.3f");
    gui->BindSliderFloat("Spot Direction Z", &m_spotDirection.z, -1.0f, 1.0f, "%.3f");
    gui->BindSliderFloat("Spot Color R", &m_spotColor.x, 0.0f, 3.0f, "%.2f");
    gui->BindSliderFloat("Spot Color G", &m_spotColor.y, 0.0f, 3.0f, "%.2f");
    gui->BindSliderFloat("Spot Color B", &m_spotColor.z, 0.0f, 3.0f, "%.2f");
    gui->BindSliderFloat("Spot Range", &m_spotRange, 1.0f, 20.0f, "%.1f");
    gui->BindSliderFloat("Spot Power", &m_spotPower, 1.0f, 64.0f, "%.1f");
    gui->BindCallback("Spot Light Update", [this]() { UpdateSpotLight(); });
    
    vector<string> elementIds = {
        "Ambient R", "Ambient G", "Ambient B", "Ambient Light Update",
        
        "Directional Enabled",
        "Dir Position X", "Dir Position Y", "Dir Position Z",
        "Dir Direction X", "Dir Direction Y", "Dir Direction Z", 
        "Dir Color R", "Dir Color G", "Dir Color B",
        "Directional Light Update",
        
        "Point Enabled",
        "Point Position X", "Point Position Y", "Point Position Z",
        "Point Color R", "Point Color G", "Point Color B",
        "Point Range", "Point Light Update",
        
        "Spot Enabled", 
        "Spot Position X", "Spot Position Y", "Spot Position Z",
        "Spot Direction X", "Spot Direction Y", "Spot Direction Z",
        "Spot Color R", "Spot Color G", "Spot Color B", 
        "Spot Range", "Spot Power", "Spot Light Update"
    };
    
    gui->BindWindow("Light Window", "Light Control", &m_showLightWindow, elementIds);
    
    LOG_FUNCTION_EXIT();
}

void LightViewModel::UpdateAmbientLight()
{
    m_lightingSystem->SetAmbientLight(m_ambientLight);
}

void LightViewModel::UpdateDirectionalLight()
{
    m_lightingSystem->SetLightEnabled("SunLight", m_directionalEnabled);
    
    if (m_directionalEnabled) 
    {
        m_lightingSystem->SetLightPosition("SunLight", m_directionalPosition);
        m_lightingSystem->SetLightDirection("SunLight", m_directionalDirection);
        m_lightingSystem->SetLightColor("SunLight", m_directionalColor);
    }
}

void LightViewModel::UpdatePointLight()
{
    m_lightingSystem->SetLightEnabled("RoomLight", m_pointEnabled);
    
    if (m_pointEnabled) 
    {
        m_lightingSystem->SetLightPosition("RoomLight", m_pointPosition);
        m_lightingSystem->SetLightColor("RoomLight", m_pointColor);
        m_lightingSystem->SetLightRange("RoomLight", m_pointRange);
    }
}

void LightViewModel::UpdateSpotLight()
{
    m_lightingSystem->SetLightEnabled("FlashLight", m_spotEnabled);
    
    if (m_spotEnabled) 
    {
        m_lightingSystem->SetLightPosition("FlashLight", m_spotPosition);
        m_lightingSystem->SetLightDirection("FlashLight", m_spotDirection);
        m_lightingSystem->SetLightColor("FlashLight", m_spotColor);
        m_lightingSystem->SetLightRange("FlashLight", m_spotRange);
        m_lightingSystem->SetLightSpotPower("FlashLight", m_spotPower);
    }
}

} // namespace Lunar
