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

	CreateSRVDescriptorHeap(static_cast<UINT> textureInfo.size(), device);

    for (auto& textureInfo : Lunar::Constants::TEXTURE_INFO)
    {
        Texture texture = {};
        texture.Resource = LoadTexture(textureInfo, device, commandList, textureInfo.first, texture.UploadBuffer);
        m_textureMap[textureInfo.name] = make_unique<Texture>(texture);
	    CreateShaderResourceView(textureInfo, device);
    }
}
	
ComPtr<ID3D12Resource> LoadTexture(const TextureInfo& textureInfo, ID3D12Device* device, ID3D12GraphicsCommandList* commandList, const std::string& filename, Microsoft::WRL::ComPtr<ID3D12Resource>& uploadBuffer)
{
	LOG_FUNCTION_ENTRY();

    D3D12_RESOURCE_DESC textureDesc = {};
    textureDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    textureDesc.DepthOrArraySize = 1;
    textureDesc.MipLevels = 1;
    textureDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM; // stbi load with RGBA(4)
    textureDesc.SampleDesc.Count = 1;
    textureDesc.SampleDesc.Quality = 0;
    textureDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
    textureDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

    UINT64 rowSizeInBytes;
    uint8_t* data = nullptr;
    if (textureInfo.fileType == FileType::DEFAULT) 
    {
        int width, height, channels;
        data = stbi_load(filename.c_str(), &width, &height, &channels, 4);
        if (!data)
        {
            LOG_ERROR("Failed to load texture: ", filename);
            throw std::runtime_error("Failed to load simple texture: " + filename);
        }
        LOG_DEBUG("Texture loaded: ", filename, " (", width, "x", height, ", ", channels, " channels)");
        textureDesc.Width = static_cast<UINT>(width);
        textureDesc.Height = static_cast<UINT>(height);
        rowSizeInBytes = UINT64(width) * 4; // RGBA
    }
    else if (textureInfo.fileType == FileType::DDS)
    {
        DirectX::ScratchImage image;
        std::wstring wfilename(filename.begin(), filename.end());
        HRESULT hr = DirectX::LoadFromDDSFile(wfilename.c_str(), DirectX::DDS_FLAGS_NONE, nullptr, image);
        if (FAILED(hr))
        {
            LOG_ERROR("Failed to load DDS texture: ", filename);
            throw std::runtime_error("Failed to load DDS texture: " + filename);
        }
        
        const DirectX::Image* img = image.GetImage(0, 0, 0);
        data = img->pixels;
        LOG_DEBUG("DDS texture loaded: ", filename, " (", img->width, "x", img->height, ")");
        
        textureDesc.Width = static_cast<UINT>(img->width);
        textureDesc.Height = static_cast<UINT>(img->height);
        textureDesc.Format = img->format;
        rowSizeInBytes = img->rowPitch;
    }

    if (textureInfo.dimensionType == TextureDimension::CUBEMAP)
    {
        textureDesc.DepthOrArraySize = 6;
    }

	ComPtr<ID3D12Resource> texture = CreateTextureResource(device, commandList, textureDesc, data, rowSizeInBytes, uploadBuffer);
	if (textureInfo.fileType == FileType::DEFAULT) stbi_image_free(data);
    return texture;
}

void TextureManager::CreateSRVDescriptorHeap(UINT textureNums, ID3D12Device* device)
{
	LOG_FUNCTION_ENTRY();
	D3D12_DESCRIPTOR_HEAP_DESC srvHeapDesc = {};
	srvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	srvHeapDesc.NumDescriptors = static_cast<UINT>(textureNums);
	srvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	srvHeapDesc.NodeMask = 0;
	THROW_IF_FAILED(device->CreateDescriptorHeap(&srvHeapDesc, IID_PPV_ARGS(m_srvHeap.GetAddressOf())))
	m_srvHandle = m_srvHeap->GetCPUDescriptorHandleForHeapStart();
}
	
void TextureManager::CreateShaderResourceView(const TextureInfo& textureInfo, ID3D12Device* device)
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
	srvDesc.ViewDimension = textureInfo.dimensionType;
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.Texture2D.MipLevels = 1;
	srvDesc.Texture2D.MostDetailedMip = 0;
	srvDesc.Texture2D.PlaneSlice = 0;
	srvDesc.Texture2D.ResourceMinLODClamp = 0.0f;

	Texture* texture = m_textureMap[textureInfo.name].get();
	srvDesc.Format = texture->Resource->GetDesc().Format;
	device->CreateShaderResourceView(texture->Resource.Get(), &srvDesc, m_srvHandle);
	m_srvHandle.ptr += device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
}

ComPtr<ID3D12Resource> TextureManager::CreateTextureResource(ID3D12Device* device, ID3D12GraphicsCommandList* commandList, const D3D12_RESOURCE_DESC& textureDesc, const uint8_t* srcData, UINT64 rowSizeInBytes, Microsoft::WRL::ComPtr<ID3D12Resource>& uploadBuffer)
{
    D3D12_HEAP_PROPERTIES defaultHeapProperties = {};
    defaultHeapProperties.Type = D3D12_HEAP_TYPE_DEFAULT;
	defaultHeapProperties.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
	defaultHeapProperties.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
	defaultHeapProperties.CreationNodeMask = 1;
	defaultHeapProperties.VisibleNodeMask = 1;

	ComPtr<ID3D12Resource> texture;

    THROW_IF_FAILED(device->CreateCommittedResource(
        &defaultHeapProperties, 
        D3D12_HEAP_FLAG_NONE, 
        &textureDesc, 
        D3D12_RESOURCE_STATE_COPY_DEST, 
        nullptr, IID_PPV_ARGS(&texture)))

    // calculate the size of the upload buffer
    UINT64 uploadBufferSize;
    /*
    struct D3D12_PLACED_SUBRESOURCE_FOOTPRINT {
        UINT64 Offset;                          
        D3D12_SUBRESOURCE_FOOTPRINT Footprint; 
    };

    struct D3D12_SUBRESOURCE_FOOTPRINT {
        DXGI_FORMAT Format;     // pixel format
        UINT Width;             
        UINT Height;            
        UINT Depth;             
        UINT RowPitch;          // byte size of a row (include padding)
    };
    */
    D3D12_PLACED_SUBRESOURCE_FOOTPRINT layouts;
    UINT numRows;
    UINT64 rowPitch;
    
    // Get GPU memory layout requirements for the texture
    // This calculates proper alignment and padding needed for GPU memory
    device->GetCopyableFootprints(
        &textureDesc,           // Input: texture description
        0,                      // First subresource index
        1,                      // Number of subresources
        0,                      // Base offset in upload buffer
        &layouts,               // Output: memory layout with alignment
        &numRows,               // Output: number of rows in the texture
        &rowPitch,        // Output: actual bytes per row (without padding)
        &uploadBufferSize       // Output: total bytes needed for upload buffer
    );

    // Results explanation:
    // - layouts.Offset: starting offset in upload buffer (usually 0)
    // - layouts.Footprint.RowPitch: bytes per row including GPU alignment padding
    // - layouts.Footprint.Width/Height: texture dimensions
    // - numRows: total number of rows to copy (equals texture height)
    // - rowSizeInBytes: actual data size per row (width * bytes_per_pixel)
    // - uploadBufferSize: total memory needed for upload buffer with alignment

	D3D12_HEAP_PROPERTIES uploadHeapProperties = defaultHeapProperties;
    uploadHeapProperties.Type = D3D12_HEAP_TYPE_UPLOAD;

    D3D12_RESOURCE_DESC uploadBufferDesc = {};
    uploadBufferDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    uploadBufferDesc.Width = uploadBufferSize;
    uploadBufferDesc.Height = 1;
    uploadBufferDesc.DepthOrArraySize = 1;
    uploadBufferDesc.MipLevels = 1;
    uploadBufferDesc.Format = DXGI_FORMAT_UNKNOWN;
    uploadBufferDesc.SampleDesc.Count = 1;
    uploadBufferDesc.SampleDesc.Quality = 0;
    uploadBufferDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    uploadBufferDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

    THROW_IF_FAILED(device->CreateCommittedResource(
        &uploadHeapProperties,
        D3D12_HEAP_FLAG_NONE,
        &uploadBufferDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr, IID_PPV_ARGS(&uploadBuffer)))

    // copy data
    void* mappedData;
    uploadBuffer->Map(0, nullptr, &mappedData);
    BYTE* destSliceStart = reinterpret_cast<BYTE*>(mappedData) + layouts.Offset;
    for (UINT i = 0; i < numRows; i++)
    {
        memcpy(
            destSliceStart + layouts.Footprint.RowPitch * i, 
            srcData + rowSizeInBytes * i, 
            rowSizeInBytes); 
    }
    uploadBuffer->Unmap(0, nullptr);

    /*
    struct D3D12_TEXTURE_COPY_LOCATION {
		ID3D12Resource* pResource;                  // Resource pointer
		D3D12_TEXTURE_COPY_TYPE Type;              // Copy type
		union {
			D3D12_PLACED_SUBRESOURCE_FOOTPRINT PlacedFootprint;  // For buffers
			UINT SubresourceIndex;                               // For textures
		};
	};
	*/
    D3D12_TEXTURE_COPY_LOCATION destLocation = {};
    destLocation.pResource = texture.Get();
    destLocation.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
    destLocation.SubresourceIndex = 0;

    D3D12_TEXTURE_COPY_LOCATION srcLocation = {};
    srcLocation.pResource = uploadBuffer.Get();
    srcLocation.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
    srcLocation.PlacedFootprint = layouts;

    /*
    void ID3D12GraphicsCommandList::CopyTextureRegion(
        const D3D12_TEXTURE_COPY_LOCATION* pDst,    // Destination location
        UINT DstX,                                  // Destination X offset
        UINT DstY,                                  // Destination Y offset
        UINT DstZ,                                  // Destination Z offset
        const D3D12_TEXTURE_COPY_LOCATION* pSrc,   // Source location
        const D3D12_BOX* pSrcBox                   // Source region (optional)
    ); 
    */
    commandList->CopyTextureRegion(&destLocation, 0, 0, 0, &srcLocation, nullptr);

    D3D12_RESOURCE_BARRIER barrier = {};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
    barrier.Transition.pResource = texture.Get();
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

    commandList->ResourceBarrier(1, &barrier);

	return texture;
}
} //namespace Lunar
