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
	float normalMapIndex;
    DirectX::XMFLOAT4   ambientLight;
	LightData lights[Lunar::LunarConstants::LIGHT_COUNT];
	DirectX::XMFLOAT4X4 shadowTransform;
};

// Root Parameter CBV 2
struct ObjectConstants
{
	DirectX::XMFLOAT4X4 World;
    DirectX::XMFLOAT4X4 WorldInvTranspose;
	int textureIndex;
	float dummy[3];
};

// Root Parameter CBV 3
struct MaterialConstants
{
	DirectX::XMFLOAT4 DiffuseAlbedo;
	DirectX::XMFLOAT3 FresnelR0;
	float             Roughness;
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
