// ParticleSystemExample.cpp - 사용 예제

#include "ParticleSystemEnhanced.h"
#include "ParticlePresets.cpp"

namespace Lunar
{

class ParticleEffectManager
{
public:
    void Initialize(ID3D12Device* device, ID3D12GraphicsCommandList* commandList,
                   TextureManager* textureManager, PipelineStateManager* psoManager)
    {
        m_particleSystem = std::make_unique<ParticleSystemEnhanced>();
        m_particleSystem->Initialize(device, commandList, textureManager, psoManager);
        
        // 프리셋 파티클 타입들 추가
        m_particleSystem->AddParticleType(ParticlePresets::CreateFireParticle());
        m_particleSystem->AddParticleType(ParticlePresets::CreateSmokeParticle());
        m_particleSystem->AddParticleType(ParticlePresets::CreateSparkParticle());
        m_particleSystem->AddParticleType(ParticlePresets::CreateMagicParticle());
        m_particleSystem->AddParticleType(ParticlePresets::CreateExplosionParticle());
        m_particleSystem->AddParticleType(ParticlePresets::CreateSnowParticle());
        m_particleSystem->AddParticleType(ParticlePresets::CreateRainParticle());
    }
    
    void CreateCampfire(const DirectX::XMFLOAT3& position)
    {
        // 불 파티클 연속 방출
        m_particleSystem->EmitParticles("Fire", position, 5);
        
        // 연기 파티클 (불보다 약간 위에서)
        DirectX::XMFLOAT3 smokePos = {position.x, position.y + 0.5f, position.z};
        m_particleSystem->EmitParticles("Smoke", smokePos, 2);
        
        // 가끔 스파크 방출
        if (rand() % 10 == 0) // 10% 확률
        {
            m_particleSystem->EmitBurst("Spark", position, 10, 0.2f);
        }
    }
    
    void CreateExplosion(const DirectX::XMFLOAT3& position)
    {
        // 폭발 파티클 버스트
        m_particleSystem->EmitBurst("Explosion", position, 50, 1.0f);
        
        // 스파크 방출
        m_particleSystem->EmitBurst("Spark", position, 100, 2.0f);
        
        // 연기 (약간 지연)
        DirectX::XMFLOAT3 smokePos = {position.x, position.y + 1.0f, position.z};
        m_particleSystem->EmitBurst("Smoke", smokePos, 30, 1.5f);
    }
    
    void CreateMagicSpell(const DirectX::XMFLOAT3& position)
    {
        // 마법 파티클을 원형으로 방출
        for (int i = 0; i < 8; i++)
        {
            float angle = (i / 8.0f) * 2.0f * 3.14159f;
            DirectX::XMFLOAT3 direction = {
                cos(angle),
                0.5f,
                sin(angle)
            };
            m_particleSystem->EmitParticles("Magic", position, 3, direction);
        }
    }
    
    void CreateWeatherEffect(const DirectX::XMFLOAT3& position, const std::string& weatherType)
    {
        if (weatherType == "snow")
        {
            // 넓은 영역에 눈 방출
            for (int i = 0; i < 20; i++)
            {
                DirectX::XMFLOAT3 snowPos = {
                    position.x + (rand() % 20 - 10),
                    position.y + 10.0f,
                    position.z + (rand() % 20 - 10)
                };
                m_particleSystem->EmitParticles("Snow", snowPos, 1);
            }
        }
        else if (weatherType == "rain")
        {
            // 넓은 영역에 비 방출
            for (int i = 0; i < 50; i++)
            {
                DirectX::XMFLOAT3 rainPos = {
                    position.x + (rand() % 30 - 15),
                    position.y + 15.0f,
                    position.z + (rand() % 30 - 15)
                };
                m_particleSystem->EmitParticles("Rain", rainPos, 1);
            }
        }
    }
    
    void Update(float deltaTime, ID3D12GraphicsCommandList* commandList, 
               const DirectX::XMFLOAT3& cameraPosition)
    {
        m_particleSystem->Update(deltaTime, commandList, cameraPosition);
    }
    
    void Render(ID3D12GraphicsCommandList* commandList, const DirectX::XMFLOAT4X4& viewProj)
    {
        m_particleSystem->Render(commandList, viewProj);
    }

private:
    std::unique_ptr<ParticleSystemEnhanced> m_particleSystem;
};

} // namespace Lunar

// MainApp.cpp에서 사용 예제:
/*
void MainApp::Initialize()
{
    // ... 기존 초기화 코드 ...
    
    m_particleEffectManager = std::make_unique<ParticleEffectManager>();
    m_particleEffectManager->Initialize(m_device.Get(), m_commandList.Get(), 
                                       m_textureManager.get(), m_psoManager.get());
}

void MainApp::Update(double dt)
{
    // ... 기존 업데이트 코드 ...
    
    // 파티클 이펙트 업데이트
    m_particleEffectManager->Update(static_cast<float>(dt), m_commandList.Get(), 
                                   m_camera->GetPosition());
    
    // 예제: 캠프파이어 효과
    static float fireTimer = 0.0f;
    fireTimer += dt;
    if (fireTimer > 0.1f) // 0.1초마다 파티클 방출
    {
        m_particleEffectManager->CreateCampfire({0.0f, 0.0f, 0.0f});
        fireTimer = 0.0f;
    }
    
    // 예제: 마우스 클릭으로 폭발 효과
    if (GetAsyncKeyState(VK_LBUTTON) & 0x8000)
    {
        // 마우스 위치를 월드 좌표로 변환하여 폭발 생성
        // m_particleEffectManager->CreateExplosion(mouseWorldPos);
    }
}

void MainApp::Render(double dt)
{
    // ... 기존 렌더링 코드 ...
    
    // 불투명 객체들 렌더링 후
    
    // 파티클 렌더링 (투명 객체들과 함께)
    DirectX::XMFLOAT4X4 viewProj = m_camera->GetViewProjectionMatrix();
    m_particleEffectManager->Render(m_commandList.Get(), viewProj);
}
*/
