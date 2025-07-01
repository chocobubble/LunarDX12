#include "ShadowViewModel.h"
#include "LunarGUI.h"
#include "ShadowManager.h"

namespace Lunar
{
void ShadowViewModel::Initialize(LunarGui* gui, ShadowManager* shadowManager)
{
	gui->BindSlider("SceneRadius", shadowManager->m_sceneRadius, 0.0f, 100.0f);
}
} // namespace Lunar
 