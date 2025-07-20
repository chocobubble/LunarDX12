#include "TextureManager.h"

#include <DirectXTex.h>
#include <DirectXMath.h>
#include <DirectXPackedVector.h>
#include <filesystem>
#include <stb_image.h>
#include <random>
#include <cmath>

#include "AsyncTextureLoader.h"
#include "DescriptorAllocator.h"
#include "PipelineStateManager.h"
#include "Utils/IBLUtils.h"
#include "Utils/Logger.h"
#include "Utils/Utils.h"

using namespace std;
using namespace Microsoft::WRL;
using namespace DirectX;

namespace Lunar
{
	
void TextureManager::Initialize(ID3D12Device* device, ID3D12GraphicsCommandList* commandList, DescriptorAllocator* descriptorAllocator, PipelineStateManager* pipelineStateManager)
{
	LOG_FUNCTION_ENTRY();

    for (auto& textureInfo : Lunar::LunarConstants::TEXTURE_INFO)
    {
    	if (textureInfo.fileType == LunarConstants::FileType::HDR)
    	{
    		LoadHDRImage(textureInfo, device, commandList, descriptorAllocator, pipelineStateManager);
    		continue;
    	}
        Texture texture = {};
        texture.Resource = LoadTexture(textureInfo, device, commandList, textureInfo.path, texture.UploadBuffer);
        m_textureMap[textureInfo.name] = make_unique<Texture>(texture);
	    CreateShaderResourceView(textureInfo, descriptorAllocator);
    }
}

void TextureManager::CreateShaderResourceView(const LunarConstants::TextureInfo& textureInfo, DescriptorAllocator* descriptorAllocator, UINT mipLevels)
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
	srvDesc.ViewDimension = static_cast<D3D12_SRV_DIMENSION>(textureInfo.dimensionType);
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	
	if (textureInfo.dimensionType == LunarConstants::TextureDimension::CUBEMAP) {
		srvDesc.TextureCube.MipLevels = mipLevels;
		srvDesc.TextureCube.MostDetailedMip = 0;
		srvDesc.TextureCube.ResourceMinLODClamp = 0.0f;
	} else {
		srvDesc.Texture2D.MipLevels = mipLevels;
		srvDesc.Texture2D.MostDetailedMip = 0;
		srvDesc.Texture2D.PlaneSlice = 0;
		srvDesc.Texture2D.ResourceMinLODClamp = 0.0f;
	}

	Texture* texture = m_textureMap[textureInfo.name].get();
	if (!texture || !texture->Resource) {
		LOG_ERROR("Failed to create texture resource for: ", textureInfo.name);
		return;
	}
	srvDesc.Format = texture->Resource->GetDesc().Format;
	// descriptorAllocator->AllocateDescriptor(textureInfo.name);
	descriptorAllocator->CreateSRV(LunarConstants::RangeType::BASIC_TEXTURES, textureInfo.name, texture->Resource.Get(), &srvDesc);
}

ComPtr<ID3D12Resource> TextureManager::LoadTexture(const LunarConstants::TextureInfo& textureInfo, ID3D12Device* device, ID3D12GraphicsCommandList* commandList, const std::string& filename, ComPtr<ID3D12Resource>& uploadBuffer)
{
    // REFACTOR: load dds, hdr, cubemap, simple texture 
    
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
	
    if (textureInfo.fileType == LunarConstants::FileType::DEFAULT) 
    {
    	if (textureInfo.dimensionType == LunarConstants::TextureDimension::CUBEMAP)
    	{
    		textureDesc.DepthOrArraySize = 6;
        
    		std::vector<std::string> faceFiles = {
    			filename + "_right.jpg",   // +X
				filename + "_left.jpg",    // -X  
				filename + "_top.jpg",     // +Y
				filename + "_bottom.jpg",  // -Y
				filename + "_front.jpg",   // +Z
				filename + "_back.jpg"     // -Z
			};
        
    		std::vector<uint8_t*> faceData(6);
    		int width, height, channels;
        
    		for (int face = 0; face < 6; ++face) {
    			faceData[face] = stbi_load(faceFiles[face].c_str(), &width, &height, &channels, 4);
    			if (!faceData[face]) {
    				LOG_ERROR("Failed to load cubemap face: ", faceFiles[face]);
    				for (int i = 0; i < face; ++i) {
    					stbi_image_free(faceData[i]);
    				}
    				throw std::runtime_error("Failed to load cubemap face: " + faceFiles[face]);
    			}
    		}
        
    		LOG_DEBUG("Cubemap loaded: ", filename, " (", width, "x", height, ")");
    		textureDesc.Width = static_cast<UINT>(width);
    		textureDesc.Height = static_cast<UINT>(height);
    		rowSizeInBytes = UINT64(width) * 4; // RGBA
        
    		ComPtr<ID3D12Resource> texture = CreateCubemapResource(device, commandList, textureDesc, faceData, rowSizeInBytes, uploadBuffer);
        
    		for (int i = 0; i < 6; ++i) {
    			stbi_image_free(faceData[i]);
    		}
        
    		return texture;
    	}
    
        uint8_t* data = nullptr;
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

        ComPtr<ID3D12Resource> texture = CreateTextureResource(device, commandList, textureDesc, data, rowSizeInBytes, uploadBuffer);
        stbi_image_free(data);
        return texture;
    }
    else if (textureInfo.fileType == LunarConstants::FileType::DDS)
    {
        uint8_t* data = nullptr;
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
        
        ComPtr<ID3D12Resource> texture = CreateTextureResource(device, commandList, textureDesc, data, rowSizeInBytes, uploadBuffer);
        return texture;
    }
    else 
    {
        LOG_ERROR("Unsupported texture file type: ", static_cast<int>(textureInfo.fileType));
        throw std::runtime_error("Unsupported texture file type: " + std::to_string(static_cast<int>(textureInfo.fileType)));
    }
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

ComPtr<ID3D12Resource> TextureManager::CreateCubemapResource(
    ID3D12Device* device, 
    ID3D12GraphicsCommandList* commandList, 
    const D3D12_RESOURCE_DESC& textureDesc, 
    const std::vector<uint8_t*>& faceData, 
    UINT64 rowSizeInBytes, 
    ComPtr<ID3D12Resource>& uploadBuffer)
{
    LOG_FUNCTION_ENTRY();
    
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
        nullptr, IID_PPV_ARGS(&texture)));

    // calculate the layout of the 6 subresources
    std::vector<D3D12_PLACED_SUBRESOURCE_FOOTPRINT> layouts(6);
    std::vector<UINT> numRows(6);
    std::vector<UINT64> rowPitch(6);
    UINT64 totalUploadBufferSize = 0;
    
    for (int face = 0; face < 6; ++face)
    {
        UINT64 faceUploadSize;
        device->GetCopyableFootprints(
            &textureDesc,           
            face,                   
            1,                      
            totalUploadBufferSize,  
            &layouts[face],         
            &numRows[face],         
            &rowPitch[face],        
            &faceUploadSize         
        );
        totalUploadBufferSize += faceUploadSize;
    }

    D3D12_HEAP_PROPERTIES uploadHeapProperties = defaultHeapProperties;
    uploadHeapProperties.Type = D3D12_HEAP_TYPE_UPLOAD;

    D3D12_RESOURCE_DESC uploadBufferDesc = {};
    uploadBufferDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    uploadBufferDesc.Width = totalUploadBufferSize;
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
        nullptr, IID_PPV_ARGS(&uploadBuffer)));
    
    void* mappedData;
    uploadBuffer->Map(0, nullptr, &mappedData);
    
    for (int face = 0; face < 6; ++face)
    {
        BYTE* destSliceStart = reinterpret_cast<BYTE*>(mappedData) + layouts[face].Offset;
        for (UINT row = 0; row < numRows[face]; ++row)
        {
            memcpy(
                destSliceStart + layouts[face].Footprint.RowPitch * row, 
                faceData[face] + rowSizeInBytes * row, 
                rowSizeInBytes
            ); 
        }
    }
    uploadBuffer->Unmap(0, nullptr);

    for (int face = 0; face < 6; ++face)
    {
        D3D12_TEXTURE_COPY_LOCATION destLocation = {};
        destLocation.pResource = texture.Get();
        destLocation.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
        destLocation.SubresourceIndex = face;  

        D3D12_TEXTURE_COPY_LOCATION srcLocation = {};
        srcLocation.pResource = uploadBuffer.Get();
        srcLocation.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
        srcLocation.PlacedFootprint = layouts[face];  

        commandList->CopyTextureRegion(&destLocation, 0, 0, 0, &srcLocation, nullptr);
    }

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

vector<vector<float>> TextureManager::EquirectangularToCubemap(float* imageData, UINT width, UINT height)
{
    UINT cubemapSize = max(width / 4, height / 2);

    vector<vector<float>> faceData(6);

    for (int face = 0; face < 6; ++face)
    {
        LOG_DEBUG("Processing cubemap face: ", face);
        faceData[face].resize(cubemapSize * cubemapSize * 3); // 3 channels RGB

        for (int y = 0; y < cubemapSize; ++y)
        {
            for (int x = 0; x < cubemapSize; ++x)
            {
                // Convert cubemap coordinates to direction 
                float u = ((x + 0.5f) * 2.0f) / cubemapSize - 1.0f; // [-1, 1]
                float v = ((y + 0.5f) * 2.0f) / cubemapSize - 1.0f; 

                float direction[3];
                switch (face) {
                	case 0: // +X
                		direction[0] = 1.0f;
						direction[1] = -v;
						direction[2] = -u;
						break;
                	case 1: // -X
                		direction[0] = -1.0f;
						direction[1] = -v;
						direction[2] = u;
						break;
                	case 2: // +Y
                		direction[0] = u;
						direction[1] = 1.0f;
						direction[2] = v;
						break;
                	case 3: // -Y
                		direction[0] = u;
						direction[1] = -1.0f;
						direction[2] = -v;
						break;
                	case 4: // +Z
                		direction[0] = u;
						direction[1] = -v;
						direction[2] = 1.0f;
						break;
                	case 5: // -Z
                		direction[0] = -u;
						direction[1] = -v;
						direction[2] = -1.0f;
						break;
                    default:
                        LOG_ERROR("Invalid cubemap face index: ", face);
                        continue;
                }
                // Normalize direction vector
                float length = sqrtf(direction[0] * direction[0] + direction[1] * direction[1] + direction[2] * direction[2]);
                if (length == 0.0f)
                {
                    LOG_ERROR("Zero length direction vector for cubemap face: ", face);
                    continue;
                }
                direction[0] /= length;
                direction[1] /= length;
                direction[2] /= length;

                float M_PI = 3.14159;
                float theta = atan2f(direction[2], direction[0]); // azimuth
                float phi = asinf(direction[1]); // elevation
                float equiU = (theta + M_PI) / (2.0f * M_PI); 
                // float equiV = (phi + M_PI / 2.0f) / M_PI;
            	float equiV = 0.5 - phi / M_PI; 

                int equiX = static_cast<int>(equiU * width);
                int equiY = static_cast<int>(equiV * height);
                equiX = std::clamp(equiX, 0, static_cast<int>(width - 1));
                equiY = std::clamp(equiY, 0, static_cast<int>(height - 1));

                // data sampling
                int srcIndex = (equiY * width + equiX) * 3; // 3 : channels RGB
                int dstIndex = (y * cubemapSize + x) * 3; // 3 : channels RGB

                faceData[face][dstIndex] = imageData[srcIndex];     // R
                faceData[face][dstIndex + 1] = imageData[srcIndex + 1]; // G
                faceData[face][dstIndex + 2] = imageData[srcIndex + 2]; // B
            }
        }
    }
    
    return faceData;
}

ComPtr<ID3D12Resource> TextureManager::CreateEmptyMapResource(ID3D12Device* device, UINT mapSize, UINT depthOrArraySize, DXGI_FORMAT format, UINT mipLevels)
{
    LOG_FUNCTION_ENTRY();
    
    D3D12_RESOURCE_DESC resourceDesc = {};
    resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    resourceDesc.Alignment = 0;
    resourceDesc.Width = mapSize;
    resourceDesc.Height = mapSize;
    resourceDesc.DepthOrArraySize = depthOrArraySize; 
    resourceDesc.MipLevels = mipLevels;
    resourceDesc.Format = format;
    resourceDesc.SampleDesc.Count = 1;
    resourceDesc.SampleDesc.Quality = 0;
    resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
    resourceDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

    D3D12_HEAP_PROPERTIES defaultHeapProperties = {};
    defaultHeapProperties.Type = D3D12_HEAP_TYPE_DEFAULT;
    defaultHeapProperties.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
    defaultHeapProperties.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
    defaultHeapProperties.CreationNodeMask = 1;
    defaultHeapProperties.VisibleNodeMask = 1;

    ComPtr<ID3D12Resource> emptyMap;
    
    THROW_IF_FAILED(device->CreateCommittedResource(
        &defaultHeapProperties, 
        D3D12_HEAP_FLAG_NONE, 
        &resourceDesc, 
        D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, 
        nullptr, IID_PPV_ARGS(&emptyMap)));
    
    LOG_DEBUG("Empty map created: ", mapSize, "x", mapSize, " with ", mipLevels, " mip levels");
    return emptyMap;
}

void TextureManager::LoadHDRImage(const LunarConstants::TextureInfo& textureInfo, ID3D12Device* device, ID3D12GraphicsCommandList* commandList, DescriptorAllocator* descriptorAllocator, const PipelineStateManager* pipelineStateManager)
{
	// 1. Load HDR Image and save as a cubemap
	// 2. Create an empty cubemap resource for irradiance map
	// 3. Calculate the irradiance map and copy to the resource
	
	string filename = textureInfo.path;
	
    if (stbi_is_hdr(filename.c_str()) == 0)
    {
        LOG_ERROR("File is not a valid HDR texture: ", filename);
        throw std::runtime_error("File is not a valid HDR texture: " + filename);
    }

    int width, height, channels;
    float* data = stbi_loadf(filename.c_str(), &width, &height, &channels, 3); // loadf : float
    if (!data) 
    {
        LOG_ERROR("Failed to load HDR texture: ", filename);
        throw std::runtime_error("Failed to load HDR texture: " + filename);
    }
	
    LOG_DEBUG("HDR texture loaded: ", filename, " (", width, "x", height, ", ", channels, " channels)");
    UINT cubemapSize = max(width / 4, height / 2);
	
	D3D12_RESOURCE_DESC textureDesc = {};
	textureDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	textureDesc.MipLevels = 1;
	textureDesc.SampleDesc.Count = 1;
	textureDesc.SampleDesc.Quality = 0;
	textureDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
	textureDesc.Flags = D3D12_RESOURCE_FLAG_NONE;
    textureDesc.Width = cubemapSize;
    textureDesc.Height = cubemapSize;
    textureDesc.DepthOrArraySize = 6;
    textureDesc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
	
    vector<vector<float>> faceData = EquirectangularToCubemap(data, width, height);
    
    vector<vector<uint16_t>> faceDataRGBA16(6);
    for (int face = 0; face < 6; ++face)
    {
        faceDataRGBA16[face].resize(cubemapSize * cubemapSize * 4); 
        for (size_t i = 0; i < cubemapSize * cubemapSize; ++i)
        {
            // Convert RGB float to RGBA16F
            faceDataRGBA16[face][i * 4 + 0] = PackedVector::XMConvertFloatToHalf(faceData[face][i * 3 + 0]); // R
            faceDataRGBA16[face][i * 4 + 1] = PackedVector::XMConvertFloatToHalf(faceData[face][i * 3 + 1]); // G
            faceDataRGBA16[face][i * 4 + 2] = PackedVector::XMConvertFloatToHalf(faceData[face][i * 3 + 2]); // B
            faceDataRGBA16[face][i * 4 + 3] = PackedVector::XMConvertFloatToHalf(1.0f); // A
        }
    }
    
    UINT64 rowSizeInBytes = static_cast<UINT64>(cubemapSize) * 4 /*RGBA*/ * sizeof(uint16_t) /*16-bit per channel*/;
    
    vector<uint8_t*> faceDataBytes(6);
    for (int face = 0; face < 6; ++face)
    {
        faceDataBytes[face] = reinterpret_cast<uint8_t*>(faceDataRGBA16[face].data());
    }

	Texture texture = {};
	texture.Resource = CreateCubemapResource(device, commandList, textureDesc, faceDataBytes, rowSizeInBytes, texture.UploadBuffer);
    
    stbi_image_free(data);
	
	m_textureMap[textureInfo.name] = make_unique<Texture>(texture);
	CreateShaderResourceView(textureInfo, descriptorAllocator);

	UINT irradianceMapSize = cubemapSize / 2;
	Texture irradianceTexture = {};
	irradianceTexture.Resource = CreateEmptyMapResource(device, irradianceMapSize, 6, DXGI_FORMAT_R16G16B16A16_FLOAT);

	LunarConstants::TextureInfo irradianceTextureInfo = textureInfo;
	irradianceTextureInfo.name = "skybox_irradiance";
	m_textureMap[irradianceTextureInfo.name] = make_unique<Texture>(irradianceTexture);
	CreateShaderResourceView(irradianceTextureInfo, descriptorAllocator);

	D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
	uavDesc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
	uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2DARRAY;
	uavDesc.Texture2DArray.MipSlice = 0;
	uavDesc.Texture2DArray.FirstArraySlice = 0;
	uavDesc.Texture2DArray.ArraySize = 6;
	// descriptorAllocator->AllocateDescriptor("skybox_irradiance_uav");
	descriptorAllocator->CreateUAV(LunarConstants::RangeType::DYNAMIC_UAV, "skybox_irradiance_uav", irradianceTexture.Resource.Get(), &uavDesc);

	D3D12_RESOURCE_BARRIER barrier = {};
	barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	barrier.Transition.pResource = texture.Resource.Get();
	barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
	barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
	barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
	commandList->ResourceBarrier(1, &barrier);

	barrier.Transition.pResource = irradianceTexture.Resource.Get();
	barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
	barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
	commandList->ResourceBarrier(1, &barrier);
	
	ID3D12DescriptorHeap* descriptorHeaps[] = { descriptorAllocator->GetHeap() };
	commandList->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);

	commandList->SetComputeRootSignature(pipelineStateManager->GetComputeRootSignature());
	commandList->SetComputeRootDescriptorTable(LunarConstants::COMPUTE_INPUT_SRV_INDEX, descriptorAllocator->GetGPUHandle(textureInfo.name));
	commandList->SetComputeRootDescriptorTable(LunarConstants::COMPUTE_OUTPUT_UAV_INDEX, descriptorAllocator->GetGPUHandle("skybox_irradiance_uav"));
	commandList->SetPipelineState(pipelineStateManager->GetPSO("irradiance"));
	commandList->Dispatch((irradianceMapSize + 7) / 8, (irradianceMapSize + 7) / 8, 6);

	barrier.Transition.pResource = irradianceTexture.Resource.Get();
	barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
	barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
	commandList->ResourceBarrier(1, &barrier);
	
	// Generate Prefiltered Environment Map
	UINT prefilteredMapSize = cubemapSize / 2; 
	UINT maxMipLevels = static_cast<UINT>(std::floor(std::log2(prefilteredMapSize))) + 1;
	
	Texture prefilteredTexture = {};
	prefilteredTexture.Resource = CreateEmptyMapResource(device, prefilteredMapSize, 6, DXGI_FORMAT_R16G16B16A16_FLOAT, maxMipLevels);
	
	LunarConstants::TextureInfo prefilteredTextureInfo = textureInfo;
	prefilteredTextureInfo.name = "skybox_prefiltered";
	m_textureMap[prefilteredTextureInfo.name] = make_unique<Texture>(prefilteredTexture);
	CreateShaderResourceView(prefilteredTextureInfo, descriptorAllocator, maxMipLevels);
	
	// commandList->SetComputeRootSignature(pipelineStateManager->GetComputeRootSignature());
	for (UINT mip = 0; mip < maxMipLevels; ++mip)
	{
		barrier.Transition.Subresource = mip;
		barrier.Transition.pResource = prefilteredTexture.Resource.Get();
		barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
		barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
		commandList->ResourceBarrier(1, &barrier);
		
		UINT mipSize = prefilteredMapSize >> mip;
		if (mipSize == 0) mipSize = 1;
		
		float roughness = static_cast<float>(mip) / static_cast<float>(maxMipLevels - 1);
		
		UINT constants[4] = {
			*reinterpret_cast<UINT*>(&roughness), // float as uint32
			mip,                                   // mip level
			0,                                     // padding
			0                                      // padding
		};
		
		uavDesc.Texture2DArray.MipSlice = mip;
		
		string uavName = "skybox_prefiltered_uav_mip" + to_string(mip);
		// descriptorAllocator->AllocateDescriptor(uavName);
		descriptorAllocator->CreateUAV(LunarConstants::RangeType::DYNAMIC_UAV, uavName, prefilteredTexture.Resource.Get(), &uavDesc);
		
		commandList->SetComputeRoot32BitConstants(LunarConstants::COMPUTE_CONSTANTS_INDEX, 4, constants, 0);
		// commandList->SetComputeRootDescriptorTable(LunarConstants::COMPUTE_INPUT_SRV_INDEX, descriptorAllocator->GetGPUHandle(textureInfo.name));
		commandList->SetComputeRootDescriptorTable(LunarConstants::COMPUTE_OUTPUT_UAV_INDEX, descriptorAllocator->GetGPUHandle(uavName));
		commandList->SetPipelineState(pipelineStateManager->GetPSO("prefiltered"));
		
		commandList->Dispatch((mipSize + 7) / 8, (mipSize + 7) / 8, 6);

		barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
		barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
		commandList->ResourceBarrier(1, &barrier);
	}

	// restore cubemap state
	barrier.Transition.pResource = texture.Resource.Get();
	barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
	barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
	barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
	commandList->ResourceBarrier(1, &barrier);

	// BRDF LUT
	const UINT brdfLutSize = 512;
	
	Texture brdfLutTexture = {};
	LunarConstants::TextureInfo brdfLutTextureInfo = textureInfo;
	string brdfLutName = string(textureInfo.name) + "_brdf_lut";
	brdfLutTextureInfo.dimensionType = LunarConstants::TextureDimension::TEXTURE2D;
	brdfLutTextureInfo.name = brdfLutName.c_str();
	brdfLutTexture.Resource = CreateEmptyMapResource(device, brdfLutSize, 1, DXGI_FORMAT_R16G16_FLOAT);
	m_textureMap[brdfLutTextureInfo.name] = make_unique<Texture>(brdfLutTexture);
	CreateShaderResourceView(brdfLutTextureInfo, descriptorAllocator);
	
	D3D12_UNORDERED_ACCESS_VIEW_DESC brdfLutUavDesc = {};
	brdfLutUavDesc.Format = DXGI_FORMAT_R16G16_FLOAT;
	brdfLutUavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
	brdfLutUavDesc.Texture2D.MipSlice = 0;
	brdfLutUavDesc.Texture2D.PlaneSlice = 0;

	// descriptorAllocator->AllocateDescriptor("brdf_lut_uav");
	descriptorAllocator->CreateUAV(LunarConstants::RangeType::DYNAMIC_UAV, "brdf_lut_uav", brdfLutTexture.Resource.Get(), &brdfLutUavDesc);

	barrier.Transition.pResource = brdfLutTexture.Resource.Get();
	barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
	barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
	barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
	commandList->ResourceBarrier(1, &barrier);

	commandList->SetComputeRootSignature(pipelineStateManager->GetComputeRootSignature());
	commandList->SetComputeRootDescriptorTable(LunarConstants::COMPUTE_OUTPUT_UAV_INDEX, descriptorAllocator->GetGPUHandle("brdf_lut_uav"));
	commandList->SetPipelineState(pipelineStateManager->GetPSO("brdfLut"));
	commandList->Dispatch((brdfLutSize + 7) / 8, (brdfLutSize + 7) / 8, 1);

	barrier.Transition.pResource = brdfLutTexture.Resource.Get();
	barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
	barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
	barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
	commandList->ResourceBarrier(1, &barrier);
}

} //namespace Lunar
