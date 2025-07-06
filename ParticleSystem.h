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
    int AddSRVToDescriptorHeap(ID3D12Device* device, ID3D12DescriptorHeap* descriptorHeap, int descriptorIndex);
    void EmitParticles(const DirectX::XMFLOAT3& position);
    void DrawParticles(ID3D12GraphicsCommandList* commandList);
    
    // 버퍼 접근 메서드 추가
    ID3D12Resource* GetInputBuffer() const { return m_particleInputBuffer.Get(); }
    ID3D12Resource* GetOutputBuffer() const { return m_particleOutputBuffer.Get(); }
    void SwapBuffers() { m_particleInputBuffer.Swap(m_particleOutputBuffer); }
    bool IsFirstFrame() const { return m_isFirstFrame; }
    void SetFirstFrameComplete() { m_isFirstFrame = false; }

private:
    void ResetParticles(const DirectX::XMFLOAT3& position = {0.0f, 0.0f, 0.0f});

    std::vector<Particle> particles;
    bool m_isFirstFrame = true;

    Microsoft::WRL::ComPtr<ID3D12Resource> m_particleInputBuffer;
    Microsoft::WRL::ComPtr<ID3D12Resource> m_uploadBuffer;
    Microsoft::WRL::ComPtr<ID3D12Resource> m_particleOutputBuffer;
};

} // namespace Lunar
