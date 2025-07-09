#include "SceneViewModel.h"
#include "LunarGUI.h"
#include "../Utils/Logger.h"
#include "../SceneRenderer.h"
#include <DirectXMath.h>

using namespace std;
using namespace DirectX;

namespace Lunar
{

void SceneViewModel::Initialize(LunarGui* gui, SceneRenderer* sceneRenderer)
{
	if (!gui || !sceneRenderer)
	{
		LOG_ERROR("SceneViewModel: Invalid gui or sceneRenderer pointer");
		return;
	}
	gui->BindCheckbox("Scene Settings Enabled", &m_showSceneWindow, nullptr, "main");

	gui->BindCheckbox("Render Wire Frame", &sceneRenderer->m_wireFrameRender, nullptr, "scene");
	
	vector<string> elementIds = {
		"Render Wire Frame"
	};

	gui->BindWindow("Scene Settings", "Scene Settings", &m_showSceneWindow, elementIds);
}

}
