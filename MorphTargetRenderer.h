#pragma once
#include <d3d12.h>
#include <wrl.h>
#include <DirectXMath.h>
#include <string>
#include <vector>
#include <unordered_map>

namespace Lunar
{

class PipelineStateManager;
class DescriptorAllocator;

class MorphTargetRenderer
{
public:
    struct MorphTarget
    {
        std::string name;
        std::vector<DirectX::XMFLOAT3> deltaPositions;  // 기본 위치로부터의 변화량
        std::vector<DirectX::XMFLOAT3> deltaNormals;    // 기본 노말로부터의 변화량
        std::vector<DirectX::XMFLOAT3> deltaTangents;   // 기본 탄젠트로부터의 변화량
        float weight = 0.0f;                            // 0.0 ~ 1.0 가중치
        
        // GPU 리소스
        Microsoft::WRL::ComPtr<ID3D12Resource> deltaPositionBuffer;
        Microsoft::WRL::ComPtr<ID3D12Resource> deltaNormalBuffer;
        Microsoft::WRL::ComPtr<ID3D12Resource> deltaTangentBuffer;
        
        D3D12_SHADER_RESOURCE_VIEW_DESC positionSRVDesc;
        D3D12_SHADER_RESOURCE_VIEW_DESC normalSRVDesc;
        D3D12_SHADER_RESOURCE_VIEW_DESC tangentSRVDesc;
    };
    
    struct MorphGroup
    {
        std::string name;
        std::vector<std::string> targetNames;
        float groupWeight = 1.0f;  // 그룹 전체 가중치
        bool isExclusive = false;  // 배타적 그룹 (하나만 활성화)
    };
    
    struct MorphSettings
    {
        bool enableGPUMorphing = true;      // GPU에서 모핑 계산
        bool enableNormalMorphing = true;   // 노말 모핑 활성화
        bool enableTangentMorphing = false; // 탄젠트 모핑 활성화
        UINT maxActiveMorphs = 8;           // 동시에 활성화할 수 있는 최대 모프 수
        float weightThreshold = 0.001f;     // 이 값 이하의 가중치는 무시
        bool enableWeightSmoothing = true;  // 가중치 부드럽게 변화
        float smoothingSpeed = 5.0f;        // 부드럽게 변화하는 속도
    };

    MorphTargetRenderer();
    ~MorphTargetRenderer();

    void Initialize(ID3D12Device* device, PipelineStateManager* psoManager, 
                   DescriptorAllocator* descriptorAllocator);
    void Shutdown();
    
    // 모프 타겟 관리
    void AddMorphTarget(const MorphTarget& target);
    void RemoveMorphTarget(const std::string& name);
    bool HasMorphTarget(const std::string& name) const;
    MorphTarget* GetMorphTarget(const std::string& name);
    
    // 모프 그룹 관리
    void AddMorphGroup(const MorphGroup& group);
    void RemoveMorphGroup(const std::string& name);
    void SetGroupWeight(const std::string& groupName, float weight);
    
    // 가중치 제어
    void SetWeight(const std::string& targetName, float weight);
    float GetWeight(const std::string& targetName) const;
    void SetWeights(const std::unordered_map<std::string, float>& weights);
    
    // 애니메이션 지원
    void AnimateWeight(const std::string& targetName, float targetWeight, float duration);
    void StopWeightAnimation(const std::string& targetName);
    
    // 프리셋 관리
    struct MorphPreset
    {
        std::string name;
        std::unordered_map<std::string, float> weights;
    };
    void SavePreset(const std::string& name, const MorphPreset& preset);
    void LoadPreset(const std::string& name, float blendTime = 0.0f);
    void BlendToPreset(const std::string& name, float blendTime);
    
    // 렌더링
    void UpdateVertexBuffer(ID3D12GraphicsCommandList* cmdList, 
                           ID3D12Resource* baseVertexBuffer,
                           UINT vertexCount, UINT vertexStride);
    
    ID3D12Resource* GetMorphedVertexBuffer() const { return m_morphedVertexBuffer.Get(); }
    
    // 설정
    MorphSettings& GetSettings() { return m_settings; }
    const MorphSettings& GetSettings() const { return m_settings; }
    
    // 업데이트
    void Update(float deltaTime);
    
    // 디버깅
    void RenderDebugUI();
    void LogMorphInfo() const;

private:
    struct WeightAnimation
    {
        float startWeight;
        float targetWeight;
        float currentTime;
        float duration;
        bool isActive;
        
        enum class EaseType
        {
            Linear,
            EaseIn,
            EaseOut,
            EaseInOut
        } easeType = EaseType::EaseInOut;
        
        float GetCurrentWeight() const;
    };
    
    struct PresetBlend
    {
        std::unordered_map<std::string, float> startWeights;
        std::unordered_map<std::string, float> targetWeights;
        float currentTime;
        float duration;
        bool isActive;
    };
    
    void CreateMorphedVertexBuffer(UINT maxVertexCount, UINT vertexStride);
    void CreateMorphConstantBuffer();
    void UpdateMorphConstantBuffer();
    
    void ExecuteGPUMorphing(ID3D12GraphicsCommandList* cmdList,
                           ID3D12Resource* baseVertexBuffer,
                           UINT vertexCount, UINT vertexStride);
    
    void ExecuteCPUMorphing(ID3D12Resource* baseVertexBuffer,
                           UINT vertexCount, UINT vertexStride);
    
    void UpdateWeightAnimations(float deltaTime);
    void UpdatePresetBlending(float deltaTime);
    void ApplyGroupConstraints();
    void SmoothWeights(float deltaTime);
    
    // 리소스
    ID3D12Device* m_device = nullptr;
    PipelineStateManager* m_psoManager = nullptr;
    DescriptorAllocator* m_descriptorAllocator = nullptr;
    
    // 모프 데이터
    std::unordered_map<std::string, MorphTarget> m_morphTargets;
    std::unordered_map<std::string, MorphGroup> m_morphGroups;
    std::unordered_map<std::string, MorphPreset> m_presets;
    
    // 가중치 관리
    std::unordered_map<std::string, float> m_currentWeights;
    std::unordered_map<std::string, float> m_targetWeights;    // 부드러운 변화용
    std::unordered_map<std::string, WeightAnimation> m_weightAnimations;
    
    // 프리셋 블렌딩
    PresetBlend m_presetBlend;
    
    // GPU 리소스
    Microsoft::WRL::ComPtr<ID3D12Resource> m_morphedVertexBuffer;
    Microsoft::WRL::ComPtr<ID3D12Resource> m_morphConstantBuffer;
    
    // 상수 버퍼 데이터
    struct MorphConstants
    {
        DirectX::XMFLOAT4 morphWeights[64];  // 최대 64개 모프 타겟 (4개씩 패킹)
        UINT activeMorphCount;
        UINT vertexCount;
        UINT vertexStride;
        float padding;
    };
    MorphConstants* m_constantData = nullptr;
    
    // 설정
    MorphSettings m_settings;
    
    // 캐시
    std::vector<std::string> m_activeMorphs;  // 현재 활성화된 모프들
    bool m_needsUpdate = true;
    
    // 통계
    struct MorphStats
    {
        UINT totalMorphTargets = 0;
        UINT activeMorphTargets = 0;
        UINT totalVerticesProcessed = 0;
        float lastUpdateTime = 0.0f;
    } m_stats;
};

// 모프 타겟 빌더 (편의 클래스)
class MorphTargetBuilder
{
public:
    MorphTargetBuilder(const std::string& name);
    
    MorphTargetBuilder& SetDeltaPositions(const std::vector<DirectX::XMFLOAT3>& deltas);
    MorphTargetBuilder& SetDeltaNormals(const std::vector<DirectX::XMFLOAT3>& deltas);
    MorphTargetBuilder& SetDeltaTangents(const std::vector<DirectX::XMFLOAT3>& deltas);
    MorphTargetBuilder& SetWeight(float weight);
    
    MorphTargetRenderer::MorphTarget Build();

private:
    MorphTargetRenderer::MorphTarget m_target;
};

} // namespace Lunar
