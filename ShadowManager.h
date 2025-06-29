#pragma once
#include <d3d12.h>
#include <wrl/client.h>

namespace Lunar
{
	
class ShadowManager
{
public:
	void Initialize(ID3D12Device* device);
	void CreateShadowMapTexture(ID3D12Device* device);
	void CreateDepthStencilView(ID3D12Device* device, Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> dsvHeap);
	void CreateShaderResourceView(ID3D12Device* device, Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> srvHeap);
	ID3D12Resource* GetShadowTexture() const { return m_shadowTexture.Get(); }
	const D3D12_VIEWPORT& GetViewport() const { return m_viewport; };
	const D3D12_RECT& GetScissorRect() const { return m_scissorRect; }	
private:
	Microsoft::WRL::ComPtr<ID3D12Resource> m_shadowTexture;
	UINT m_shadowMapWidth = 1024;
	UINT m_shadowMapHeight = 1024;

	D3D12_VIEWPORT m_viewport;
	D3D12_RECT m_scissorRect;
};
	
} // namespace Lunar
