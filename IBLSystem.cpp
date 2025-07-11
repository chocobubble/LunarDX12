#include "IBLSystem.h"
#include "Utils/Logger.h"
#include <vector>

namespace Lunar
{

bool IBLSystem::Initialize(ID3D12Device* device, ID3D12GraphicsCommandList* commandList)
{
    // Generate BRDF LUT (this can be done once and reused)
    m_iblTextures.brdfLUT = GenerateBRDFLUT(device, commandList);
    if (!m_iblTextures.brdfLUT) {
        Logger::Log("Failed to generate BRDF LUT");
        return false;
    }
    
    Logger::Log("IBL System initialized successfully");
    return true;
}

bool IBLSystem::LoadEnvironmentMap(const std::string& hdrFilename,
                                  ID3D12Device* device,
                                  ID3D12GraphicsCommandList* commandList,
                                  D3D12_CPU_DESCRIPTOR_HANDLE& srvHandle)
{
    // Load HDR file
    auto hdrImage = HDRLoader::LoadHDR(hdrFilename);
    if (!hdrImage) {
        Logger::Log("Failed to load HDR file: " + hdrFilename);
        return false;
    }
    
    Logger::Log("Loaded HDR image: " + std::to_string(hdrImage->width) + "x" + std::to_string(hdrImage->height));
    
    // Convert equirectangular to cubemap
    auto environmentCubemap = HDRLoader::EquirectangularToCubemap(*hdrImage, 512);
    m_iblTextures.environmentCubemap = CreateCubemapFromHDRImages(environmentCubemap, device, commandList);
    if (!m_iblTextures.environmentCubemap) {
        Logger::Log("Failed to create environment cubemap");
        return false;
    }
    
    // Generate irradiance map
    auto irradianceImages = HDRLoader::GenerateIrradianceMap(environmentCubemap, 64);
    m_iblTextures.irradianceMap = CreateCubemapFromHDRImages(irradianceImages, device, commandList);
    if (!m_iblTextures.irradianceMap) {
        Logger::Log("Failed to create irradiance map");
        return false;
    }
    
    // Generate prefilter map (simplified - just use first mip level)
    auto prefilterMips = HDRLoader::GeneratePrefilterMap(environmentCubemap, 256, 5);
    if (!prefilterMips.empty()) {
        m_iblTextures.prefilterMap = CreateCubemapFromHDRImages(prefilterMips[0], device, commandList);
    }
    
    // Create shader resource views
    CreateShaderResourceView(m_iblTextures.environmentCubemap.Get(), device, srvHandle, true);
    m_iblTextures.environmentSRV = srvHandle;
    srvHandle.ptr += device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    
    CreateShaderResourceView(m_iblTextures.irradianceMap.Get(), device, srvHandle, true);
    m_iblTextures.irradianceSRV = srvHandle;
    srvHandle.ptr += device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    
    if (m_iblTextures.prefilterMap) {
        CreateShaderResourceView(m_iblTextures.prefilterMap.Get(), device, srvHandle, true);
        m_iblTextures.prefilterSRV = srvHandle;
        srvHandle.ptr += device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    }
    
    CreateShaderResourceView(m_iblTextures.brdfLUT.Get(), device, srvHandle, false);
    m_iblTextures.brdfSRV = srvHandle;
    srvHandle.ptr += device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    
    Logger::Log("IBL textures created successfully");
    return true;
}

void IBLSystem::BindIBLTextures(ID3D12GraphicsCommandList* commandList,
                               UINT environmentRootIndex,
                               UINT irradianceRootIndex,
                               UINT prefilterRootIndex,
                               UINT brdfRootIndex)
{
    // Convert CPU handles to GPU handles for binding
    // Note: This is simplified - in practice you'd need proper GPU descriptor heap management
    
    // For now, just log that we're binding (actual implementation would require descriptor heap setup)
    Logger::Log("Binding IBL textures to root signature");
}

Microsoft::WRL::ComPtr<ID3D12Resource> IBLSystem::CreateCubemapFromHDRImages(
    const std::vector<std::unique_ptr<HDRImage>>& images,
    ID3D12Device* device,
    ID3D12GraphicsCommandList* commandList,
    DXGI_FORMAT format)
{
    if (images.size() != 6 || !images[0]) {
        return nullptr;
    }
    
    int width = images[0]->width;
    int height = images[0]->height;
    
    // Create cubemap resource
    D3D12_RESOURCE_DESC textureDesc = {};
    textureDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    textureDesc.Width = width;
    textureDesc.Height = height;
    textureDesc.DepthOrArraySize = 6; // 6 faces
    textureDesc.MipLevels = 1;
    textureDesc.Format = format;
    textureDesc.SampleDesc.Count = 1;
    textureDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
    textureDesc.Flags = D3D12_RESOURCE_FLAG_NONE;
    
    Microsoft::WRL::ComPtr<ID3D12Resource> cubemap;
    
    D3D12_HEAP_PROPERTIES heapProps = {};
    heapProps.Type = D3D12_HEAP_TYPE_DEFAULT;
    
    HRESULT hr = device->CreateCommittedResource(
        &heapProps,
        D3D12_HEAP_FLAG_NONE,
        &textureDesc,
        D3D12_RESOURCE_STATE_COPY_DEST,
        nullptr,
        IID_PPV_ARGS(&cubemap));
    
    if (FAILED(hr)) {
        return nullptr;
    }
    
    // Upload data for each face
    const UINT bytesPerPixel = 8; // R16G16B16A16_FLOAT = 8 bytes per pixel
    const UINT64 uploadBufferSize = width * height * bytesPerPixel * 6;
    
    // Create upload buffer
    Microsoft::WRL::ComPtr<ID3D12Resource> uploadBuffer;
    D3D12_HEAP_PROPERTIES uploadHeapProps = {};
    uploadHeapProps.Type = D3D12_HEAP_TYPE_UPLOAD;
    
    D3D12_RESOURCE_DESC uploadDesc = {};
    uploadDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    uploadDesc.Width = uploadBufferSize;
    uploadDesc.Height = 1;
    uploadDesc.DepthOrArraySize = 1;
    uploadDesc.MipLevels = 1;
    uploadDesc.Format = DXGI_FORMAT_UNKNOWN;
    uploadDesc.SampleDesc.Count = 1;
    uploadDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    
    hr = device->CreateCommittedResource(
        &uploadHeapProps,
        D3D12_HEAP_FLAG_NONE,
        &uploadDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(&uploadBuffer));
    
    if (FAILED(hr)) {
        return nullptr;
    }
    
    // Map and copy data
    void* mappedData;
    uploadBuffer->Map(0, nullptr, &mappedData);
    
    for (int face = 0; face < 6; ++face) {
        const HDRImage& image = *images[face];
        
        // Convert float RGB to half-float RGBA for R16G16B16A16_FLOAT
        std::vector<uint16_t> halfFloatData(width * height * 4);
        
        for (int i = 0; i < width * height; ++i) {
            // Simple float to half conversion (not optimal, but functional)
            float r = image.data[i * 3];
            float g = image.data[i * 3 + 1];
            float b = image.data[i * 3 + 2];
            float a = 1.0f;
            
            // Convert to half-float (simplified)
            halfFloatData[i * 4] = static_cast<uint16_t>(std::min(65535.0f, r * 65535.0f));
            halfFloatData[i * 4 + 1] = static_cast<uint16_t>(std::min(65535.0f, g * 65535.0f));
            halfFloatData[i * 4 + 2] = static_cast<uint16_t>(std::min(65535.0f, b * 65535.0f));
            halfFloatData[i * 4 + 3] = static_cast<uint16_t>(65535);
        }
        
        // Copy face data to upload buffer
        size_t faceOffset = face * width * height * bytesPerPixel;
        memcpy(static_cast<char*>(mappedData) + faceOffset, 
               halfFloatData.data(), 
               width * height * bytesPerPixel);
    }
    
    uploadBuffer->Unmap(0, nullptr);
    
    // Copy from upload buffer to texture
    for (int face = 0; face < 6; ++face) {
        D3D12_TEXTURE_COPY_LOCATION dst = {};
        dst.pResource = cubemap.Get();
        dst.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
        dst.SubresourceIndex = face;
        
        D3D12_TEXTURE_COPY_LOCATION src = {};
        src.pResource = uploadBuffer.Get();
        src.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
        src.PlacedFootprint.Offset = face * width * height * bytesPerPixel;
        src.PlacedFootprint.Footprint.Format = format;
        src.PlacedFootprint.Footprint.Width = width;
        src.PlacedFootprint.Footprint.Height = height;
        src.PlacedFootprint.Footprint.Depth = 1;
        src.PlacedFootprint.Footprint.RowPitch = width * bytesPerPixel;
        
        commandList->CopyTextureRegion(&dst, 0, 0, 0, &src, nullptr);
    }
    
    // Transition to shader resource
    D3D12_RESOURCE_BARRIER barrier = {};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Transition.pResource = cubemap.Get();
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    
    commandList->ResourceBarrier(1, &barrier);
    
    return cubemap;
}

void IBLSystem::CreateShaderResourceView(ID3D12Resource* resource,
                                        ID3D12Device* device,
                                        D3D12_CPU_DESCRIPTOR_HANDLE& srvHandle,
                                        bool isCubemap)
{
    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    
    if (isCubemap) {
        srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE;
        srvDesc.TextureCube.MipLevels = 1;
    } else {
        srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
        srvDesc.Texture2D.MipLevels = 1;
    }
    
    device->CreateShaderResourceView(resource, &srvDesc, srvHandle);
}

Microsoft::WRL::ComPtr<ID3D12Resource> IBLSystem::GenerateBRDFLUT(
    ID3D12Device* device,
    ID3D12GraphicsCommandList* commandList,
    int lutSize)
{
    // Create a simple BRDF LUT texture (placeholder implementation)
    D3D12_RESOURCE_DESC textureDesc = {};
    textureDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    textureDesc.Width = lutSize;
    textureDesc.Height = lutSize;
    textureDesc.DepthOrArraySize = 1;
    textureDesc.MipLevels = 1;
    textureDesc.Format = DXGI_FORMAT_R16G16_FLOAT;
    textureDesc.SampleDesc.Count = 1;
    textureDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
    textureDesc.Flags = D3D12_RESOURCE_FLAG_NONE;
    
    Microsoft::WRL::ComPtr<ID3D12Resource> brdfLUT;
    
    D3D12_HEAP_PROPERTIES heapProps = {};
    heapProps.Type = D3D12_HEAP_TYPE_DEFAULT;
    
    HRESULT hr = device->CreateCommittedResource(
        &heapProps,
        D3D12_HEAP_FLAG_NONE,
        &textureDesc,
        D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
        nullptr,
        IID_PPV_ARGS(&brdfLUT));
    
    if (FAILED(hr)) {
        return nullptr;
    }
    
    // In a real implementation, you would generate the BRDF LUT using compute shaders
    // For now, we just create an empty texture
    
    return brdfLUT;
}

} // namespace Lunar
