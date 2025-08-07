#pragma once
#include <d3d12.h>
#include <wrl.h>
#include <DirectXMath.h>
#include <chrono>

namespace Lunar
{

class PipelineStateManager;
class DescriptorAllocator;

class TAARenderer
{
public:
    struct TAASettings
    {
        bool enabled = true;
        float blendFactor = 0.9f;          // 히스토리 블렌딩 비율 (0.0 = 현재 프레임만, 1.0 = 히스토리만)
        float motionThreshold = 0.01f;     // 모션 벡터 임계값
        float luminanceThreshold = 0.1f;   // 루미넌스 차이 임계값
        bool enableVarianceClipping = true; // 분산 클리핑 활성화
        float varianceGamma = 1.0f;        // 분산 클리핑 강도
        bool enableYCoCg = true;           // YCoCg 색공간 사용
        float sharpening = 0.1f;           // 샤프닝 강도
        bool enableJitterRemoval = true;   // 지터 제거
        
        // 지터 패턴
        enum class JitterPattern
        {
            Halton,
            Uniform2x2,
            Uniform4x4,
            Random
        } jitterPattern = JitterPattern::Halton;
    };

    TAARenderer();
    ~TAARenderer();

    void Initialize(ID3D12Device* device, UINT width, UINT height,
                   PipelineStateManager* psoManager, DescriptorAllocator* descriptorAllocator);
    void Shutdown();
    
    void Resize(UINT width, UINT height);
    
    // TAA 렌더링
    void Render(ID3D12GraphicsCommandList* cmdList,
               ID3D12Resource* currentFrame,
               ID3D12Resource* motionVectors,
               ID3D12Resource* depthBuffer);
    
    // 지터 오프셋 계산 (렌더링 전에 호출)
    DirectX::XMFLOAT2 GetCurrentJitterOffset() const { return m_currentJitterOffset; }
    void UpdateJitter();
    
    // 결과 텍스처 접근
    ID3D12Resource* GetResolvedFrame() const { return m_resolvedFrame.Get(); }
    ID3D12Resource* GetHistoryBuffer() const { return m_historyBuffer.Get(); }
    
    // 설정
    TAASettings& GetSettings() { return m_settings; }
    const TAASettings& GetSettings() const { return m_settings; }
    
    // 디버깅
    void RenderDebugVisualization(ID3D12GraphicsCommandList* cmdList, ID3D12Resource* outputTarget);
    void ResetHistory(); // 히스토리 버퍼 초기화

private:
    void CreateRenderTargets();
    void CreateConstantBuffer();
    
    void ExecuteTAAPass(ID3D12GraphicsCommandList* cmdList,
                       ID3D12Resource* currentFrame,
                       ID3D12Resource* motionVectors,
                       ID3D12Resource* depthBuffer);
    
    void ExecuteSharpeningPass(ID3D12GraphicsCommandList* cmdList);
    
    DirectX::XMFLOAT2 CalculateJitterOffset();
    DirectX::XMFLOAT2 GetHaltonJitter(UINT frameIndex);
    DirectX::XMFLOAT2 GetUniform2x2Jitter(UINT frameIndex);
    DirectX::XMFLOAT2 GetUniform4x4Jitter(UINT frameIndex);
    DirectX::XMFLOAT2 GetRandomJitter();
    
    // 리소스
    ID3D12Device* m_device = nullptr;
    PipelineStateManager* m_psoManager = nullptr;
    DescriptorAllocator* m_descriptorAllocator = nullptr;
    
    UINT m_width = 0;
    UINT m_height = 0;
    
    // TAA 텍스처들
    Microsoft::WRL::ComPtr<ID3D12Resource> m_historyBuffer;     // 이전 프레임 결과
    Microsoft::WRL::ComPtr<ID3D12Resource> m_resolvedFrame;    // 현재 프레임 TAA 결과
    Microsoft::WRL::ComPtr<ID3D12Resource> m_tempBuffer;       // 임시 버퍼 (샤프닝용)
    
    // 디스크립터 핸들들
    D3D12_CPU_DESCRIPTOR_HANDLE m_historyRTV;
    D3D12_CPU_DESCRIPTOR_HANDLE m_resolvedRTV;
    D3D12_CPU_DESCRIPTOR_HANDLE m_tempRTV;
    
    D3D12_CPU_DESCRIPTOR_HANDLE m_historySRV;
    D3D12_CPU_DESCRIPTOR_HANDLE m_resolvedSRV;
    D3D12_CPU_DESCRIPTOR_HANDLE m_tempSRV;
    
    D3D12_GPU_DESCRIPTOR_HANDLE m_historySRVGpu;
    D3D12_GPU_DESCRIPTOR_HANDLE m_resolvedSRVGpu;
    D3D12_GPU_DESCRIPTOR_HANDLE m_tempSRVGpu;
    
    // 지터 관련
    DirectX::XMFLOAT2 m_currentJitterOffset;
    DirectX::XMFLOAT2 m_previousJitterOffset;
    UINT m_frameIndex = 0;
    
    // 설정
    TAASettings m_settings;
    
    // 상수 버퍼
    struct TAAConstants
    {
        DirectX::XMFLOAT2 screenSize;
        DirectX::XMFLOAT2 invScreenSize;
        DirectX::XMFLOAT2 jitterOffset;
        DirectX::XMFLOAT2 prevJitterOffset;
        
        float blendFactor;
        float motionThreshold;
        float luminanceThreshold;
        float varianceGamma;
        
        float sharpening;
        UINT enableVarianceClipping;
        UINT enableYCoCg;
        UINT enableJitterRemoval;
        
        float padding[4];
    };
    
    Microsoft::WRL::ComPtr<ID3D12Resource> m_constantBuffer;
    TAAConstants* m_constantData = nullptr;
    
    // 히스토리 유효성 추적
    bool m_historyValid = false;
    bool m_needsReset = false;
};

} // namespace Lunar
