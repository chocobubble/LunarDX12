#pragma once
#include <d3d12.h>
#include <memory>
#include <string>
#include <unordered_map>
#include <wrl/client.h>

namespace Lunar
{

struct Texture
{
	Microsoft::WRL::ComPtr<ID3D12Resource> Resource;
	Microsoft::WRL::ComPtr<ID3D12Resource> UploadBuffer;
};
	
class TextureManager
{
public:
	void Initialize(ID3D12Device* device, ID3D12GraphicsCommandList* commandList);
	ID3D12DescriptorHeap* GetSRVHeap() { return m_srvHeap.Get(); };

private:
	void CreateSRVDescriptorHeap(ID3D12Device* device);
	void CreateShaderResourceView(ID3D12Device* device);
	std::unordered_map<std::string, std::unique_ptr<Texture>> m_textureMap;
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> m_srvHeap;
	D3D12_CPU_DESCRIPTOR_HANDLE m_srvHandle;
	
};
	
} // namespace Lunar
