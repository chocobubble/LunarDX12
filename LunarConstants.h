#pragma once
#include <array>
#include <dxgi1_6.h>

namespace Lunar 
{
namespace LunarConstants
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
	
/////////////// textures  ///////////////
enum class FileType : uint8_t {
    DEFAULT = 0,
    DDS = 1
};

enum class TextureDimension : uint8_t {
    TEXTURE2D = 4,
    CUBEMAP = 9
}; 
struct TextureInfo
{
    const char* name;
    const char* path;
    FileType fileType;
    TextureDimension dimensionType; 
};
static constexpr std::array<TextureInfo, 6> TEXTURE_INFO = {{
    {"wall", "Assets\\Textures\\wall.jpg", FileType::DEFAULT, TextureDimension::TEXTURE2D},
    {"tree1", "Assets\\Textures\\tree1.dds", FileType::DDS, TextureDimension::TEXTURE2D},
    {"tree2", "Assets\\Textures\\tree2.dds", FileType::DDS, TextureDimension::TEXTURE2D},
    {"skybox", "Assets\\Textures\\skybox\\skybox", FileType::DEFAULT, TextureDimension::CUBEMAP},
	{"tile_color", "Assets\\Textures\\tile\\tile-color.png", FileType::DEFAULT, TextureDimension::TEXTURE2D},
	{"tile_normal", "Assets\\Textures\\tile\\tile-normal.png", FileType::DEFAULT, TextureDimension::TEXTURE2D},
}};
struct ShaderInfo
{
	const char* name;
	const char* path;
	const char* target;
};
static constexpr std::array<ShaderInfo, 9> SHADER_INFO = {{
	{ "basicVS",      "Shaders\\BasicVertexShader.hlsl",     "vs_5_0" },
	{ "basicPS",      "Shaders\\BasicPixelShader.hlsl",      "ps_5_0" },
	{ "billboardVS",  "Shaders\\BillboardVertexShader.hlsl", "vs_5_0" },
	{ "billboardGS",  "Shaders\\BillboardGeometryShader.hlsl","gs_5_0" },
	{ "billboardPS",  "Shaders\\BillboardPixelShader.hlsl",   "ps_5_0" },
	{ "skyBoxVS",     "Shaders\\SkyBoxVertexShader.hlsl",     "vs_5_0" },
	{ "skyBoxPS",     "Shaders\\SkyBoxPixelShader.hlsl",      "ps_5_0" },
	{ "shadowMapVS",     "Shaders\\ShadowMapVertexShader.hlsl",     "vs_5_0" },
	{ "shadowMapPS",     "Shaders\\ShadowMapPixelShader.hlsl",      "ps_5_0" }
}};
enum class LightType
{
	Directional,
	Point,
	Spot
};
struct LightInfo
{
	LightType lightType;
	const char* name;
	const float strength[3];
	const float direction[3];
	const float position[3];
	const float fallOffStart;
	const float fallOffEnd;
	const float spotPower;
};
static constexpr std::array<LightInfo, 3> LIGHT_INFO = { {
	{ LightType::Directional, "SunLight", {1.2f, 1.0f, 0.8f}, {0.57735f, -0.57735f, 0.57735f}, {7.0f, 7.0f, 7.0f}, 0.0f, 0.0f, 0.0f },
	{ LightType::Point, "RoomLight", {1.0f, 0.9f, 0.7f},     {0.0f, 0.0f, 0.0f}, {0.0f, 3.0f, 0.0f}, 2.0f, 8.0f, 0.0f},
	{ LightType::Spot, "FlashLight", {1.0f, 1.0f, 0.9f}, {0.0f, -0.5f, 1.0f}, {0.0f, 1.5f, 0.0f}, 1.0f, 10.0f, 16.0f }
}};
}	
}
