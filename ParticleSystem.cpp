#include "ParticleSystem.h"

#include <d3d12.h>

#include "Logger.h"
#include "LunarConstants.h"
#include "Utils.h"

using namespace DirectX;

namespace Lunar
{

void ParticleSystem::Initialize(ID3D12Device* device, ID3D12GraphicsCommandList* commandList)
{
    // Create 500 particles with random positions and velocities and colors 
    // 1. initial velocities are towards the up direction of the screen
    // 2. for now, initial positions are in the center of the world
    // 3. colors are random
    particles.resize(512);
    ResetParticles();

    // Create double buffers for the particles
    // 1. Create a default heap for the particle buffer
    // 2. Create a resource description for the particle buffer
    // 3. Create the committed resource for the particle buffer
    // 4. Create an upload buffer to copy the particle data to the GPU
    // 5. Map the upload buffer to copy the particle data
    // 6. With transition barrier, copy the data from the upload buffer to the default buffer
    // 7. Transition the particle buffer to the generic read state

    D3D12_HEAP_PROPERTIES defaultHeapProperties = {};
    defaultHeapProperties.Type = D3D12_HEAP_TYPE_DEFAULT;
    defaultHeapProperties.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
    defaultHeapProperties.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
    defaultHeapProperties.CreationNodeMask = 1;
    defaultHeapProperties.VisibleNodeMask = 1;
    
    D3D12_RESOURCE_DESC bufferDesc = {};
    bufferDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    bufferDesc.Width = sizeof(Particle) * particles.size();
    bufferDesc.Height = 1;
    bufferDesc.DepthOrArraySize = 1;
    bufferDesc.MipLevels = 1;
    bufferDesc.Format = DXGI_FORMAT_UNKNOWN;
    bufferDesc.SampleDesc.Count = 1;
    bufferDesc.SampleDesc.Quality = 0;
    bufferDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    bufferDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

    THROW_IF_FAILED(device->CreateCommittedResource(
        &defaultHeapProperties,
        D3D12_HEAP_FLAG_NONE,
        &bufferDesc,
        D3D12_RESOURCE_STATE_COMMON,
        nullptr, 
        IID_PPV_ARGS(&m_particleBuffers[0])));

    THROW_IF_FAILED(device->CreateCommittedResource(
        &defaultHeapProperties,
        D3D12_HEAP_FLAG_NONE,
        &bufferDesc,
        D3D12_RESOURCE_STATE_COMMON,
        nullptr, 
        IID_PPV_ARGS(&m_particleBuffers[1])));

    D3D12_HEAP_PROPERTIES uploadHeapProperties = defaultHeapProperties;
    uploadHeapProperties.Type = D3D12_HEAP_TYPE_UPLOAD;
    
    D3D12_RESOURCE_DESC uploadBufferDesc = bufferDesc;
    uploadBufferDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

    THROW_IF_FAILED(device->CreateCommittedResource(
        &uploadHeapProperties,
        D3D12_HEAP_FLAG_NONE,
        &uploadBufferDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr, 
        IID_PPV_ARGS(&m_uploadBuffers[0])));

    THROW_IF_FAILED(device->CreateCommittedResource(
        &uploadHeapProperties,
        D3D12_HEAP_FLAG_NONE,
        &uploadBufferDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr, 
        IID_PPV_ARGS(&m_uploadBuffers[1])));

    for (int i = 0; i < 2; ++i) {
        BYTE* pData = nullptr;
        THROW_IF_FAILED(m_uploadBuffers[i]->Map(0, nullptr, reinterpret_cast<void**>(&pData)));
        memcpy(pData, particles.data(), sizeof(Particle) * particles.size());
        m_uploadBuffers[i]->Unmap(0, nullptr);

        D3D12_RESOURCE_BARRIER barrier = {};
        barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
        barrier.Transition.pResource = m_particleBuffers[i].Get();
        barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COMMON;
        barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_DEST;
        barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        commandList->ResourceBarrier(1, &barrier);

        commandList->CopyBufferRegion(
            m_particleBuffers[i].Get(),
            0,
            m_uploadBuffers[i].Get(),
            0,
            sizeof(Particle) * particles.size()
        );

        barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
        barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_GENERIC_READ;
        commandList->ResourceBarrier(1, &barrier);
    }
}


void ParticleSystem::EmitParticles(const XMFLOAT3& position)
{
    ResetParticles(position);
}

void ParticleSystem::ResetParticles(const XMFLOAT3& position)
{
    for (auto& particle : particles)
    {
        particle.position[0] = position.x; 
        particle.position[1] = position.y; 
        particle.position[2] = position.z; 

        particle.velocity[0] = static_cast<float>(rand() % 100) / 100.0f; 
        particle.velocity[1] = static_cast<float>(rand() % 100) / 100.0f + 1.0f;
        particle.velocity[2] = static_cast<float>(rand() % 100) / 100.0f;

        particle.force[0] = 0.0f;
        particle.force[1] = -9.81f; // Gravity force in the negative Y direction
        particle.force[2] = 0.0f; 

        particle.color[0] = static_cast<float>(rand() % 256) / 255.0f;
        particle.color[1] = static_cast<float>(rand() % 256) / 255.0f;
        particle.color[2] = static_cast<float>(rand() % 256) / 255.0f;
        particle.color[3] = 1.0f;

        particle.lifetime = static_cast<float>(rand() % 10 + 1); 
        particle.age = 0.0f; 
    }
}

int ParticleSystem::GetActiveParticleCount() const
{
    int activeCount = 0;
    for (const auto& particle : particles) {
        if (particle.age < particle.lifetime) {
            ++activeCount;
        }
    }
    return activeCount;
}

void ParticleSystem::DrawParticles(ID3D12GraphicsCommandList* commandList)
{
    int activeParticles = GetActiveParticleCount();

    if (activeParticles > 0) {
        commandList->SetGraphicsRootShaderResourceView(
            LunarConstants::PARTICLE_SRV_ROOT_PARAMETER_INDEX, 
            m_particleBuffers[m_currentBuffer]->GetGPUVirtualAddress()
        );
        commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_POINTLIST);
        commandList->DrawInstanced(particles.size(), 1, 0, 0);
    }
}

void ParticleSystem::Update(float deltaTime, ID3D12GraphicsCommandList* commandList)
{
    int inputBufferIndex = m_currentBuffer;
    int outputBufferIndex = 1 - m_currentBuffer;
    
    D3D12_RESOURCE_BARRIER barrier = {};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
    barrier.Transition.pResource = m_particleBuffers[outputBufferIndex].Get();
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_GENERIC_READ;
    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
    commandList->ResourceBarrier(1, &barrier);
    
    commandList->SetComputeRootShaderResourceView(
        LunarConstants::PARTICLE_SRV_ROOT_PARAMETER_INDEX, 
        m_particleBuffers[inputBufferIndex]->GetGPUVirtualAddress()
    );
    
    commandList->SetComputeRootUnorderedAccessView(
        LunarConstants::PARTICLE_UAV_ROOT_PARAMETER_INDEX, 
        m_particleBuffers[outputBufferIndex]->GetGPUVirtualAddress()
    );
    
    int particlesToDraw = 64;
    commandList->Dispatch(particlesToDraw, 1, 1);

    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_GENERIC_READ;
    commandList->ResourceBarrier(1, &barrier);
    
    m_currentBuffer = outputBufferIndex;
}

} // namespace Lunar