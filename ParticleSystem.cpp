#include "ParticleSystem.h"

#include <d3d12.h>

#include "Logger.h"
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

    // Create a input buffer for the particles
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
    
    D3D12_RESOURCE_DESC inputBufferDesc = {};
    inputBufferDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    inputBufferDesc.Width = sizeof(Particle) * particles.size();
    inputBufferDesc.Height = 1;
    inputBufferDesc.DepthOrArraySize = 1;
    inputBufferDesc.MipLevels = 1;
    inputBufferDesc.Format = DXGI_FORMAT_UNKNOWN;
    inputBufferDesc.SampleDesc.Count = 1;
    inputBufferDesc.SampleDesc.Quality = 0;
    inputBufferDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    inputBufferDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

    THROW_IF_FAILED(device->CreateCommittedResource(
        &defaultHeapProperties,
        D3D12_HEAP_FLAG_NONE,
        &inputBufferDesc,
        D3D12_RESOURCE_STATE_COMMON,
        nullptr, 
        IID_PPV_ARGS(&m_particleInputBuffer)));

    D3D12_HEAP_PROPERTIES uploadHeapProperties = defaultHeapProperties;
    uploadHeapProperties.Type = D3D12_HEAP_TYPE_UPLOAD;

    // upload buffer description is same as the particle buffer
    THROW_IF_FAILED(device->CreateCommittedResource(
        &uploadHeapProperties,
        D3D12_HEAP_FLAG_NONE,
        &inputBufferDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr, 
        IID_PPV_ARGS(&m_uploadBuffer)));

    BYTE* pData = reinterpret_cast<BYTE*>(particles.data());
    THROW_IF_FAILED(m_uploadBuffer->Map(0, nullptr, reinterpret_cast<void**>(&pData)))
    memcpy(pData, particles.data(), sizeof(Particle) * particles.size());
    m_uploadBuffer->Unmap(0, nullptr);

    D3D12_RESOURCE_BARRIER barrier = {};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
    barrier.Transition.pResource = m_particleInputBuffer.Get();
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COMMON;
    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_DEST;
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    commandList->ResourceBarrier(1, &barrier);

    commandList->CopyBufferRegion(
        m_particleInputBuffer.Get(),
        0,
        m_uploadBuffer.Get(),
        0,
        sizeof(Particle) * particles.size()
    );

    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_GENERIC_READ;
    commandList->ResourceBarrier(1, &barrier);

    // Create an output buffer for the particles
    {
        D3D12_RESOURCE_DESC outputBufferDesc = {};
        outputBufferDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
        outputBufferDesc.Width = sizeof(Particle) * particles.size();
        outputBufferDesc.Height = 1;
        outputBufferDesc.DepthOrArraySize = 1;
        outputBufferDesc.MipLevels = 1;
        outputBufferDesc.Format = DXGI_FORMAT_UNKNOWN;
        outputBufferDesc.SampleDesc.Count = 1;
        outputBufferDesc.SampleDesc.Quality = 0;
        outputBufferDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
        outputBufferDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

        D3D12_HEAP_PROPERTIES outputHeapProperties = {};
        outputHeapProperties.Type = D3D12_HEAP_TYPE_DEFAULT;
        outputHeapProperties.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
        outputHeapProperties.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
        outputHeapProperties.CreationNodeMask = 1;
        outputHeapProperties.VisibleNodeMask = 1;

        THROW_IF_FAILED(device->CreateCommittedResource(
            &outputHeapProperties,
            D3D12_HEAP_FLAG_NONE,
            &outputBufferDesc,
            D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
            nullptr, 
            IID_PPV_ARGS(&m_particleOutputBuffer)));
    }
}

int ParticleSystem::AddSRVToDescriptorHeap(ID3D12Device* device, ID3D12DescriptorHeap* descriptorHeap, int descriptorIndex)
{
    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Format = DXGI_FORMAT_UNKNOWN;
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
    srvDesc.Buffer.FirstElement = 0;
    srvDesc.Buffer.NumElements = particles.size();
    srvDesc.Buffer.StructureByteStride = sizeof(Particle);
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;

    D3D12_CPU_DESCRIPTOR_HANDLE handle = descriptorHeap->GetCPUDescriptorHandleForHeapStart();
    handle.ptr += descriptorIndex * device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    device->CreateShaderResourceView(m_particleInputBuffer.Get(), &srvDesc, handle);
    ++descriptorIndex;

    D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
    uavDesc.Format = DXGI_FORMAT_UNKNOWN;
    uavDesc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
    uavDesc.Buffer.FirstElement = 0;
    uavDesc.Buffer.NumElements = particles.size();
    uavDesc.Buffer.StructureByteStride = sizeof(Particle);
    uavDesc.Buffer.CounterOffsetInBytes = 0;
    uavDesc.Buffer.Flags = D3D12_BUFFER_UAV_FLAG_NONE; 

	D3D12_CPU_DESCRIPTOR_HANDLE uavHandle = descriptorHeap->GetCPUDescriptorHandleForHeapStart();
    device->CreateUnorderedAccessView(m_particleOutputBuffer.Get(), nullptr, &uavDesc, uavHandle);

    return descriptorIndex;
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

void ParticleSystem::DrawParticles(ID3D12GraphicsCommandList* commandList)
{
    int particlesToDraw = 0;
    for (const auto& particle : particles)
    {
        if (particle.age < particle.lifetime)
        {
            ++particlesToDraw;
        }
    }
    LOG_DEBUG("Number of particles to draw: {}", particlesToDraw);
}

} // namespace Lunar