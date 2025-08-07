#pragma once
#include <d3d12.h>
#include <wrl.h>
#include <DirectXMath.h>
#include <vector>

namespace Lunar
{

class PipelineStateManager;
class DescriptorAllocator;

class SSAORenderer
{
public:
    struct SSAOSettings
    {
        float radius = 0.5f;           // 샘플링 반경
        float bias = 0.025f;           // 깊이 바이어스
        float power = 2.0f;            // AO 강도
        float fadeStart = 0.2f;        // 페이드 시작 거리
        float fadeEnd = 2.0f;          // 페이드 끝 거리
        UINT sampleCount = 64;         // 샘플 개수 (16, 32, 64)
        UINT noiseSize = 4;            // 노이즈 텍스처 크기 (4x4)
        bool enableBlur = true;        // 블러 활성화
        float blurRadius = 2.0f;       // 블러 반경
        bool enableTemporalFilter = false; // 시간적 필터링
    };

    SSAORenderer();
    ~SSAORenderer();

    void Initialize(ID3D12Device* device, UINT width, UINT height,
                   PipelineStateManager* psoManager, DescriptorAllocator* descriptorAllocator);
    void Shutdown();
    
    void Resize(UINT width, UINT height);
    
    // SSAO 렌더링
    void Render(ID3D12GraphicsCommandList* cmdList,
               ID3D12Resource* depthBuffer,
               ID3D12Resource* normalBuffer,
               const DirectX::XMFLOAT4X4& view,
               const DirectX::XMFLOAT4X4& projection);
    
    // 결과 텍스처 접근
    ID3D12Resource* GetAOTexture() const { return m_aoTexture.Get(); }
    ID3D12Resource* GetBlurredAOTexture() const { return m_blurredAOTexture.Get(); }
    
    // 설정
    SSAOSettings& GetSettings() { return m_settings; }
    const SSAOSettings& GetSettings() const { return m_settings; }
    
    // 디버깅
    void RenderDebugVisualization(ID3D12GraphicsCommandList* cmdList, ID3D12Resource* outputTarget);

private:
    void CreateAOTextures();
    void CreateNoiseTexture();
    void GenerateSampleKernel();
    void CreateConstantBuffer();
    
    void ExecuteSSAOPass(ID3D12GraphicsCommandList* cmdList,
                        ID3D12Resource* depthBuffer,
                        ID3D12Resource* normalBuffer);
    
    void ExecuteBlurPass(ID3D12GraphicsCommandList* cmdList);
    void ExecuteTemporalFilter(ID3D12GraphicsCommandList* cmdList);
    
    // 리소스
    ID3D12Device* m_device = nullptr;
    PipelineStateManager* m_psoManager = nullptr;
    DescriptorAllocator* m_descriptorAllocator = nullptr;
    
    UINT m_width = 0;
    UINT m_height = 0;
    
    // SSAO 텍스처들
    Microsoft::WRL::ComPtr<ID3D12Resource> m_aoTexture;         // 원본 AO
    Microsoft::WRL::ComPtr<ID3D12Resource> m_blurredAOTexture;  // 블러된 AO
    Microsoft::WRL::ComPtr<ID3D12Resource> m_temporalAOTexture; // 시간적 필터링된 AO
    Microsoft::WRL::ComPtr<ID3D12Resource> m_noiseTexture;      // 랜덤 노이즈
    
    // 디스크립터 핸들들
    D3D12_CPU_DESCRIPTOR_HANDLE m_aoRTV;
    D3D12_CPU_DESCRIPTOR_HANDLE m_blurredAORTV;
    D3D12_CPU_DESCRIPTOR_HANDLE m_temporalAORTV;
    
    D3D12_CPU_DESCRIPTOR_HANDLE m_aoSRV;
    D3D12_CPU_DESCRIPTOR_HANDLE m_blurredAOSRV;
    D3D12_CPU_DESCRIPTOR_HANDLE m_noiseSRV;
    
    D3D12_GPU_DESCRIPTOR_HANDLE m_aoSRVGpu;
    D3D12_GPU_DESCRIPTOR_HANDLE m_blurredAOSRVGpu;
    D3D12_GPU_DESCRIPTOR_HANDLE m_noiseSRVGpu;
    
    // 샘플 커널
    std::vector<DirectX::XMFLOAT4> m_sampleKernel;
    
    // 설정
    SSAOSettings m_settings;
    
    // 상수 버퍼
    struct SSAOConstants
    {
        DirectX::XMFLOAT4X4 view;
        DirectX::XMFLOAT4X4 projection;
        DirectX::XMFLOAT4X4 invProjection;
        DirectX::XMFLOAT4 samples[64]; // 최대 64개 샘플
        DirectX::XMFLOAT2 screenSize;
        DirectX::XMFLOAT2 invScreenSize;
        DirectX::XMFLOAT2 noiseScale;   // 화면크기 / 노이즈크기
        float radius;
        float bias;
        float power;
        float fadeStart;
        float fadeEnd;
        UINT sampleCount;
        float blurRadius;
        float padding;
    };
    
    Microsoft::WRL::ComPtr<ID3D12Resource> m_constantBuffer;
    SSAOConstants* m_constantData = nullptr;
    
    // 프레임 카운터 (시간적 필터링용)
    UINT m_frameIndex = 0;
};

} // namespace Lunar
