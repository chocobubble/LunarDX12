#pragma once 
#include <d3d12.h>
#include <DirectXMath.h>
#include <vector>
#include <wrl/client.h>

namespace Lunar
{

class ParticleSystem
{
	struct Particle
	{
		float position[3]; 
		float velocity[3];
		float force[3];
		float color[4];
		float lifetime; 
		float age;      
	};
public:
    void Initialize(ID3D12Device* device, ID3D12GraphicsCommandList* commandList);
    void EmitParticles(const DirectX::XMFLOAT3& position);
    void DrawParticles(ID3D12GraphicsCommandList* commandList);
    
    void            Update(float deltaTime, ID3D12GraphicsCommandList* commandList);
	ID3D12Resource* GetCurrentBufferResource() { return m_particleBuffers[m_currentBuffer].Get(); }
    
    int GetActiveParticleCount() const;
private:
    void ResetParticles(const DirectX::XMFLOAT3& position = {0.0f, 0.0f, 0.0f});
	void UploadParticlesToGPU(ID3D12GraphicsCommandList* commandList);

    std::vector<Particle> particles;
    bool m_isFirstFrame = true;
	bool m_resetFlag = false;

    int m_currentBuffer = 0;

    Microsoft::WRL::ComPtr<ID3D12Resource> m_particleBuffers[2];
    Microsoft::WRL::ComPtr<ID3D12Resource> m_uploadBuffers[2];
};

} // namespace Lunar
