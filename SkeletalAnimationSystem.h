#pragma once
#include <DirectXMath.h>
#include <string>
#include <vector>
#include <unordered_map>
#include <memory>
#include <d3d12.h>
#include <wrl.h>

#include "ModelLoader.h" // Skeleton, AnimationClip 등 사용

namespace Lunar
{

class SkeletalAnimationSystem
{
public:
    struct AnimationState
    {
        std::string name;
        const AnimationClip* clip = nullptr;
        float currentTime = 0.0f;
        float speed = 1.0f;
        bool isLooping = true;
        bool isPlaying = false;
        float weight = 1.0f;  // 블렌딩용 가중치
        
        // 애니메이션 이벤트
        struct AnimationEvent
        {
            float time;
            std::string eventName;
            std::string parameter;
        };
        std::vector<AnimationEvent> events;
        std::vector<bool> eventTriggered; // 이벤트 발생 여부 추적
    };
    
    struct BlendNode
    {
        enum class Type
        {
            Clip,           // 단일 애니메이션 클립
            Blend2D,        // 2D 블렌딩 (예: 걷기/뛰기)
            AdditiveBlend,  // 가산 블렌딩
            Override        // 특정 본만 오버라이드
        };
        
        Type type;
        std::string name;
        std::vector<std::unique_ptr<BlendNode>> children;
        
        // 블렌딩 파라미터
        DirectX::XMFLOAT2 blendPosition = {0.0f, 0.0f}; // 2D 블렌딩용
        float blendWeight = 1.0f;
        
        // 클립 노드용
        AnimationState* animationState = nullptr;
        
        // 오버라이드 노드용
        std::vector<std::string> targetBones;
        DirectX::XMFLOAT4X4 overrideTransform;
    };

    SkeletalAnimationSystem();
    ~SkeletalAnimationSystem();

    void Initialize(ID3D12Device* device, const Skeleton& skeleton);
    void Shutdown();
    
    // 애니메이션 관리
    void AddAnimationClip(const AnimationClip& clip);
    void RemoveAnimationClip(const std::string& name);
    bool HasAnimationClip(const std::string& name) const;
    
    // 애니메이션 재생 제어
    void PlayAnimation(const std::string& animationName, bool loop = true, float speed = 1.0f);
    void StopAnimation(const std::string& animationName);
    void PauseAnimation(const std::string& animationName);
    void ResumeAnimation(const std::string& animationName);
    
    // 블렌딩
    void BlendAnimations(const std::string& fromAnim, const std::string& toAnim, float blendTime);
    void SetBlendWeight(const std::string& animationName, float weight);
    void CrossFade(const std::string& fromAnim, const std::string& toAnim, float duration);
    
    // 업데이트
    void Update(float deltaTime);
    
    // 본 변환 행렬 계산
    std::vector<DirectX::XMFLOAT4X4> GetBoneTransforms() const;
    std::vector<DirectX::XMFLOAT4X4> GetFinalBoneTransforms() const; // 오프셋 적용된 최종 변환
    
    // GPU 리소스 업데이트
    void UpdateBoneBuffer(ID3D12GraphicsCommandList* cmdList);
    ID3D12Resource* GetBoneBuffer() const { return m_boneBuffer.Get(); }
    D3D12_GPU_VIRTUAL_ADDRESS GetBoneBufferGPUAddress() const;
    
    // 본 정보 접근
    const Skeleton& GetSkeleton() const { return m_skeleton; }
    int GetBoneIndex(const std::string& boneName) const;
    DirectX::XMFLOAT4X4 GetBoneTransform(int boneIndex) const;
    DirectX::XMFLOAT4X4 GetBoneWorldTransform(int boneIndex) const;
    
    // 애니메이션 상태 접근
    AnimationState* GetAnimationState(const std::string& name);
    const AnimationState* GetAnimationState(const std::string& name) const;
    std::vector<std::string> GetActiveAnimations() const;
    
    // 고급 기능
    void SetBoneOverride(const std::string& boneName, const DirectX::XMFLOAT4X4& transform);
    void ClearBoneOverride(const std::string& boneName);
    void SetRootMotion(bool enabled) { m_enableRootMotion = enabled; }
    DirectX::XMFLOAT3 GetRootMotionDelta() const { return m_rootMotionDelta; }
    
    // 애니메이션 이벤트
    using AnimationEventCallback = std::function<void(const std::string& eventName, const std::string& parameter)>;
    void SetEventCallback(AnimationEventCallback callback) { m_eventCallback = callback; }
    void AddAnimationEvent(const std::string& animationName, float time, const std::string& eventName, const std::string& parameter = "");
    
    // IK (Inverse Kinematics) - 기본 구현
    void SetIKTarget(const std::string& boneName, const DirectX::XMFLOAT3& targetPosition);
    void ClearIKTarget(const std::string& boneName);
    void SolveIK();
    
    // 디버깅
    void RenderDebugSkeleton(ID3D12GraphicsCommandList* cmdList, const DirectX::XMFLOAT4X4& worldTransform);
    void LogAnimationInfo() const;

private:
    struct BoneTransform
    {
        DirectX::XMFLOAT3 position;
        DirectX::XMFLOAT4 rotation; // 쿼터니언
        DirectX::XMFLOAT3 scale;
        
        DirectX::XMFLOAT4X4 ToMatrix() const;
        static BoneTransform Lerp(const BoneTransform& a, const BoneTransform& b, float t);
        static BoneTransform Slerp(const BoneTransform& a, const BoneTransform& b, float t);
    };
    
    struct IKChain
    {
        std::vector<int> boneIndices;
        DirectX::XMFLOAT3 targetPosition;
        int iterations = 10;
        float tolerance = 0.01f;
    };
    
    // 애니메이션 계산
    BoneTransform CalculateBoneTransform(const AnimationState& state, int boneIndex, float time) const;
    BoneTransform BlendBoneTransforms(const std::vector<std::pair<BoneTransform, float>>& transforms) const;
    void CalculateLocalTransforms(std::vector<BoneTransform>& localTransforms) const;
    void CalculateGlobalTransforms(const std::vector<BoneTransform>& localTransforms, std::vector<DirectX::XMFLOAT4X4>& globalTransforms) const;
    
    // 블렌딩 트리 처리
    BoneTransform EvaluateBlendNode(const BlendNode& node, int boneIndex, float time) const;
    
    // 루트 모션 계산
    void CalculateRootMotion(float deltaTime);
    
    // 애니메이션 이벤트 처리
    void ProcessAnimationEvents(AnimationState& state, float oldTime, float newTime);
    
    // IK 솔버
    void SolveTwoJointIK(const IKChain& chain);
    void SolveCCDIK(const IKChain& chain);
    
    // GPU 리소스 생성
    void CreateBoneBuffer();
    
    // 리소스
    ID3D12Device* m_device = nullptr;
    Microsoft::WRL::ComPtr<ID3D12Resource> m_boneBuffer;
    DirectX::XMFLOAT4X4* m_boneBufferData = nullptr;
    
    // 스켈레톤 데이터
    Skeleton m_skeleton;
    
    // 애니메이션 데이터
    std::unordered_map<std::string, AnimationClip> m_animationClips;
    std::unordered_map<std::string, std::unique_ptr<AnimationState>> m_animationStates;
    
    // 블렌딩 트리
    std::unique_ptr<BlendNode> m_blendTree;
    
    // 본 오버라이드
    std::unordered_map<std::string, DirectX::XMFLOAT4X4> m_boneOverrides;
    
    // IK 체인들
    std::unordered_map<std::string, IKChain> m_ikChains;
    
    // 루트 모션
    bool m_enableRootMotion = false;
    DirectX::XMFLOAT3 m_rootMotionDelta = {0.0f, 0.0f, 0.0f};
    DirectX::XMFLOAT3 m_previousRootPosition = {0.0f, 0.0f, 0.0f};
    
    // 이벤트 콜백
    AnimationEventCallback m_eventCallback;
    
    // 캐시된 변환 행렬들
    mutable std::vector<DirectX::XMFLOAT4X4> m_boneTransforms;
    mutable std::vector<DirectX::XMFLOAT4X4> m_finalBoneTransforms;
    mutable bool m_transformsDirty = true;
    
    // 상수
    static constexpr UINT MAX_BONES = 256;
    static constexpr float ANIMATION_BLEND_EPSILON = 0.001f;
};

} // namespace Lunar
