#pragma once 

namespace Lunar
{

struct Particle
{
    float velocity[3];
    float position[3];
    float force[3];
    float color[4];
    float lifetime; 
    float age;      
};

class Particle 
{

public:
    void Initialize(ID3D12Device* device, ID3D12GraphicsCommandList* commandList);
    int AddSRVToDescriptorHeap(ID3D12Device* device, ID3D12DescriptorHeap* descriptorHeap, int descriptorIndex);
    void EmitParticles();
    void DrawParticles(ID3D12GraphicsCommandList* commandList);

private:
    void ResetParticles();

    std::vector<Particle> particles;

    Microsoft::WRL::ComPtr<ID3D12Resource> m_particleInputBuffer;
    Microsoft::WRL::ComPtr<ID3D12Resource> m_uploadBuffer;
    Microsoft::WRL::ComPtr<ID3D12Resource> m_particleOutputBuffer;
};

} // namespace Lunar
