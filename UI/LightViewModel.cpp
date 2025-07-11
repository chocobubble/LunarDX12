#include "LightViewModel.h"
#include "LunarGUI.h"
#include "../LightingSystem.h"
#include "../Utils/Logger.h"
#include "../SceneRenderer.h"

using namespace std;
using namespace DirectX;

namespace Lunar
{

void LightViewModel::Initialize(LunarGui* gui, LightingSystem* lightingSystem, SceneRenderer* sceneRenderer)
{
    LOG_FUNCTION_ENTRY();
    
    if (!gui || !lightingSystem || !sceneRenderer)
    {
        LOG_ERROR("LightViewModel: Invalid gui, lightingSystem, or sceneRenderer pointer");
        return;
    }
    
    m_lightingSystem = lightingSystem;
    m_sceneRenderer = sceneRenderer;
    
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

	function<void(float*)> updateAmbientLight = [this](float* f) { UpdateAmbientLight(); };
	function<void(float*)> updateDirectionalLight = [this](float* f) { UpdateDirectionalLight(); };
	function<void(float*)> updatePointLight = [this](float* f) { UpdatePointLight(); };
	function<void(float*)> updateSpotLight = [this](float* f) { UpdateSpotLight(); };
    
    // Ambient Light Controls
    gui->BindSlider("Ambient R", &m_ambientLight.x, 0.0f, 2.0f,  updateAmbientLight);
    gui->BindSlider("Ambient G", &m_ambientLight.y, 0.0f, 2.0f,  updateAmbientLight);
    gui->BindSlider("Ambient B", &m_ambientLight.z, 0.0f, 2.0f, updateAmbientLight);
    
    // Directional Light Controls
    gui->BindCheckbox("Directional Enabled", &m_directionalEnabled);
    gui->BindSlider("Dir Position X", &m_directionalPosition.x, -10.0f, 10.0f, updateDirectionalLight);
    gui->BindSlider("Dir Position Y", &m_directionalPosition.y, -10.0f, 10.0f, updateDirectionalLight);
    gui->BindSlider("Dir Position Z", &m_directionalPosition.z, -10.0f, 10.0f, updateDirectionalLight);
    gui->BindSlider("Dir Direction X", &m_directionalDirection.x, -1.0f, 1.0f, updateDirectionalLight);
    gui->BindSlider("Dir Direction Y", &m_directionalDirection.y, -1.0f, 1.0f, updateDirectionalLight);
    gui->BindSlider("Dir Direction Z", &m_directionalDirection.z, -1.0f, 1.0f, updateDirectionalLight);
    gui->BindSlider("Dir Color R", &m_directionalColor.x, 0.0f, 3.0f, updateDirectionalLight);
    gui->BindSlider("Dir Color G", &m_directionalColor.y, 0.0f, 3.0f, updateDirectionalLight);
    gui->BindSlider("Dir Color B", &m_directionalColor.z, 0.0f, 3.0f, updateDirectionalLight);
    
    // Point Light Controls
    gui->BindCheckbox("Point Enabled", &m_pointEnabled);
    gui->BindSlider("Point Position X", &m_pointPosition.x, -10.0f, 10.0f, updatePointLight);
    gui->BindSlider("Point Position Y", &m_pointPosition.y, -10.0f, 10.0f, updatePointLight);
    gui->BindSlider("Point Position Z", &m_pointPosition.z, -10.0f, 10.0f, updatePointLight);
    gui->BindSlider("Point Color R", &m_pointColor.x, 0.0f, 3.0f, updatePointLight);
    gui->BindSlider("Point Color G", &m_pointColor.y, 0.0f, 3.0f, updatePointLight);
    gui->BindSlider("Point Color B", &m_pointColor.z, 0.0f, 3.0f, updatePointLight);
    gui->BindSlider("Point Range", &m_pointRange, 1.0f, 20.0f, updatePointLight);
    
    // Spot Light Controls
    gui->BindCheckbox("Spot Enabled", &m_spotEnabled);
    gui->BindSlider("Spot Position X", &m_spotPosition.x, -10.0f, 10.0f, updateSpotLight);
    gui->BindSlider("Spot Position Y", &m_spotPosition.y, -10.0f, 10.0f, updateSpotLight);
    gui->BindSlider("Spot Position Z", &m_spotPosition.z, -10.0f, 10.0f, updateSpotLight);
    gui->BindSlider("Spot Direction X", &m_spotDirection.x, -1.0f, 1.0f, updateSpotLight);
    gui->BindSlider("Spot Direction Y", &m_spotDirection.y, -1.0f, 1.0f, updateSpotLight);
    gui->BindSlider("Spot Direction Z", &m_spotDirection.z, -1.0f, 1.0f, updateSpotLight); 
    gui->BindSlider("Spot Color R", &m_spotColor.x, 0.0f, 3.0f, updateSpotLight);
    gui->BindSlider("Spot Color G", &m_spotColor.y, 0.0f, 3.0f, updateSpotLight);
    gui->BindSlider("Spot Color B", &m_spotColor.z, 0.0f, 3.0f, updateSpotLight);
    gui->BindSlider("Spot Range", &m_spotRange, 1.0f, 20.0f, updateSpotLight); 
    gui->BindSlider("Spot Power", &m_spotPower, 1.0f, 64.0f, updateSpotLight);
    
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
    
    if (m_sceneRenderer) {
        m_sceneRenderer->UpdateLightVisualization();
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
    
    if (m_sceneRenderer) {
        m_sceneRenderer->UpdateLightVisualization();
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
    
    if (m_sceneRenderer) {
        m_sceneRenderer->UpdateLightVisualization();
    }
}

} // namespace Lunar
