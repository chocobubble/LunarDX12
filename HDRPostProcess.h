#pragma once
#include <d3d12.h>
#include <wrl.h>
#include <DirectXMath.h>
#include <vector>

namespace Lunar
{

class PipelineStateManager;
class DescriptorAllocator;

class HDRPostProcess
{
public:
    struct ToneMappingSettings
    {
        enum class Type
        {
            Linear,
            Reinhard,
            ReinhardExtended,
            ACES,
            Uncharted2,
            Count
        };
        
        Type type = Type::ACES;
        float exposure = 1.0f;
        float gamma = 2.2f;
        float whitePoint = 11.2f;  // Reinhard Extended용
        bool enableAutoExposure = false;
        float adaptationSpeed = 1.0f;
        float minLuminance = 0.1f;
        float maxLuminance = 10.0f;
    };
    
    struct BloomSettings
    {
        bool enabled = true;
        float threshold = 1.0f;        // 밝기 임계값
        float intensity = 0.8f;        // 블룸 강도
        float scatter = 0.7f;          // 산란 정도
        UINT iterations = 5;           // 다운샘플링 단계
        float radius = 1.0f;           // 블러 반경
        bool enableLensDirt = false;   // 렌즈 더트 효과
        float lensDirtIntensity = 0.5f;
    };
    
    struct ColorGradingSettings
    {
        DirectX::XMFLOAT3 shadows = {1.0f, 1.0f, 1.0f};
        DirectX::XMFLOAT3 midtones = {1.0f, 1.0f, 1.0f};
        DirectX::XMFLOAT3 highlights = {1.0f, 1.0f, 1.0f};
        float saturation = 1.0f;
        float contrast = 1.0f;
        float brightness = 0.0f;
        float vibrance = 0.0f;
    };

    HDRPostProcess();
    ~HDRPostProcess();

    void Initialize(ID3D12Device* device, UINT width, UINT height,
                   PipelineStateManager* psoManager, DescriptorAllocator* descriptorAllocator);
    void Shutdown();
    
    void Resize(UINT width, UINT height);
    
    // 메인 처리 함수
    void Process(ID3D12GraphicsCommandList* cmdList,
                ID3D12Resource* hdrInput,
                ID3D12Resource* ldrOutput);
    
    // 개별 패스들
    void ExecuteBloomPass(ID3D12GraphicsCommandList* cmdList, ID3D12Resource* hdrInput);
    void ExecuteToneMappingPass(ID3D12GraphicsCommandList* cmdList,
                               ID3D12Resource* hdrInput,
                               ID3D12Resource* bloomTexture,
                               ID3D12Resource* ldrOutput);
    
    // 설정 접근
    ToneMappingSettings& GetToneMappingSettings() { return m_toneMappingSettings; }
    BloomSettings& GetBloomSettings() { return m_bloomSettings; }
    ColorGradingSettings& GetColorGradingSettings() { return m_colorGradingSettings; }
    
    // 결과 텍스처 접근
    ID3D12Resource* GetBloomTexture() const { return m_bloomChain.empty() ? nullptr : m_bloomChain[0].Get(); }
    ID3D12Resource* GetLuminanceTexture() const { return m_luminanceTexture.Get(); }
    
    // 디버깅
    void RenderDebugVisualization(ID3D12GraphicsCommandList* cmdList, ID3D12Resource* outputTarget);

private:
    void CreateBloomChain();
    void CreateLuminanceTextures();
    void CreateLensDirtTexture();
    void CreateConstantBuffer();
    
    void ExecuteBrightPass(ID3D12GraphicsCommandList* cmdList, ID3D12Resource* hdrInput);
    void ExecuteDownsample(ID3D12GraphicsCommandList* cmdList);
    void ExecuteUpsampleAndBlend(ID3D12GraphicsCommandList* cmdList);
    void ExecuteLuminanceCalculation(ID3D12GraphicsCommandList* cmdList, ID3D12Resource* hdrInput);
    void ExecuteAutoExposure(ID3D12GraphicsCommandList* cmdList);
    
    // 리소스
    ID3D12Device* m_device = nullptr;
    PipelineStateManager* m_psoManager = nullptr;
    DescriptorAllocator* m_descriptorAllocator = nullptr;
    
    UINT m_width = 0;
    UINT m_height = 0;
    
    // 블룸 체인 (다운샘플링된 텍스처들)
    std::vector<Microsoft::WRL::ComPtr<ID3D12Resource>> m_bloomChain;
    Microsoft::WRL::ComPtr<ID3D12Resource> m_brightPassTexture;
    
    // 루미넌스 텍스처들
    Microsoft::WRL::ComPtr<ID3D12Resource> m_luminanceTexture;      // 현재 프레임 평균 루미넌스
    Microsoft::WRL::ComPtr<ID3D12Resource> m_adaptedLuminance;      // 적응된 루미넌스
    Microsoft::WRL::ComPtr<ID3D12Resource> m_prevAdaptedLuminance;  // 이전 프레임 적응된 루미넌스
    
    // 렌즈 더트 텍스처
    Microsoft::WRL::ComPtr<ID3D12Resource> m_lensDirtTexture;
    
    // 디스크립터 핸들들
    std::vector<D3D12_CPU_DESCRIPTOR_HANDLE> m_bloomRTVs;
    std::vector<D3D12_CPU_DESCRIPTOR_HANDLE> m_bloomSRVs;
    std::vector<D3D12_GPU_DESCRIPTOR_HANDLE> m_bloomSRVsGpu;
    
    D3D12_CPU_DESCRIPTOR_HANDLE m_brightPassRTV;
    D3D12_CPU_DESCRIPTOR_HANDLE m_brightPassSRV;
    D3D12_GPU_DESCRIPTOR_HANDLE m_brightPassSRVGpu;
    
    D3D12_CPU_DESCRIPTOR_HANDLE m_luminanceSRV;
    D3D12_GPU_DESCRIPTOR_HANDLE m_luminanceSRVGpu;
    
    // 설정들
    ToneMappingSettings m_toneMappingSettings;
    BloomSettings m_bloomSettings;
    ColorGradingSettings m_colorGradingSettings;
    
    // 상수 버퍼
    struct HDRConstants
    {
        // 톤 매핑
        float exposure;
        float gamma;
        float whitePoint;
        UINT toneMappingType;
        
        // 블룸
        float bloomThreshold;
        float bloomIntensity;
        float bloomScatter;
        float bloomRadius;
        
        // 컬러 그레이딩
        DirectX::XMFLOAT3 shadows;
        float saturation;
        DirectX::XMFLOAT3 midtones;
        float contrast;
        DirectX::XMFLOAT3 highlights;
        float brightness;
        float vibrance;
        float lensDirtIntensity;
        
        // 화면 정보
        DirectX::XMFLOAT2 screenSize;
        DirectX::XMFLOAT2 invScreenSize;
        
        // 자동 노출
        float adaptationSpeed;
        float minLuminance;
        float maxLuminance;
        float deltaTime;
        
        float padding[4];
    };
    
    Microsoft::WRL::ComPtr<ID3D12Resource> m_constantBuffer;
    HDRConstants* m_constantData = nullptr;
    
    // 프레임 시간 추적 (자동 노출용)
    float m_deltaTime = 0.0f;
    std::chrono::high_resolution_clock::time_point m_lastFrameTime;
};

} // namespace Lunar
