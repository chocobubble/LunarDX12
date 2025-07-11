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
    // Scene Render Target 생성 (HDR 포맷)
    if (!CreateComputeTexture(DXGI_FORMAT_R16G16B16A16_FLOAT, m_sceneTexture))
    {
        LOG_ERROR("Failed to create Scene render target");
        return false;
    }
    
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
    
    LOG_INFO("Created texture set: Scene RT + 2 HDR + 1 LDR = 56MB total");
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
    // Scene texture의 RTV는 CreateComputeTexture에서 이미 생성되지 않으므로 여기서 생성
    static bool rtvCreated = false;
    static D3D12_CPU_DESCRIPTOR_HANDLE sceneRTV = {};
    
    if (!rtvCreated)
    {
        // RTV 할당
        sceneRTV = const_cast<PostProcessManager*>(this)->AllocateRtvDescriptor();
        
        // RTV 생성
        D3D12_RENDER_TARGET_VIEW_DESC rtvDesc = {};
        rtvDesc.Format = m_sceneTexture.format;
        rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
        rtvDesc.Texture2D.MipSlice = 0;
        
        m_device->CreateRenderTargetView(m_sceneTexture.texture.Get(), &rtvDesc, sceneRTV);
        rtvCreated = true;
        
        LOG_INFO("Created Scene RTV");
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

void PostProcessManager::BeginScene(ID3D12GraphicsCommandList* cmdList)
{
    LOG_FUNCTION_ENTRY();
    
    // Scene texture를 Render Target으로 전환
    TransitionResource(cmdList, m_sceneTexture.texture.Get(), 
                      D3D12_RESOURCE_STATE_UNORDERED_ACCESS, 
                      D3D12_RESOURCE_STATE_RENDER_TARGET);
    
    // Depth buffer를 Depth Write로 전환
    TransitionResource(cmdList, m_depthBuffer.Get(),
                      D3D12_RESOURCE_STATE_DEPTH_READ,
                      D3D12_RESOURCE_STATE_DEPTH_WRITE);
    
    // Render Target 설정
    D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = GetSceneRTV();
    D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle = GetSceneDSV();
    
    cmdList->OMSetRenderTargets(1, &rtvHandle, FALSE, &dsvHandle);
    
    // Clear render target and depth buffer
    float clearColor[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
    cmdList->ClearRenderTargetView(rtvHandle, clearColor, 0, nullptr);
    cmdList->ClearDepthStencilView(dsvHandle, D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, nullptr);
    
    LOG_INFO("Scene rendering started");
}

void PostProcessManager::EndScene(ID3D12GraphicsCommandList* cmdList)
{
    LOG_FUNCTION_ENTRY();
    
    // Scene texture를 SRV로 전환 (Post-processing에서 읽기 위해)
    TransitionResource(cmdList, m_sceneTexture.texture.Get(), 
                      D3D12_RESOURCE_STATE_RENDER_TARGET, 
                      D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
    
    // Depth buffer를 SRV로 전환 (필요시 Post-processing에서 읽기 위해)
    TransitionResource(cmdList, m_depthBuffer.Get(),
                      D3D12_RESOURCE_STATE_DEPTH_WRITE,
                      D3D12_RESOURCE_STATE_DEPTH_READ);
    
    LOG_INFO("Scene rendering ended, ready for post-processing");
}

void PostProcessManager::ApplyPostEffects(ID3D12GraphicsCommandList* cmdList)
{
    LOG_FUNCTION_ENTRY();
    
    if (m_activeEffect == PostProcessEffect::None) {
        // No post-processing, just copy scene to back buffer
        // TODO: Implement direct copy
        LOG_INFO("No post-processing effects active");
        return;
    }
    
    // Update constant buffer
    UpdateConstantBuffer();
    
    // Set descriptor heap
    ID3D12DescriptorHeap* heaps[] = { m_srvUavHeap.Get() };
    cmdList->SetDescriptorHeaps(1, heaps);
    
    // Copy scene texture to HDR buffer 1 first
    CopySceneToHDR(cmdList);
    
    // Apply selected effect
    switch (m_activeEffect) {
        case PostProcessEffect::ToneMapping:
            ApplyPass(cmdList, "ToneMapping");
            break;
        case PostProcessEffect::Bloom:
            ApplyPass(cmdList, "BloomBrightPass");
            ApplyPass(cmdList, "GaussianBlur");
            break;
        // Add other effects...
        default:
            break;
    }
    
    // Final copy to back buffer
    CopyToBackBuffer(cmdList);
    
    LOG_INFO("Post-processing effects applied");
}

void PostProcessManager::CopySceneToHDR(ID3D12GraphicsCommandList* cmdList)
{
    // Scene texture (SRV) -> HDR Buffer 1 (UAV)
    TransitionResource(cmdList, m_hdrBuffer[0].texture.Get(),
                      D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
                      D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
    
    // Set compute root signature and PSO
    cmdList->SetComputeRootSignature(m_computeRootSignature.Get());
    // TODO: Create copy compute shader PSO
    
    // Bind scene texture as SRV (t0)
    cmdList->SetComputeRootDescriptorTable(1, /* Scene SRV GPU handle */);
    
    // Bind HDR buffer as UAV (u0)  
    cmdList->SetComputeRootDescriptorTable(2, /* HDR UAV GPU handle */);
    
    // Dispatch
    UINT dispatchX = GetDispatchSize(m_width, 8);
    UINT dispatchY = GetDispatchSize(m_height, 8);
    cmdList->Dispatch(dispatchX, dispatchY, 1);
    
    // UAV barrier
    UAVBarrier(cmdList, m_hdrBuffer[0].texture.Get());
}

void PostProcessManager::CopyToBackBuffer(ID3D12GraphicsCommandList* cmdList)
{
    // TODO: Implement copy from LDR buffer to back buffer
    // This would typically be done with a fullscreen quad or compute shader
}

void PostProcessManager::UAVBarrier(ID3D12GraphicsCommandList* cmdList, ID3D12Resource* resource)
{
    D3D12_RESOURCE_BARRIER barrier = {};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
    barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
    barrier.UAV.pResource = resource;
    
    cmdList->ResourceBarrier(1, &barrier);
}

} // namespace Lunar
#include "PostProcessManager.h"

#include "LunarConstants.h"
#include "Utils/Logger.h"
#include "Utils/Utils.h"

namespace Lunar
{

PostProcessManager::PostProcessManager()
{
	m_width = Utils::GetDisplayWidth();
	m_height = Utils::GetDisplayHeight();
}

void PostProcessManager::Initialize(ID3D12Device* device, D3D12_GPU_DESCRIPTOR_HANDLE handle, UINT offset)
{
	LOG_FUNCTION_ENTRY();
	
    D3D12_HEAP_PROPERTIES heapProperties = {};
    heapProperties.Type = D3D12_HEAP_TYPE_DEFAULT;
    heapProperties.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
    heapProperties.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
    heapProperties.CreationNodeMask = 1;
    heapProperties.VisibleNodeMask = 1;

    D3D12_RESOURCE_DESC textureDesc = {};
    textureDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    textureDesc.Width = m_width;
    textureDesc.Height = m_height;
    textureDesc.DepthOrArraySize = 1;
    textureDesc.MipLevels = 1;
    textureDesc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
    textureDesc.SampleDesc.Count = 1;
    textureDesc.SampleDesc.Quality = 0;
    textureDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
    textureDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

    THROW_IF_FAILED(device->CreateCommittedResource(
        &heapProperties,
        D3D12_HEAP_FLAG_NONE,
        &textureDesc,
        D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
        nullptr,
        IID_PPV_ARGS(&m_postProcessPing.texture)));

    THROW_IF_FAILED(device->CreateCommittedResource(
        &heapProperties,
        D3D12_HEAP_FLAG_NONE,
        &textureDesc,
        D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
        nullptr,
        IID_PPV_ARGS(&m_postProcessPong.texture)));

    m_postProcessPing.srvHandle = handle;
    m_postProcessPing.srvHandle.ptr += offset * device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    ++offset;
    m_postProcessPing.uavHandle = handle;
    m_postProcessPing.uavHandle.ptr += offset * device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    ++offset;
    m_postProcessPong.srvHandle = handle;
    m_postProcessPong.srvHandle.ptr += offset * device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    ++offset;
    m_postProcessPong.uavHandle = handle;
    m_postProcessPong.uavHandle.ptr += offset * device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    ++offset;

}

void PostProcessManager::ApplyPostEffects(ID3D12GraphicsCommandList* commandList, ID3D12Resource* sceneRenderTarget, ID3D12RootSignature* rootSignature)
{
    /*
    PING-PONG BUFFER VISUALIZATION:

    Frame N:     [Scene RT] → [Ping] → [Pong] → [Present]
    Frame N+1:   [Scene RT] → [Pong] → [Ping] → [Present]

    Current State: m_currentOutputIndex toggles between 0 and 1
    - When 0: Input=Pong, Output=Ping
    - When 1: Input=Ping, Output=Pong
    */

	m_currentOutputIndex = 1 - m_currentOutputIndex; // toggle between ping and pong
	
	ComputeTexture& inputTexture = m_currentOutputIndex == 0 ? m_postProcessPong : m_postProcessPing;
	ComputeTexture& outputTexture = m_currentOutputIndex == 0 ? m_postProcessPing : m_postProcessPong;
	
	// copy the scene render target to the ping texture
	D3D12_RESOURCE_BARRIER barriers[2] = {};
	barriers[0].Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION; 
	barriers[0].Transition.pResource = sceneRenderTarget;
	barriers[0].Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
	barriers[0].Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_SOURCE;
	barriers[0].Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

	barriers[1].Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION; 
	barriers[1].Transition.pResource = inputTexture.texture.Get();
	barriers[1].Transition.StateBefore = D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
	barriers[1].Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_DEST;
	barriers[1].Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

	commandList->ResourceBarrier(_countof(barriers), barriers);
	commandList->CopyResource(inputTexture.texture.Get(), sceneRenderTarget);

	barriers[0].Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_SOURCE;
	barriers[0].Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
	
	barriers[1].Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
	barriers[1].Transition.StateAfter = D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
	commandList->ResourceBarrier(2, barriers);

	commandList->SetComputeRootSignature(rootSignature);

	commandList->SetComputeRootDescriptorTable(LunarConstants::POST_PROCESS_INPUT_ROOT_PARAMETER_INDEX, inputTexture.srvHandle);
	commandList->SetComputeRootDescriptorTable(LunarConstants::POST_PROCESS_OUTPUT_ROOT_PARAMETER_INDEX, outputTexture.uavHandle);

	// commandList->Dispatch((m_width + 15) / 16, (m_height + 15) / 16, 1);

	barriers[0].Transition.pResource = inputTexture.texture.Get();
	barriers[0].Transition.StateBefore = D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
	barriers[0].Transition.StateAfter = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;

	barriers[1].Transition.pResource = outputTexture.texture.Get();
	barriers[1].Transition.StateBefore = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
	barriers[1].Transition.StateAfter = D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
	commandList->ResourceBarrier(_countof(barriers), barriers);
}

} // namespace Lunar