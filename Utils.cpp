#include "Utils.h"
#include <comdef.h>

std::string Lunar::LunarException::ToString() const
{
	// Get error message as wstring
	std::wstring wmsg = _com_error(m_hr).ErrorMessage();
    
	// Convert to string using proper Windows API
	int size_needed = WideCharToMultiByte(CP_UTF8, 0, wmsg.c_str(), -1, NULL, 0, NULL, NULL);
	std::string msg(size_needed, 0);
	WideCharToMultiByte(CP_UTF8, 0, wmsg.c_str(), -1, &msg[0], size_needed, NULL, NULL);
    
	return "[Error] " + m_function + " " + m_file + " " + std::to_string(m_line) + " error: " + msg;
}

ComPtr<ID3D12Resource> Utils::LoadSimpleTexture(ID3D12Device* device, ID3D12GraphicsCommandList* commandList, const std::string& filename, ComPtr<ID3D12Resource>& uploadBuffer)
{
    int width, height, channels;
    unsigned char* data = stbi_load(filename.c_str(), &width, &height, &channels, 4);
    if (!data)
    {
        LOG_ERROR("Failed to load texture: ", filename);
    }
    D3D12_RESOURCE_DESC textureDesc = {};
    textureDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    textureDesc.Width = static_cast<UINT>(width);
    textureDesc.Height = static_cast<UINT>(height);
    textureDesc.DepthOrArraySize = 1;
    textureDesc.MipLevels = 1;
    textureDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM; // stbi load with RGBA(4)
    textureDesc.SampleDesc.Count = 1;
    textureDesc.SampleDesc.Quality = 0;
    textureDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
    textureDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

    D3D12_HEAP_PROPERTIES heapProperties = {};
    heapProperties.Type = D3D12_HEAP_TYPE_DEFAULT;
	heapProperties.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
	heapProperties.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
	heapProperties.CreationNodeMask = 1;
	heapProperties.VisibleNodeMask = 1;

    ComPtr<ID3D12Resource> texture;

    try
    {
    THROW_IF_FAILED(device->CreateCommittedResource(
        &heapProperties, 
        D3D12_HEAP_FLAG_NONE, 
        &textureDesc, 
        D3D12_RESOURCE_STATE_COPY_DEST, 
        nullptr, IID_PPV_ARGS(&texture)))
    }
    catch (Exception e)
    {
        stbi_image_free(data);
        throw std::runtime_error("Failed to create texture resource");
    }

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
    UINT64 rowSizeInBytes;
    
    // Get GPU memory layout requirements for the texture
    // This calculates proper alignment and padding needed for GPU memory
    device->GetCopyableFootprints(
        &textureDesc,           // Input: texture description
        0,                      // First subresource index
        1,                      // Number of subresources
        0,                      // Base offset in upload buffer
        &layouts,               // Output: memory layout with alignment
        &numRows,               // Output: number of rows in the texture
        &rowSizeInBytes,        // Output: actual bytes per row (without padding)
        &uploadBufferSize       // Output: total bytes needed for upload buffer
    );

    // Results explanation:
    // - layouts.Offset: starting offset in upload buffer (usually 0)
    // - layouts.Footprint.RowPitch: bytes per row including GPU alignment padding
    // - layouts.Footprint.Width/Height: texture dimensions
    // - numRows: total number of rows to copy (equals texture height)
    // - rowSizeInBytes: actual data size per row (width * bytes_per_pixel)
    // - uploadBufferSize: total memory needed for upload buffer with alignment

    // upload buffer
    D3D12_HEAP_PROPERTIES heapProperties = {};
    heapProperties.Type = D3D12_HEAP_TYPE_UPLOAD;
	heapProperties.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
	heapProperties.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
	heapProperties.CreationNodeMask = 1;
	heapProperties.VisibleNodeMask = 1;

    D3D12_RESOURCE_DESC uploadBufferDesc = {};
    textureDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    textureDesc.Width = uploadBufferSize;
    textureDesc.Height = 1;
    textureDesc.DepthOrArraySize = 1;
    textureDesc.MipLevels = 1;
    textureDesc.Format = DXGI_FORMAT_UNKNOWN;
    textureDesc.SampleDesc.Count = 1;
    textureDesc.SampleDesc.Quality = 0;
    textureDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    textureDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

    THROW_IF_FAILED(device->CreateCommittedResource(
        &heapProperties,
        D3D12_HEAP_FLAG_NONE,
        &uploadBufferDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr, IID_PPV_ARGS(&uploadBuffer)));

    // copy data
    void* mappedData;
    uploadBuffer->Map(0, nullptr, &mappedData);
    BYTE* destSliceStart = reinterpret_cast<BYTE*>(mappedData) + layouts.Offset;
    for (UINT i = 0; i < numRows; i++)
    {
        memcpy(
            destSliceStart + layouts.Footprint.RowPitch * i, 
            data + i * rowSizeInBytes, 
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
    destLocation.pResource = uploadBuffer.Get();
    destLocation.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
    destLocation.SubresourceIndex = 0;

    D3D12_TEXTURE_COPY_LOCATION srcLocation = {};
    srcLocation.pResource = texture.Get();
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

    stbi_image_free(data);
    return texture;
}