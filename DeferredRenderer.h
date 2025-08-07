#pragma once
#include <d3d12.h>
#include <wrl.h>
#include <DirectXMath.h>
#include <vector>
#include <memory>

namespace Lunar
{

class PipelineStateManager;
class DescriptorAllocator;
class LightingSystem;
struct Light;
struct RenderObject;

class DeferredRenderer
{
public:
    struct GBuffer
    {
        // G-Buffer 레이아웃:
        // RT0: Albedo (RGB) + Metallic (A)
        // RT1: Normal (RGB) + Roughness (A) 
        // RT2: Motion Vector (RG) + Depth (B) + Object ID (A)
        // RT3: Emission (RGB) + AO (A)
        Microsoft::WRL::ComPtr<ID3D12Resource> albedoMetallic;
        Microsoft::WRL::ComPtr<ID3D12Resource> normalRoughness;
        Microsoft::WRL::ComPtr<ID3D12Resource> motionDepthID;
        Microsoft::WRL::ComPtr<ID3D12Resource> emissionAO;
        Microsoft::WRL::ComPtr<ID3D12Resource> depthStencil;
        
        // RTV/DSV 핸들
        D3D12_CPU_DESCRIPTOR_HANDLE rtvHandles[4];
        D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle;
        
        // SRV 핸들 (라이팅 패스에서 사용)
        D3D12_CPU_DESCRIPTOR_HANDLE srvHandles[4];
        D3D12_GPU_DESCRIPTOR_HANDLE srvGpuHandles[4];
    };
    
    struct DeferredSettings
    {
        bool enableTiledLighting = true;
        UINT tileSize = 16; // 16x16 타일
        UINT maxLightsPerTile = 256;
        bool enableDebugVisualization = false;
        
        enum class DebugMode
        {
            None,
            Albedo,
            Normal,
            Roughness,
            Metallic,
            Depth,
            MotionVector,
            LightHeatmap
        } debugMode = DebugMode::None;
    };

    DeferredRenderer();
    ~DeferredRenderer();

    void Initialize(ID3D12Device* device, UINT width, UINT height,
                   PipelineStateManager* psoManager, DescriptorAllocator* descriptorAllocator);
    void Shutdown();
    
    void Resize(UINT width, UINT height);
    
    // 렌더링 패스들
    void BeginGeometryPass(ID3D12GraphicsCommandList* cmdList);
    void EndGeometryPass(ID3D12GraphicsCommandList* cmdList);
    
    void ExecuteLightingPass(ID3D12GraphicsCommandList* cmdList, 
                           const std::vector<Light*>& lights,
                           ID3D12Resource* outputTarget);
    
    void ExecuteTransparentPass(ID3D12GraphicsCommandList* cmdList,
                              const std::vector<RenderObject*>& transparentObjects);
    
    // G-Buffer 접근
    const GBuffer& GetGBuffer() const { return m_gBuffer; }
    ID3D12Resource* GetFinalOutput() const { return m_lightAccumulation.Get(); }
    
    // 설정
    DeferredSettings& GetSettings() { return m_settings; }
    const DeferredSettings& GetSettings() const { return m_settings; }
    
    // 디버깅
    void RenderDebugVisualization(ID3D12GraphicsCommandList* cmdList, ID3D12Resource* outputTarget);

private:
    void CreateGBuffer();
    void CreateLightAccumulationBuffer();
    void CreateTiledLightingResources();
    void CreateDebugResources();
    
    void ExecuteTiledLightCulling(ID3D12GraphicsCommandList* cmdList, const std::vector<Light*>& lights);
    void ExecuteLightingCompute(ID3D12GraphicsCommandList* cmdList);
    
    // 리소스
    ID3D12Device* m_device = nullptr;
    PipelineStateManager* m_psoManager = nullptr;
    DescriptorAllocator* m_descriptorAllocator = nullptr;
    
    UINT m_width = 0;
    UINT m_height = 0;
    
    GBuffer m_gBuffer;
    Microsoft::WRL::ComPtr<ID3D12Resource> m_lightAccumulation;
    
    // 타일드 라이팅 리소스
    Microsoft::WRL::ComPtr<ID3D12Resource> m_lightIndexBuffer;      // 타일별 라이트 인덱스
    Microsoft::WRL::ComPtr<ID3D12Resource> m_lightCountBuffer;      // 타일별 라이트 개수
    Microsoft::WRL::ComPtr<ID3D12Resource> m_lightDataBuffer;       // 라이트 데이터
    Microsoft::WRL::ComPtr<ID3D12Resource> m_tileDebugBuffer;       // 디버그 시각화용
    
    UINT m_tilesX = 0;
    UINT m_tilesY = 0;
    
    DeferredSettings m_settings;
    
    // 상수 버퍼
    struct DeferredConstants
    {
        DirectX::XMFLOAT4X4 invViewProj;
        DirectX::XMFLOAT4X4 view;
        DirectX::XMFLOAT4X4 proj;
        DirectX::XMFLOAT3 cameraPos;
        float padding1;
        DirectX::XMFLOAT2 screenSize;
        DirectX::XMFLOAT2 invScreenSize;
        UINT numLights;
        UINT tileSize;
        UINT tilesX;
        UINT tilesY;
        UINT maxLightsPerTile;
        UINT debugMode;
        float padding2[2];
    };
    
    Microsoft::WRL::ComPtr<ID3D12Resource> m_constantBuffer;
    DeferredConstants* m_constantData = nullptr;
};

// 타일드 라이팅을 위한 컴퓨트 셰이더 구조체
struct TileLightData
{
    UINT lightCount;
    UINT lightIndices[255]; // 최대 255개 라이트 (UINT 패킹 고려)
};

} // namespace Lunar
