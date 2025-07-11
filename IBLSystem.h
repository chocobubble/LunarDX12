#pragma once
#include <d3d12.h>
#include <wrl/client.h>
#include <memory>
#include <string>
#include "Utils/HDRLoader.h"

namespace Lunar
{

class IBLSystem
{
public:
    struct IBLTextures
    {
        Microsoft::WRL::ComPtr<ID3D12Resource> environmentCubemap;
        Microsoft::WRL::ComPtr<ID3D12Resource> irradianceMap;
        Microsoft::WRL::ComPtr<ID3D12Resource> prefilterMap;
        Microsoft::WRL::ComPtr<ID3D12Resource> brdfLUT;
        
        D3D12_CPU_DESCRIPTOR_HANDLE environmentSRV;
        D3D12_CPU_DESCRIPTOR_HANDLE irradianceSRV;
        D3D12_CPU_DESCRIPTOR_HANDLE prefilterSRV;
        D3D12_CPU_DESCRIPTOR_HANDLE brdfSRV;
    };

public:
    IBLSystem() = default;
    ~IBLSystem() = default;

    // Initialize IBL system
    bool Initialize(ID3D12Device* device, ID3D12GraphicsCommandList* commandList);
    
    // Load HDR environment map and generate IBL textures
    bool LoadEnvironmentMap(const std::string& hdrFilename,
                           ID3D12Device* device,
                           ID3D12GraphicsCommandList* commandList,
                           D3D12_CPU_DESCRIPTOR_HANDLE& srvHandle);
    
    // Get IBL textures for rendering
    const IBLTextures& GetIBLTextures() const { return m_iblTextures; }
    
    // Bind IBL textures to root signature
    void BindIBLTextures(ID3D12GraphicsCommandList* commandList,
                        UINT environmentRootIndex,
                        UINT irradianceRootIndex,
                        UINT prefilterRootIndex,
                        UINT brdfRootIndex);

private:
    IBLTextures m_iblTextures;
    
    // Helper functions
    Microsoft::WRL::ComPtr<ID3D12Resource> CreateCubemapFromHDRImages(
        const std::vector<std::unique_ptr<HDRImage>>& images,
        ID3D12Device* device,
        ID3D12GraphicsCommandList* commandList,
        DXGI_FORMAT format = DXGI_FORMAT_R16G16B16A16_FLOAT);
    
    Microsoft::WRL::ComPtr<ID3D12Resource> CreateTexture2DFromHDRImage(
        const HDRImage& image,
        ID3D12Device* device,
        ID3D12GraphicsCommandList* commandList,
        DXGI_FORMAT format = DXGI_FORMAT_R16G16B16A16_FLOAT);
    
    void CreateShaderResourceView(ID3D12Resource* resource,
                                 ID3D12Device* device,
                                 D3D12_CPU_DESCRIPTOR_HANDLE& srvHandle,
                                 bool isCubemap = false);
    
    // Generate BRDF integration LUT
    Microsoft::WRL::ComPtr<ID3D12Resource> GenerateBRDFLUT(
        ID3D12Device* device,
        ID3D12GraphicsCommandList* commandList,
        int lutSize = 512);
    
    // Upload helper
    void UploadTextureData(ID3D12Resource* texture,
                          const void* data,
                          UINT64 dataSize,
                          UINT width,
                          UINT height,
                          UINT bytesPerPixel,
                          ID3D12Device* device,
                          ID3D12GraphicsCommandList* commandList);
};

} // namespace Lunar
