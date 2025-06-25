#include "TextureManager.h"

#include <filesystem>

#include "Logger.h"
#include "Utils.h"

using namespace std;
using namespace Microsoft::WRL;

namespace Lunar
{
	
void TextureManager::Initialize(ID3D12Device* device, ID3D12GraphicsCommandList* commandList)
{
	LOG_FUNCTION_ENTRY();

	std::string wallTexturePath = "Assets\\Textures\\wall.jpg";
	std::string tree1TexturePath = "Assets\\Textures\\tree1.dds";
	std::string tree2TexturePath = "Assets\\Textures\\tree2.dds";
	if (!filesystem::exists(wallTexturePath) || !filesystem::exists(tree1TexturePath) || !filesystem::exists(tree2TexturePath))
	{
		LOG_ERROR("Texture file not found");
		return;
	}

	Texture wall = {};
	wall.Resource = Utils::LoadSimpleTexture(device, commandList, wallTexturePath, wall.UploadBuffer);
	m_textureMap["wall"] = make_unique<Texture>(wall);

	Texture tree1 = {};
	tree1.Resource = Utils::LoadDDSTexture(device, commandList, tree1TexturePath, tree1.UploadBuffer);
	m_textureMap["tree1"] = make_unique<Texture>(tree1);

	Texture tree2 = {};
	tree2.Resource = Utils::LoadDDSTexture(device, commandList, tree2TexturePath, tree2.UploadBuffer);
	m_textureMap["tree2"] = make_unique<Texture>(tree2);
	
	CreateSRVDescriptorHeap(device);
	CreateShaderResourceView(device);
}
	
void TextureManager::CreateSRVDescriptorHeap(ID3D12Device* device)
{
	LOG_FUNCTION_ENTRY();
	D3D12_DESCRIPTOR_HEAP_DESC srvHeapDesc = {};
	srvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	srvHeapDesc.NumDescriptors = static_cast<UINT>(m_textureMap.size());
	srvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	srvHeapDesc.NodeMask = 0;
	THROW_IF_FAILED(device->CreateDescriptorHeap(&srvHeapDesc, IID_PPV_ARGS(m_srvHeap.GetAddressOf())))
}
	
void TextureManager::CreateShaderResourceView(ID3D12Device* device)
{
	LOG_FUNCTION_ENTRY();

	/*
	struct D3D12_SHADER_RESOURCE_VIEW_DESC {
		DXGI_FORMAT Format;                    // Pixel format of the resource data
											  // Use DXGI_FORMAT_UNKNOWN to use resource's original format
											  // Can specify different format for type conversion
	
		D3D12_SRV_DIMENSION ViewDimension;    // Specifies the resource type and how shader will access it
											  // Common values:
											  // - D3D12_SRV_DIMENSION_TEXTURE1D: 1D texture
											  // - D3D12_SRV_DIMENSION_TEXTURE2D: 2D texture
											  // - D3D12_SRV_DIMENSION_TEXTURE3D: 3D texture
											  // - D3D12_SRV_DIMENSION_TEXTURECUBE: Cube map
											  // - D3D12_SRV_DIMENSION_BUFFER: Raw buffer
	
		UINT Shader4ComponentMapping;         // Controls how texture components (RGBA) are mapped to shader
											  // Use D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING for standard RGBA mapping
											  // Can rearrange/swizzle components (e.g., BGRA to RGBA)
	
		union {
			D3D12_BUFFER_SRV Buffer;                    // Used when ViewDimension is BUFFER
			D3D12_TEX1D_SRV Texture1D;                 // Used for 1D textures
			D3D12_TEX1D_ARRAY_SRV Texture1DArray;      // Used for 1D texture arrays
			D3D12_TEX2D_SRV Texture2D;                 // Used for 2D textures (most common)
			D3D12_TEX2D_ARRAY_SRV Texture2DArray;      // Used for 2D texture arrays
			D3D12_TEX2DMS_SRV Texture2DMS;             // Used for multisampled 2D textures
			D3D12_TEX2DMS_ARRAY_SRV Texture2DMSArray;  // Used for multisampled 2D texture arrays
			D3D12_TEX3D_SRV Texture3D;                 // Used for 3D textures
			D3D12_TEXCUBE_SRV TextureCube;              // Used for cube map textures
			D3D12_TEXCUBE_ARRAY_SRV TextureCubeArray;  // Used for cube map texture arrays
		};
	};
	
	// D3D12_TEX2D_SRV structure (most commonly used for 2D textures)
	struct D3D12_TEX2D_SRV {
		UINT MostDetailedMip;        // Index of the most detailed mipmap level to use
									 // 0 = highest resolution level
	
		UINT MipLevels;              // Number of mipmap levels to use
									 // Use -1 to use all available mip levels from MostDetailedMip
									 // Use 1 for single mip level (no mipmapping)
	
		UINT PlaneSlice;             // For planar formats, specifies which plane to access
									 // Use 0 for non-planar formats (most common case)
	
		FLOAT ResourceMinLODClamp;   // Minimum LOD (Level of Detail) clamp value
									 // Prevents shader from sampling below this mip level
									 // Use 0.0f for no clamping (most common)
	};
	*/
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.Texture2D.MipLevels = 1;
	srvDesc.Texture2D.MostDetailedMip = 0;
	srvDesc.Texture2D.PlaneSlice = 0;
	srvDesc.Texture2D.ResourceMinLODClamp = 0.0f;

	m_srvHandle = m_srvHeap->GetCPUDescriptorHandleForHeapStart();
	for (auto& texture : m_textureMap)
	{
		srvDesc.Format = texture.second->Resource->GetDesc().Format; // dds needs BC2_UNORM
		device->CreateShaderResourceView(texture.second->Resource.Get(), &srvDesc, m_srvHandle);
		m_srvHandle.ptr += device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	}
}
	
} //namespace Lunar
