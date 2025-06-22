#include "SceneViewModel.h"
#include "LunarGUI.h"
#include "Logger.h"
#include "SceneRenderer.h"
#include "MaterialManager.h"
#include "LightingSystem.h"

namespace Lunar
{

void SceneViewModel::Initialize(LunarGui* gui, SceneRenderer* sceneRenderer, MaterialManager* materialManager, LightingSystem* LightingSystem) 
{
    gui = make_unique<LunarGui>();
    m_geometryNames = sceneRenderer->GetGeometryNames();
    m_materialNames = materialManager->GetMaterialNames();
    m_lights = LightingSystem->GetLightIndices();

    // geometry
    gui->BindListBox("Geometry", &m_selectedGeometryIndex, &m_geometryNames, 
        [this]() { m_selectedGeometryName = m_geometryNames[m_selectedGeometryIndex]; });
    gui->BindSlider("Position", &m_geometryLocation, -5.0f, 5.0f, 
        [this]() { sceneRenderer->SetGeometryLocation(m_selectedGeometryName, m_geometryLocation); });

    // lights
    gui->BindListBox("Lights", &m_selectedLightIndex, &m_lights,
        [this]() { m_selectedLightName = m_lights[m_selectedLightIndex]; });
    gui->BindSlider("LightPosition", &m_lightPosition, -5.0f, 5.0f,
        [this]() { LightingSystem->SetLightPosition(m_selectedLightName, m_lightPosition); });
}

}
