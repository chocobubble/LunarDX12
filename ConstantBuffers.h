#pragma once
#include <d3d12.h>
#include <DirectXMath.h>
#include <wrl/client.h>

#include "LightingSystem.h"
#include "LunarConstants.h"

namespace Lunar
{
	
// Root Parameter CBV 1
struct BasicConstants
{
	DirectX::XMFLOAT4X4 view;
	DirectX::XMFLOAT4X4 projection;
	DirectX::XMFLOAT3   eyePos;
	float dummy;
    DirectX::XMFLOAT4   ambientLight;
	LightData lights[Lunar::LunarConstants::LIGHT_COUNT];
	DirectX::XMFLOAT4X4 shadowTransform;
};

// Root Parameter CBV 2
struct ObjectConstants
{
	DirectX::XMFLOAT4X4 World;
	int textureIndex;
	float dummy[3];
};

// Root Parameter CBV 3 - Legacy Material
struct MaterialConstants
{
	DirectX::XMFLOAT4 DiffuseAlbedo;
	DirectX::XMFLOAT3 FresnelR0;
	float             Roughness;
};

// PBR Material Structure
struct PBRMaterial
{
	DirectX::XMFLOAT3 albedo;        // Base color
	float metallic;                  // Metallic factor (0.0 ~ 1.0)
	DirectX::XMFLOAT3 emissive;      // Emissive color
	float roughness;                 // Roughness factor (0.0 ~ 1.0)
	DirectX::XMFLOAT3 F0;            // Fresnel reflectance at normal incidence
	float ao;                        // Ambient occlusion
};

// PBR Material Constants for GPU
struct PBRMaterialConstants
{
	PBRMaterial material;
	int useAlbedoTexture;
	int useNormalTexture;
	int useMetallicTexture;
	int useRoughnessTexture;
};

class ConstantBuffer
{
public:
	ConstantBuffer(ID3D12Device* device, UINT elementByteSize);
	~ConstantBuffer();
	void CopyData(void* data, UINT size);
	ID3D12Resource* GetResource() const { return m_constantBuffer.Get(); }
private:
	Microsoft::WRL::ComPtr<ID3D12Resource> m_constantBuffer;
	BYTE* m_mappedData = nullptr;
};
	
} // namespace Lunar
