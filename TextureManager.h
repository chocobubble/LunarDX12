#pragma once
#include <d3d12.h>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>
#include <wrl/client.h>

#include "LunarConstants.h"

namespace Lunar
{

class DescriptorAllocator;
class PipelineStateManager;
	
struct Texture
{
	Microsoft::WRL::ComPtr<ID3D12Resource> Resource;
	Microsoft::WRL::ComPtr<ID3D12Resource> UploadBuffer;
};
	
class TextureManager
{
public:
	void Initialize(ID3D12Device* device, ID3D12GraphicsCommandList* commandList, DescriptorAllocator* descriptorAllocator, PipelineStateManager* pipelineStateManager);

private:
	std::unordered_map<std::string, std::unique_ptr<Texture>> m_textureMap;
	
	void CreateShaderResourceView(const LunarConstants::TextureInfo& textureInfo, DescriptorAllocator* descriptorAllocator, UINT mipLevels = 1);
	
	Microsoft::WRL::ComPtr<ID3D12Resource> LoadTexture(const LunarConstants::TextureInfo& textureInfo, ID3D12Device* device, ID3D12GraphicsCommandList* commandList, const std::string& filename, Microsoft::WRL::ComPtr<ID3D12Resource>& uploadBuffer);
	Microsoft::WRL::ComPtr<ID3D12Resource> CreateTextureResource(
		ID3D12Device*                           device,
		ID3D12GraphicsCommandList*              commandList,
		const D3D12_RESOURCE_DESC&              textureDesc,
		const uint8_t*                          srcData,
		UINT64                                  rowSizeInBytes,
		Microsoft::WRL::ComPtr<ID3D12Resource>& uploadBuffer);
	Microsoft::WRL::ComPtr<ID3D12Resource> CreateCubemapResource(
		ID3D12Device*                           device,
		ID3D12GraphicsCommandList*              commandList,
		const D3D12_RESOURCE_DESC&              textureDesc,
		const std::vector<uint8_t*>&            faceData,
		UINT64                                  rowSizeInBytes,
		Microsoft::WRL::ComPtr<ID3D12Resource>& uploadBuffer);
	
	std::vector<std::vector<float>> EquirectangularToCubemap(float* imageData, UINT width, UINT height);

    Microsoft::WRL::ComPtr<ID3D12Resource> CreateEmptyMapResource(ID3D12Device* device, UINT mapSize, UINT depthOrArraySize, DXGI_FORMAT format, UINT mipLevels = 1);

	void LoadHDRImage(const LunarConstants::TextureInfo& textureInfo, ID3D12Device* device, ID3D12GraphicsCommandList* commandList, DescriptorAllocator* descriptorAllocator, const PipelineStateManager* pipelineStateManager);
};
	
} // namespace Lunar
