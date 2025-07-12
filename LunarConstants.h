#pragma once
#include <array>
#include <dxgi1_6.h>
#include <DirectXMath.h>

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

static constexpr UINT BASIC_CONSTANTS_ROOT_PARAMETER_INDEX = 0;
static constexpr UINT OBJECT_CONSTANTS_ROOT_PARAMETER_INDEX = 1;
static constexpr UINT MATERIAL_CONSTANTS_ROOT_PARAMETER_INDEX = 2;
static constexpr UINT TEXTURE_SR_ROOT_PARAMETER_INDEX = 3;
static constexpr UINT PARTICLE_SRV_ROOT_PARAMETER_INDEX = 4;
static constexpr UINT PARTICLE_UAV_ROOT_PARAMETER_INDEX = 5;
static constexpr UINT POST_PROCESS_INPUT_ROOT_PARAMETER_INDEX = 6;
static constexpr UINT POST_PROCESS_OUTPUT_ROOT_PARAMETER_INDEX = 7;

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
static constexpr std::array<TextureInfo, 10> TEXTURE_INFO = {{
    {"wall", "Assets\\Textures\\wall.jpg", FileType::DEFAULT, TextureDimension::TEXTURE2D},
    {"tree1", "Assets\\Textures\\tree1.dds", FileType::DDS, TextureDimension::TEXTURE2D},
    {"tree2", "Assets\\Textures\\tree2.dds", FileType::DDS, TextureDimension::TEXTURE2D},
	{"tile_color", "Assets\\Textures\\metal\\metal-color.jpg", FileType::DEFAULT, TextureDimension::TEXTURE2D},
	{"tile_normal", "Assets\\Textures\\metal\\metal-normal.png", FileType::DEFAULT, TextureDimension::TEXTURE2D},
	{"tile_ao", "Assets\\Textures\\metal\\metal-ao.jpg", FileType::DEFAULT, TextureDimension::TEXTURE2D},
	{"tile_height", "Assets\\Textures\\metal\\metal-height.png", FileType::DEFAULT, TextureDimension::TEXTURE2D},
	{"tile_metallic", "Assets\\Textures\\metal\\metal-metallic.jpg", FileType::DEFAULT, TextureDimension::TEXTURE2D},
	{"tile_roughness", "Assets\\Textures\\metal\\metal-roughness.jpg", FileType::DEFAULT, TextureDimension::TEXTURE2D},
    {"skybox", "Assets\\Textures\\skybox\\skybox", FileType::DEFAULT, TextureDimension::CUBEMAP},
}};

/////////////// Shaders ///////////////
struct ShaderInfo
{
	const char* name;
	const char* path;
	const char* target;
    const char* entryPoint = "main"; // Default entry point
};
static constexpr std::array<ShaderInfo, 20> SHADER_INFO = {{
	{ "basicVS", "Shaders\\BasicVertexShader.hlsl", "vs_5_1" },
	{ "basicPS", "Shaders\\BasicPixelShader.hlsl", "ps_5_1" },
	{ "basicHS", "Shaders\\BasicHullShader.hlsl", "hs_5_1" },
	{ "basicDS", "Shaders\\BasicDomainShader.hlsl", "ds_5_1" },
	{ "billboardVS", "Shaders\\BillboardVertexShader.hlsl", "vs_5_1" },
	{ "billboardGS", "Shaders\\BillboardGeometryShader.hlsl", "gs_5_1" },
	{ "billboardPS", "Shaders\\BillboardPixelShader.hlsl", "ps_5_1" },
	{ "skyBoxVS", "Shaders\\SkyBoxVertexShader.hlsl", "vs_5_1" },
	{ "skyBoxPS", "Shaders\\SkyBoxPixelShader.hlsl", "ps_5_1" },
	{ "shadowMapVS", "Shaders\\ShadowMapVertexShader.hlsl", "vs_5_1" },
	{ "shadowMapPS", "Shaders\\ShadowMapPixelShader.hlsl", "ps_5_1" },
	{ "normalVS", "Shaders\\NormalVertexShader.hlsl", "vs_5_1" },
	{ "normalGS", "Shaders\\NormalGeometryShader.hlsl", "gs_5_1" },
	{ "normalPS", "Shaders\\NormalPixelShader.hlsl", "ps_5_1" },
    { "particlesUpdateCS", "Shaders\\ParticlesComputeShader.hlsl", "cs_5_1" },
    { "particlesVS", "Shaders\\ParticleVertexShader.hlsl", "vs_5_1" },
    { "particlesPS", "Shaders\\ParticlePixelShader.hlsl", "ps_5_1" },
    { "particlesGS", "Shaders\\ParticleGeometryShader.hlsl", "gs_5_1" },
    { "gaussianBlurXCS", "Shaders\\GaussianBlurCS.hlsl", "cs_5_1", "BlurX" },
    { "gaussianBlurYCS", "Shaders\\GaussianBlurCS.hlsl", "cs_5_1", "BlurY" },
}};

/////////////// Debug Flags ///////////////
namespace DebugFlags
{
    enum : uint32_t
    {
        SHOW_NORMALS       = 1 << 0,  // 0x01
        NORMAL_MAP_ENABLED = 1 << 1,  // 0x02
        SHOW_UVS           = 1 << 2,  // 0x04
        PBR_ENABLED        = 1 << 3,  // 0x08
        SHADOWS_ENABLED    = 1 << 4,  // 0x10
        WIREFRAME          = 1 << 5,  // 0x20
        AO_ENABLED         = 1 << 6,  // 0x40
        IBL_ENABLED        = 1 << 7,  // 0x80
        HEIGHT_MAP_ENABLED = 1 << 8,  // 0x100
        METALLIC_MAP_ENABLED = 1 << 9, // 0x200
        ROUGHNESS_MAP_ENABLED = 1 << 10, // 0x400
        ALBEDO_MAP_ENABLED = 1 << 11, // 0x800
    };
}

/////////////// Lights ///////////////
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
	{ LightType::Directional, "SunLight", {1.2f, 1.0f, 0.8f}, {0.57735f, -0.57735f, 0.57735f}, {-3.0f, 3.0f, -3.0f}, 0.0f, 0.0f, 0.0f },
	{ LightType::Point, "RoomLight", {1.0f, 0.9f, 0.7f}, {-0.57735f, -0.57735f, 0.57735f}, {3.0f, 3.0f, -3.0f}, 2.0f, 8.0f, 0.0f},
	{ LightType::Spot, "FlashLight", {1.0f, 1.0f, 0.9f}, {0.0f, -0.707f, -0.707f}, {0.0f, 3.0f, 3.0f}, 1.0f, 10.0f, 16.0f }
}};

// Light Visualization Colors
namespace LightVizColors 
{
    static constexpr DirectX::XMFLOAT4 DIRECTIONAL_LIGHT = {1.0f, 1.0f, 0.0f, 1.0f};    // yellow
    static constexpr DirectX::XMFLOAT4 DIRECTIONAL_ARROW = {1.0f, 0.5f, 0.0f, 1.0f};    // orange 
    static constexpr DirectX::XMFLOAT4 POINT_LIGHT = {1.0f, 0.0f, 0.0f, 1.0f};          // red 
    static constexpr DirectX::XMFLOAT4 SPOT_LIGHT = {0.0f, 0.0f, 1.0f, 1.0f};           // blue 
    static constexpr DirectX::XMFLOAT4 SPOT_ARROW = {0.0f, 0.8f, 1.0f, 1.0f};           // sky-blue 
}


/////////////// PBR Materials ///////////////
struct PBRMaterialPreset
{
	const char* name;
	const DirectX::XMFLOAT3 albedo;
	const float metallic;
	const float roughness;
	const DirectX::XMFLOAT3 F0;
	const float ao;
};

static constexpr std::array<PBRMaterialPreset, 9> PBR_MATERIAL_PRESETS = {{
	// DEFAULT
	{"default", {0.7f, 0.7f, 0.7f}, 0.0f,	0.5f, {0.04f, 0.04f, 0.04f}, 1.0f},

	// Metals
	{"gold",      {1.000f, 0.766f, 0.336f}, 1.0f, 0.1f,  {1.000f, 0.710f, 0.290f}, 1.0f},
	{"silver",    {0.972f, 0.960f, 0.915f}, 1.0f, 0.05f, {0.972f, 0.960f, 0.915f}, 1.0f},
	{"copper",    {0.955f, 0.637f, 0.538f}, 1.0f, 0.15f, {0.955f, 0.637f, 0.538f}, 1.0f},
	{"iron",      {0.560f, 0.570f, 0.580f}, 1.0f, 0.3f,  {0.560f, 0.570f, 0.580f}, 1.0f},
	
	// Dielectrics
	{"plastic",   {0.8f, 0.2f, 0.2f}, 0.0f, 0.5f, {0.04f, 0.04f, 0.04f}, 1.0f},
	{"rubber",    {0.1f, 0.1f, 0.1f}, 0.0f, 0.9f, {0.04f, 0.04f, 0.04f}, 1.0f},
	{"ceramic",   {0.9f, 0.9f, 0.85f}, 0.0f, 0.1f, {0.05f, 0.05f, 0.05f}, 1.0f},
	{"wood",      {0.6f, 0.4f, 0.2f}, 0.0f, 0.8f, {0.04f, 0.04f, 0.04f}, 1.0f}
}};
}	
}
