#pragma once
#include <dxgi1_6.h>

namespace Lunar 
{
namespace Constants
{
	
static constexpr DXGI_FORMAT SWAP_CHAIN_FORMAT = DXGI_FORMAT_R8G8B8A8_UNORM;
static constexpr UINT SAMPLE_COUNT = 1;  // Not Use MSAA
static constexpr UINT BUFFER_COUNT = 2;

static constexpr float ASPECT_RATIO = 1.78f;
static constexpr float FOV_ANGLE = 45.0f;
static constexpr float NEAR_PLANE = 0.1f;
static constexpr float FAR_PLANE = 100.0f;
static constexpr float MOUSE_SENSITIVITY = 0.005f;
static constexpr float CAMERA_MOVE_SPEED = 1.0f;

static constexpr UINT LIGHT_COUNT = 3;

static constexpr UINT BASIC_CONSTANTS_ROOT_PARAMETER_INDEX = 1;
static constexpr UINT OBJECT_CONSTANTS_ROOT_PARAMETER_INDEX = 2;
static constexpr UINT MATERIAL_CONSTANTS_ROOT_PARAMETER_INDEX = 3;
	
}	
}
