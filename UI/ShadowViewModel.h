#pragma once

namespace Lunar
{
class ShadowManager;
class LunarGui;

class ShadowViewModel
{
public:
	ShadowViewModel() = default;
	~ShadowViewModel() = default;

	void Initialize(LunarGui* gui, ShadowManager* shadowManager);

private:

};
} // namespace Lunar
 