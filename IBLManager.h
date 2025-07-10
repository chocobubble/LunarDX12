#pragma once

#include <d3d12.h>
#include <dxgi1_6.h>
#include <wrl/client.h>
#include <string>
#include <vector>
#include <memory>
#include <unordered_map>

using Microsoft::WRL::ComPtr;

namespace Lunar
{

struct HDRImage
{
    std::vector<float> pixels;
    int width = 0;
    int height = 0;
    int channels = 3;
    
    float* GetPixel(int x, int y) {
        return &pixels[(y * width + x) * channels];
    }
    
    bool IsValid() const {
        return width > 0 && height > 0 && !pixels.empty();
    }
};

struct IBLTextures
{
    ComPtr<ID3D12Resource> environmentCubemap;
    ComPtr<ID3D12Resource> irradianceMap;
    ComPtr<ID3D12Resource> prefilteredMap;
    ComPtr<ID3D12Resource> brdfLUT;
    
    D3D12_CPU_DESCRIPTOR_HANDLE envSRV = {};
    D3D12_CPU_DESCRIPTOR_HANDLE irradianceSRV = {};
    D3D12_CPU_DESCRIPTOR_HANDLE prefilteredSRV = {};
    D3D12_CPU_DESCRIPTOR_HANDLE brdfSRV = {};
    
    D3D12_CPU_DESCRIPTOR_HANDLE envUAV = {};
    D3D12_CPU_DESCRIPTOR_HANDLE irradianceUAV = {};
    D3D12_CPU_DESCRIPTOR_HANDLE prefilteredUAV = {};
    D3D12_CPU_DESCRIPTOR_HANDLE brdfUAV = {};
};

struct IBLConstants
{
    float envIntensity = 1.0f;
    float maxReflectionLod = 4.0f;
    float exposure = 1.0f;
    float gamma = 2.2f;
};

class IBLManager
{
public:
    IBLManager() = default;
    ~IBLManager() = default;

    bool Initialize(ID3D12Device* device);
    void Shutdown();
    
    bool LoadEnvironmentMap(const std::string& hdrFilePath);
    bool GenerateIBLTextures(ID3D12GraphicsCommandList* cmdList);
    
    const IBLTextures& GetIBLTextures() const { return m_iblTextures; }
    IBLConstants& GetConstants() { return m_constants; }
    const IBLConstants& GetConstants() const { return m_constants; }
    
    void BindIBLTextures(ID3D12GraphicsCommandList* cmdList, UINT rootParameterIndex);
    void RenderGUI();

private:
    ID3D12Device* m_device = nullptr;
    
    ComPtr<ID3D12DescriptorHeap> m_srvHeap;
    ComPtr<ID3D12DescriptorHeap> m_uavHeap;
    UINT m_srvDescriptorSize = 0;
    UINT m_uavDescriptorSize = 0;
    
    ComPtr<ID3D12RootSignature> m_computeRootSignature;
    std::unordered_map<std::string, ComPtr<ID3D12PipelineState>> m_computePSOs;
    
    ComPtr<ID3D12Resource> m_constantBuffer;
    IBLConstants m_constants;
    void* m_constantBufferData = nullptr;
    
    IBLTextures m_iblTextures;
    HDRImage m_hdrImage;
    
    UINT m_currentSrvIndex = 0;
    UINT m_currentUavIndex = 0;
    
    static const UINT ENV_CUBEMAP_SIZE = 512;
    static const UINT IRRADIANCE_SIZE = 32;
    static const UINT PREFILTERED_SIZE = 128;
    static const UINT BRDF_LUT_SIZE = 512;
    static const UINT MAX_MIP_LEVELS = 5;
    
    bool CreateDescriptorHeaps();
    bool CreateConstantBuffer();
    bool CreateRootSignature();
    bool CreatePipelineStates();
    
    bool CreateEnvironmentCubemap();
    bool CreateIrradianceMap();
    bool CreatePrefilteredMap();
    bool CreateBRDFLUT();
    
    void ConvertEquirectangularToCubemap(ID3D12GraphicsCommandList* cmdList);
    void GenerateIrradianceMap(ID3D12GraphicsCommandList* cmdList);
    void GeneratePrefilteredMap(ID3D12GraphicsCommandList* cmdList);
    void GenerateBRDFLUT(ID3D12GraphicsCommandList* cmdList);
    
    D3D12_CPU_DESCRIPTOR_HANDLE AllocateSRVDescriptor();
    D3D12_CPU_DESCRIPTOR_HANDLE AllocateUAVDescriptor();
    
    void TransitionResource(
        ID3D12GraphicsCommandList* cmdList,
        ID3D12Resource* resource,
        D3D12_RESOURCE_STATES before,
        D3D12_RESOURCE_STATES after);
    
    void UpdateConstantBuffer();
    void UAVBarrier(ID3D12GraphicsCommandList* cmdList, ID3D12Resource* resource);
};

class HDRLoader
{
public:
    static bool LoadHDR(const std::string& filename, HDRImage& image);
    static bool LoadEXR(const std::string& filename, HDRImage& image);

private:
    static bool LoadRadianceHDR(const std::string& filename, HDRImage& image);
    static float ConvertRGBE(unsigned char rgbe[4]);
    static void ConvertScanline(float* output, unsigned char* input, int width);
    static bool ReadHeader(FILE* file, int& width, int& height);
    static bool ReadPixels(FILE* file, HDRImage& image);
};

} // namespace Lunar
