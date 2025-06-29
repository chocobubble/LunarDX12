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
private:
	Microsoft::WRL::ComPtr<ID3D12Resource> m_shadowTexture;
	UINT m_shadowMapWidth = 1024;
	UINT m_shadowMapHeight = 1024;

	D3D12_VIEWPORT m_viewport;
	D3D12_RECT m_scissorRect;
};
	
} // namespace Lunar
