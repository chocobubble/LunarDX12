#include "SceneViewModel.h"
#include "LunarGUI.h"
#include "Logger.h"
#include "SceneRenderer.h"
#include "MaterialManager.h"
#include "LightingSystem.h"
#include <DirectXMath.h>

using namespace std;
using namespace DirectX;

namespace Lunar
{

void SceneViewModel::Initialize(LunarGui* gui, SceneRenderer* sceneRenderer)
{
    m_geometryNames = sceneRenderer->GetGeometryNames();
    m_materialNames = sceneRenderer->GetMaterialManager()->GetMaterialNames();
    m_lights = sceneRenderer->GetLightingSystem()->GetLightIndices();
	gui->BindCheckbox("Scene Settings Enabled", &m_showSceneWindow);
	gui->BindSlider("NormalMapIndex", &(sceneRenderer->m_basicConstants.normalMapIndex), 0.0f, 1.0f);
	gui->BindCheckbox("DrawNormals", &sceneRenderer->m_drawNormals);
	vector<string> elementIds = {
		"NormalMapIndex", 
	};
	gui->BindWindow("Settings", "Settings", &m_showSceneWindow, elementIds);

    // geometry
 //    gui->BindListBox("Geometry", &m_selectedGeometryIndex, &m_geometryNames, 
 //        [this](vector<string>* v) {
	//         m_selectedGeometryName = m_geometryNames[m_selectedGeometryIndex];
 //        });
 //
	// auto callback = std::function<void(XMFLOAT3*)>( [this, sceneRenderer](XMFLOAT3* pos) { sceneRenderer->SetGeometryLocation(m_selectedGeometryName, *pos); } );
 //    gui->BindSlider("Position", &m_geometryLocation, XMFLOAT3{-5.0f, -5.0f, -5.0f}, DirectX::XMFLOAT3{5.0f, 5.0f, 5.0f}, callback);

    // // lights
    // gui->BindListBox("Lights", &m_selectedLightIndex, &m_lights,
    //     [this](std::vector<int>* v) { m_selectedLightName = std::to_string(m_lights[m_selectedLightIndex]); });
    // gui->BindSlider("LightPosition", &m_lightPosition, DirectX::XMFLOAT3{-5.0f, -5.0f, -5.0f}, DirectX::XMFLOAT3{5.0f, 5.0f, 5.0f},
    //     [this, sceneRenderer](DirectX::XMFLOAT3* pos) { sceneRenderer->GetLightingSystem()->SetLightPosition(m_selectedLightName, *pos); });
}

}
