#include "PostProcessManager.h"
#include "Logger.h"
#include "Utils.h"
#include "imgui.h"
#include <d3dcompiler.h>

namespace Lunar
{

bool PostProcessManager::Initialize(ID3D12Device* device, UINT width, UINT height)
{
    LOG_FUNCTION_ENTRY();
    
    m_device = device;
    m_width = width;
    m_height = height;
    
    m_srvUavDescriptorSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    m_rtvDescriptorSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
    m_dsvDescriptorSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);
    
    m_constants.textureWidth = width;
    m_constants.textureHeight = height;
    m_constants.texelSizeX = 1.0f / width;
    m_constants.texelSizeY = 1.0f / height;
    
    if (!CreateDescriptorHeaps()) return false;
    if (!CreateTextures()) return false;
    if (!CreateDepthBuffer()) return false;
    if (!CreateConstantBuffer()) return false;
    if (!CreateRootSignature()) return false;
    if (!CreatePipelineStates()) return false;
    
    LOG_FUNCTION_EXIT();
    return true;
}

void PostProcessManager::Shutdown()
{
    if (m_constantBufferData)
    {
        m_constantBuffer->Unmap(0, nullptr);
        m_constantBufferData = nullptr;
    }
}

bool PostProcessManager::CreateDescriptorHeaps()
{
    D3D12_DESCRIPTOR_HEAP_DESC srvUavHeapDesc = {};
    srvUavHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    srvUavHeapDesc.NumDescriptors = 64;
    srvUavHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
    
    HRESULT hr = m_device->CreateDescriptorHeap(&srvUavHeapDesc, IID_PPV_ARGS(&m_srvUavHeap));
    if (FAILED(hr))
    {
        LOG_ERROR("Failed to create SRV/UAV descriptor heap");
        return false;
    }
    
    D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc = {};
    rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
    rtvHeapDesc.NumDescriptors = 16;
    rtvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
    
    hr = m_device->CreateDescriptorHeap(&rtvHeapDesc, IID_PPV_ARGS(&m_rtvHeap));
    if (FAILED(hr))
    {
        LOG_ERROR("Failed to create RTV descriptor heap");
        return false;
    }
    
    D3D12_DESCRIPTOR_HEAP_DESC dsvHeapDesc = {};
    dsvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
    dsvHeapDesc.NumDescriptors = 4;
    dsvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
    
    hr = m_device->CreateDescriptorHeap(&dsvHeapDesc, IID_PPV_ARGS(&m_dsvHeap));
    if (FAILED(hr))
    {
        LOG_ERROR("Failed to create DSV descriptor heap");
        return false;
    }
    
    return true;
}

bool PostProcessManager::CreateTextures()
{
    if (!CreateComputeTexture(DXGI_FORMAT_R16G16B16A16_FLOAT, m_hdrBuffer[0]))
    {
        LOG_ERROR("Failed to create HDR buffer A");
        return false;
    }
    
    if (!CreateComputeTexture(DXGI_FORMAT_R16G16B16A16_FLOAT, m_hdrBuffer[1]))
    {
        LOG_ERROR("Failed to create HDR buffer B");
        return false;
    }
    
    if (!CreateComputeTexture(DXGI_FORMAT_R8G8B8A8_UNORM, m_ldrBuffer))
    {
        LOG_ERROR("Failed to create LDR buffer");
        return false;
    }
    
    LOG_INFO("Created optimized texture set: 2 HDR (16MB each) + 1 LDR (8MB) = 40MB total");
    return true;
}

bool PostProcessManager::CreateComputeTexture(DXGI_FORMAT format, ComputeTexture& texture)
{
    D3D12_RESOURCE_DESC textureDesc = {};
    textureDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    textureDesc.Width = m_width;
    textureDesc.Height = m_height;
    textureDesc.DepthOrArraySize = 1;
    textureDesc.MipLevels = 1;
    textureDesc.Format = format;
    textureDesc.SampleDesc.Count = 1;
    textureDesc.SampleDesc.Quality = 0;
    textureDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
    textureDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS | D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
    
    D3D12_HEAP_PROPERTIES heapProps = {};
    heapProps.Type = D3D12_HEAP_TYPE_DEFAULT;
    heapProps.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
    heapProps.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
    
    HRESULT hr = m_device->CreateCommittedResource(
        &heapProps,
        D3D12_HEAP_FLAG_NONE,
        &textureDesc,
        D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
        nullptr,
        IID_PPV_ARGS(&texture.texture));
    
    if (FAILED(hr))
    {
        LOG_ERROR("Failed to create compute texture");
        return false;
    }
    
    texture.format = format;
    texture.width = m_width;
    texture.height = m_height;
    
    texture.srvHandle = AllocateSrvUavDescriptor();
    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Format = format;
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Texture2D.MipLevels = 1;
    srvDesc.Texture2D.MostDetailedMip = 0;
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    
    m_device->CreateShaderResourceView(texture.texture.Get(), &srvDesc, texture.srvHandle);
    
    texture.uavHandle = AllocateSrvUavDescriptor();
    D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
    uavDesc.Format = format;
    uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
    uavDesc.Texture2D.MipSlice = 0;
    
    m_device->CreateUnorderedAccessView(texture.texture.Get(), nullptr, &uavDesc, texture.uavHandle);
    
    return true;
}

bool PostProcessManager::CreateDepthBuffer()
{
    // Create depth buffer resource
    D3D12_RESOURCE_DESC depthDesc = {};
    depthDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    depthDesc.Width = m_width;
    depthDesc.Height = m_height;
    depthDesc.DepthOrArraySize = 1;
    depthDesc.MipLevels = 1;
    depthDesc.Format = DXGI_FORMAT_R24G8_TYPELESS;
    depthDesc.SampleDesc.Count = 1;
    depthDesc.SampleDesc.Quality = 0;
    depthDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
    depthDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
    
    D3D12_CLEAR_VALUE clearValue = {};
    clearValue.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
    clearValue.DepthStencil.Depth = 1.0f;
    clearValue.DepthStencil.Stencil = 0;
    
    D3D12_HEAP_PROPERTIES heapProps = {};
    heapProps.Type = D3D12_HEAP_TYPE_DEFAULT;
    
    HRESULT hr = m_device->CreateCommittedResource(
        &heapProps,
        D3D12_HEAP_FLAG_NONE,
        &depthDesc,
        D3D12_RESOURCE_STATE_DEPTH_WRITE,
        &clearValue,
        IID_PPV_ARGS(&m_depthBuffer));
    
    if (FAILED(hr))
    {
        LOG_ERROR("Failed to create depth buffer");
        return false;
    }
    
    // Create DSV
    m_depthDSV = AllocateDsvDescriptor();
    D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc = {};
    dsvDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
    dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
    dsvDesc.Texture2D.MipSlice = 0;
    
    m_device->CreateDepthStencilView(m_depthBuffer.Get(), &dsvDesc, m_depthDSV);
    
    // Create SRV for depth
    m_depthSRV = AllocateSrvUavDescriptor();
    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Format = DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Texture2D.MipLevels = 1;
    srvDesc.Texture2D.MostDetailedMip = 0;
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    
    m_device->CreateShaderResourceView(m_depthBuffer.Get(), &srvDesc, m_depthSRV);
    
    return true;
}

bool PostProcessManager::CreateConstantBuffer()
{
    // Create constant buffer
    UINT constantBufferSize = (sizeof(PostProcessConstants) + 255) & ~255; // 256-byte aligned
    
    D3D12_RESOURCE_DESC cbDesc = {};
    cbDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    cbDesc.Width = constantBufferSize;
    cbDesc.Height = 1;
    cbDesc.DepthOrArraySize = 1;
    cbDesc.MipLevels = 1;
    cbDesc.Format = DXGI_FORMAT_UNKNOWN;
    cbDesc.SampleDesc.Count = 1;
    cbDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    cbDesc.Flags = D3D12_RESOURCE_FLAG_NONE;
    
    D3D12_HEAP_PROPERTIES heapProps = {};
    heapProps.Type = D3D12_HEAP_TYPE_UPLOAD;
    heapProps.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
    heapProps.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
    
    HRESULT hr = m_device->CreateCommittedResource(
        &heapProps,
        D3D12_HEAP_FLAG_NONE,
        &cbDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(&m_constantBuffer));
    
    if (FAILED(hr))
    {
        LOG_ERROR("Failed to create constant buffer");
        return false;
    }
    
    // Map constant buffer
    D3D12_RANGE readRange = {};
    hr = m_constantBuffer->Map(0, &readRange, &m_constantBufferData);
    if (FAILED(hr))
    {
        LOG_ERROR("Failed to map constant buffer");
        return false;
    }
    
    return true;
}

D3D12_CPU_DESCRIPTOR_HANDLE PostProcessManager::AllocateSrvUavDescriptor()
{
    D3D12_CPU_DESCRIPTOR_HANDLE handle = m_srvUavHeap->GetCPUDescriptorHandleForHeapStart();
    handle.ptr += m_currentSrvUavIndex * m_srvUavDescriptorSize;
    m_currentSrvUavIndex++;
    return handle;
}

D3D12_CPU_DESCRIPTOR_HANDLE PostProcessManager::AllocateRtvDescriptor()
{
    D3D12_CPU_DESCRIPTOR_HANDLE handle = m_rtvHeap->GetCPUDescriptorHandleForHeapStart();
    handle.ptr += m_currentRtvIndex * m_rtvDescriptorSize;
    m_currentRtvIndex++;
    return handle;
}

D3D12_CPU_DESCRIPTOR_HANDLE PostProcessManager::AllocateDsvDescriptor()
{
    D3D12_CPU_DESCRIPTOR_HANDLE handle = m_dsvHeap->GetCPUDescriptorHandleForHeapStart();
    handle.ptr += m_currentDsvIndex * m_dsvDescriptorSize;
    m_currentDsvIndex++;
    return handle;
}

D3D12_CPU_DESCRIPTOR_HANDLE PostProcessManager::GetSceneRTV() const
{
    // Create RTV for scene texture if needed
    static bool rtvCreated = false;
    static D3D12_CPU_DESCRIPTOR_HANDLE sceneRTV = {};
    
    if (!rtvCreated)
    {
        sceneRTV = m_rtvHeap->GetCPUDescriptorHandleForHeapStart();
        sceneRTV.ptr += m_currentRtvIndex * m_rtvDescriptorSize;
        
        D3D12_RENDER_TARGET_VIEW_DESC rtvDesc = {};
        rtvDesc.Format = m_sceneTexture.format;
        rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
        rtvDesc.Texture2D.MipSlice = 0;
        
        m_device->CreateRenderTargetView(m_sceneTexture.texture.Get(), &rtvDesc, sceneRTV);
        rtvCreated = true;
    }
    
    return sceneRTV;
}

D3D12_CPU_DESCRIPTOR_HANDLE PostProcessManager::GetSceneDSV() const
{
    return m_depthDSV;
}

void PostProcessManager::UpdateConstantBuffer()
{
    memcpy(m_constantBufferData, &m_constants, sizeof(PostProcessConstants));
}

UINT PostProcessManager::GetDispatchSize(UINT textureSize, UINT threadGroupSize) const
{
    return (textureSize + threadGroupSize - 1) / threadGroupSize;
}

void PostProcessManager::TransitionResource(
    ID3D12GraphicsCommandList* cmdList,
    ID3D12Resource* resource,
    D3D12_RESOURCE_STATES before,
    D3D12_RESOURCE_STATES after)
{
    if (before == after) return;
    
    D3D12_RESOURCE_BARRIER barrier = {};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
    barrier.Transition.pResource = resource;
    barrier.Transition.StateBefore = before;
    barrier.Transition.StateAfter = after;
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    
    cmdList->ResourceBarrier(1, &barrier);
}

} // namespace Lunar
