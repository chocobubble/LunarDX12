#pragma once 
#include <d3d12.h>
#include <DirectXMath.h>
#include <vector>
#include <wrl/client.h>
#include <string>

namespace Lunar
{

class TextureManager;
class PipelineStateManager;

class ParticleSystemEnhanced
{
public:
    // 블렌딩 모드 열거형
    enum class BlendMode
    {
        Alpha,          // 투명 블렌딩 (연기, 구름)
        Additive,       // 가산 블렌딩 (불, 폭발)
        Multiplicative, // 곱셈 블렌딩 (그림자)
        Screen,         // 스크린 블렌딩 (부드러운 빛)
        Subtractive     // 감산 블렌딩 (특수 효과)
    };
    
    // 파티클 타입별 설정
    struct ParticleType
    {
        std::string name;
        BlendMode blendMode = BlendMode::Alpha;
        std::string texturePath;
        DirectX::XMFLOAT2 sizeRange = {0.01f, 0.05f};      // 최소/최대 크기
        DirectX::XMFLOAT2 lifetimeRange = {1.0f, 5.0f};    // 최소/최대 수명
        DirectX::XMFLOAT3 velocityBase = {0.0f, 1.0f, 0.0f}; // 기본 속도
        DirectX::XMFLOAT3 velocityVariation = {1.0f, 0.5f, 1.0f}; // 속도 변화량
        DirectX::XMFLOAT4 colorStart = {1.0f, 1.0f, 1.0f, 1.0f}; // 시작 색상
        DirectX::XMFLOAT4 colorEnd = {1.0f, 1.0f, 1.0f, 0.0f};   // 끝 색상
        float gravity = -9.81f;
        float drag = 0.1f;          // 공기 저항
        bool fadeOverLifetime = true; // 수명에 따른 페이드
        bool scaleOverLifetime = false; // 수명에 따른 크기 변화
    };

private:
    struct Particle
    {
        DirectX::XMFLOAT3 position;
        DirectX::XMFLOAT3 velocity;
        DirectX::XMFLOAT3 force;
        DirectX::XMFLOAT4 color;
        float lifetime;
        float age;
        float size;
        float rotation;         // 회전각
        float rotationSpeed;    // 회전 속도
        UINT typeIndex;         // 파티클 타입 인덱스
        float distanceToCamera; // 카메라로부터의 거리 (정렬용)
    };

public:
    ParticleSystemEnhanced();
    ~ParticleSystemEnhanced();

    void Initialize(ID3D12Device* device, ID3D12GraphicsCommandList* commandList,
                   TextureManager* textureManager, PipelineStateManager* psoManager);
    void Shutdown();
    
    // 파티클 타입 관리
    void AddParticleType(const ParticleType& type);
    void RemoveParticleType(const std::string& name);
    bool HasParticleType(const std::string& name) const;
    
    // 파티클 방출
    void EmitParticles(const std::string& typeName, const DirectX::XMFLOAT3& position, 
                      UINT count = 1, const DirectX::XMFLOAT3& direction = {0.0f, 1.0f, 0.0f});
    void EmitBurst(const std::string& typeName, const DirectX::XMFLOAT3& position, 
                   UINT count, float radius = 0.0f);
    
    // 업데이트 및 렌더링
    void Update(float deltaTime, ID3D12GraphicsCommandList* commandList, 
               const DirectX::XMFLOAT3& cameraPosition);
    void Render(ID3D12GraphicsCommandList* commandList, const DirectX::XMFLOAT4X4& viewProj);
    
    // 설정
    void SetMaxParticles(UINT maxParticles);
    void SetSortingEnabled(bool enabled) { m_enableSorting = enabled; }
    void SetDepthSoftening(bool enabled) { m_enableDepthSoftening = enabled; }
    
    // 통계
    UINT GetActiveParticleCount() const;
    UINT GetParticleCount(const std::string& typeName) const;
    
    // 디버깅
    void RenderDebugInfo();

private:
    void CreateBuffers();
    void CreateBlendStates();
    void UpdateParticles(float deltaTime, const DirectX::XMFLOAT3& cameraPosition);
    void SortParticles(); // 깊이 정렬
    void UploadParticlesToGPU(ID3D12GraphicsCommandList* commandList);
    void RenderParticlesByType(ID3D12GraphicsCommandList* commandList, 
                              const DirectX::XMFLOAT4X4& viewProj);
    
    // 파티클 생성 헬퍼
    Particle CreateParticle(const ParticleType& type, const DirectX::XMFLOAT3& position,
                           const DirectX::XMFLOAT3& direction);
    
    // 색상 보간
    DirectX::XMFLOAT4 InterpolateColor(const DirectX::XMFLOAT4& start, 
                                      const DirectX::XMFLOAT4& end, float t);
    
    // 리소스
    ID3D12Device* m_device = nullptr;
    TextureManager* m_textureManager = nullptr;
    PipelineStateManager* m_psoManager = nullptr;
    
    // 파티클 데이터
    std::vector<Particle> m_particles;
    std::vector<ParticleType> m_particleTypes;
    std::unordered_map<std::string, UINT> m_typeNameToIndex;
    
    // GPU 리소스
    Microsoft::WRL::ComPtr<ID3D12Resource> m_particleBuffers[2];
    Microsoft::WRL::ComPtr<ID3D12Resource> m_uploadBuffers[2];
    Microsoft::WRL::ComPtr<ID3D12Resource> m_constantBuffer;
    
    // 블렌드 스테이트들
    std::unordered_map<BlendMode, Microsoft::WRL::ComPtr<ID3D12PipelineState>> m_blendPSOs;
    
    // 설정
    UINT m_maxParticles = 10000;
    UINT m_currentBuffer = 0;
    bool m_enableSorting = true;
    bool m_enableDepthSoftening = true;
    bool m_needsGPUUpdate = true;
    
    // 상수 버퍼 데이터
    struct ParticleConstants
    {
        DirectX::XMFLOAT4X4 viewProjection;
        DirectX::XMFLOAT3 cameraPosition;
        float deltaTime;
        DirectX::XMFLOAT2 screenSize;
        float softParticleDistance;
        float padding;
    };
    ParticleConstants* m_constantData = nullptr;
    
    // 통계
    struct ParticleStats
    {
        UINT totalParticles = 0;
        UINT activeParticles = 0;
        UINT particlesEmittedThisFrame = 0;
        float updateTime = 0.0f;
        float renderTime = 0.0f;
    } m_stats;
};

// 파티클 이펙트 프리셋들
namespace ParticlePresets
{
    ParticleSystemEnhanced::ParticleType CreateFireParticle();
    ParticleSystemEnhanced::ParticleType CreateSmokeParticle();
    ParticleSystemEnhanced::ParticleType CreateSparkParticle();
    ParticleSystemEnhanced::ParticleType CreateMagicParticle();
    ParticleSystemEnhanced::ParticleType CreateExplosionParticle();
    ParticleSystemEnhanced::ParticleType CreateSnowParticle();
    ParticleSystemEnhanced::ParticleType CreateRainParticle();
}

} // namespace Lunar
