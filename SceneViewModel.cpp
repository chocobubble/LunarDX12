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
	gui->BindCheckbox("DrawNormals", &sceneRenderer->m_drawNormals);
}

}
