#pragma once

namespace Lunar
{
class LunarGui;
class PostProcessManager;

class PostProcessViewModel
{
public:
    PostProcessViewModel() = default;
    ~PostProcessViewModel() = default;

    void Initialize(LunarGui* gui, PostProcessManager* postProcessManager);
private:
    bool m_showPostProcessWindow = true;
};

} // namespace Lunar