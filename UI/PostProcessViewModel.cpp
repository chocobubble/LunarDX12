#include "PostProcessViewModel.h"
#include "LunarGUI.h"
#include "../PostProcessManager.h"
#include "../Utils/Logger.h"

using namespace std;

namespace Lunar
{

void PostProcessViewModel::Initialize(LunarGui* gui, PostProcessManager* postProcessManager)
{
    if (!gui)
    {
        LOG_ERROR("PostProcessViewModel: Invalid gui pointer");
        return;
    }

    LOG_FUNCTION_ENTRY();
    
    gui->BindCheckbox("Post Process Enabled", &m_showPostProcessWindow, nullptr, "main");
    gui->BindCheckbox("Blur X Enabled", &postProcessManager->m_blurXEnabled, nullptr, "postprocess");
    gui->BindCheckbox("Blur Y Enabled", &postProcessManager->m_blurYEnabled, nullptr, "postprocess");

    vector<string> elementIds = {
        "Blur X Enabled",
        "Blur Y Enabled"
    };

    gui->BindWindow("Post Process Settings", "Post Process Settings", &m_showPostProcessWindow, elementIds);
}

} // namespace Lunar