#pragma once
#include <d3d12.h>
#include <DirectXMath.h>
#include <memory>
#include <wrl/client.h>

#include "ConstantBuffers.h"

namespace Lunar
{

class ShadowManager
{
friend class ShadowViewModel;
public:
	void Initialize(ID3D12Device* device);
	void CreateShadowMapTexture(ID3D12Device* device);
	void CreateDSV(ID3D12Device* device, ID3D12DescriptorHeap* dsvHeap);
	void CreateSRV(ID3D12Device* device, ID3D12DescriptorHeap* srvHeap);
	void UpdateShadowCB(const BasicConstants& basicConstants);
	ID3D12Resource* GetShadowTexture() const { return m_shadowTexture.Get(); }
	const D3D12_VIEWPORT& GetViewport() const { return m_viewport; };
	const D3D12_RECT& GetScissorRect() const { return m_scissorRect; }
	const D3D12_CPU_DESCRIPTOR_HANDLE& GetDSVHandle() const { return m_dsvHandle; }
	const ConstantBuffer* GetShadowCB() const { return m_shadowCB.get(); }
	const DirectX::XMFLOAT4X4& GetShadowTransform() const { return m_shadowTransform; }
private:
	Microsoft::WRL::ComPtr<ID3D12Resource> m_shadowTexture;
	UINT m_shadowMapWidth = 1024;
	UINT m_shadowMapHeight = 1024;
	DirectX::XMFLOAT3 m_sceneCenterPosition = { 0.0f, 0.0f, 0.0f };
	float m_sceneRadius = 15.0f;
	DirectX::XMFLOAT4X4 m_shadowTransform;

	D3D12_CPU_DESCRIPTOR_HANDLE m_dsvHandle;

	D3D12_VIEWPORT m_viewport;
	D3D12_RECT m_scissorRect;

	BasicConstants m_basicConstants;
	std::unique_ptr<ConstantBuffer> m_shadowCB;
};
	
} // namespace Lunar
