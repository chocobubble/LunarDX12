#include "PostProcessManager.h"

#include "DescriptorAllocator.h"
#include "LunarConstants.h"
#include "Utils/Logger.h"
#include "Utils/Utils.h"
#include "PipelineStateManager.h"

namespace Lunar
{

PostProcessManager::PostProcessManager()
{
	m_width = Utils::GetDisplayWidth();
	m_height = Utils::GetDisplayHeight();

	m_postProcessPing.srvOffsetKey = "PostProcess_Ping_SRV";
	m_postProcessPing.uavOffsetKey = "PostProcess_Ping_UAV";
	m_postProcessPong.srvOffsetKey = "PostProcess_Pong_SRV";
	m_postProcessPong.uavOffsetKey = "PostProcess_Pong_UAV";
}

void PostProcessManager::Initialize(ID3D12Device* device, DescriptorAllocator* descriptorAllocator)
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
    textureDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    textureDesc.SampleDesc.Count = 1;
    textureDesc.SampleDesc.Quality = 0;
    textureDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
    textureDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

    THROW_IF_FAILED(device->CreateCommittedResource(
        &heapProperties,
        D3D12_HEAP_FLAG_NONE,
        &textureDesc,
        D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE,
        nullptr,
        IID_PPV_ARGS(&m_postProcessPing.texture)));

    THROW_IF_FAILED(device->CreateCommittedResource(
        &heapProperties,
        D3D12_HEAP_FLAG_NONE,
        &textureDesc,
        D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE,
        nullptr,
        IID_PPV_ARGS(&m_postProcessPong.texture)));

	m_postProcessPing.srvOffset = descriptorAllocator->AllocateDescriptor(m_postProcessPing.srvOffsetKey);
    
    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Texture2D.MostDetailedMip = 0;
    srvDesc.Texture2D.MipLevels = 1;
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;

	descriptorAllocator->CreateSRV(m_postProcessPing.texture.Get(), &srvDesc, m_postProcessPing.srvOffsetKey);

    /*
    typedef struct D3D12_UNORDERED_ACCESS_VIEW_DESC {
        DXGI_FORMAT         Format;
        D3D12_UAV_DIMENSION ViewDimension;
        union {
            D3D12_BUFFER_UAV        Buffer;
            D3D12_TEX1D_UAV         Texture1D;
            D3D12_TEX1D_ARRAY_UAV   Texture1DArray;
            D3D12_TEX2D_UAV         Texture2D;
            D3D12_TEX2D_ARRAY_UAV   Texture2DArray;
            D3D12_TEX2DMS_UAV       Texture2DMS;
            D3D12_TEX2DMS_ARRAY_UAV Texture2DMSArray;
            D3D12_TEX3D_UAV         Texture3D;
        };
    } D3D12_UNORDERED_ACCESS_VIEW_DESC;
    */
    D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
    uavDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;

	m_postProcessPing.uavOffset = descriptorAllocator->AllocateDescriptor(m_postProcessPing.uavOffsetKey);
	descriptorAllocator->CreateUAV(m_postProcessPing.texture.Get(), &uavDesc, m_postProcessPing.uavOffsetKey);

	m_postProcessPong.srvOffset = descriptorAllocator->AllocateDescriptor(m_postProcessPong.srvOffsetKey);
	descriptorAllocator->CreateSRV(m_postProcessPong.texture.Get(), &srvDesc, m_postProcessPong.srvOffsetKey);
	m_postProcessPong.uavOffset = descriptorAllocator->AllocateDescriptor(m_postProcessPong.uavOffsetKey);
	descriptorAllocator->CreateUAV(m_postProcessPong.texture.Get(), &uavDesc, m_postProcessPong.uavOffsetKey);
}

void PostProcessManager::ApplyPostEffects(ID3D12GraphicsCommandList* commandList, ID3D12Resource* sceneRenderTarget, PipelineStateManager* pipelineStateManager, DescriptorAllocator* descriptorAllocator)
{
    // First, copy the scene render target to output texture
	ComputeTexture& inputTexture = m_currentOutputIndex == 0 ? m_postProcessPong : m_postProcessPing;
	ComputeTexture& outputTexture = m_currentOutputIndex == 0 ? m_postProcessPing : m_postProcessPong;
	
	D3D12_RESOURCE_BARRIER barriers[2] = {};
	barriers[0].Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION; 
	barriers[0].Transition.pResource = sceneRenderTarget;
	barriers[0].Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
	barriers[0].Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_SOURCE;
	barriers[0].Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

	barriers[1].Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION; 
	barriers[1].Transition.pResource = outputTexture.texture.Get();
	barriers[1].Transition.StateBefore = D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
	barriers[1].Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_DEST;
	barriers[1].Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

	commandList->ResourceBarrier(_countof(barriers), barriers);
	commandList->CopyResource(outputTexture.texture.Get(), sceneRenderTarget);

	barriers[0].Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_SOURCE;
	barriers[0].Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
	
	barriers[1].Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
	barriers[1].Transition.StateAfter = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
	commandList->ResourceBarrier(2, barriers);

    if (m_blurXEnabled)
    {
        SwapBuffers(commandList, descriptorAllocator);
        commandList->SetComputeRootSignature(pipelineStateManager->GetRootSignature());
        commandList->SetPipelineState(pipelineStateManager->GetPSO("gaussianBlurX"));
        commandList->Dispatch((m_width + 63) / 64, m_height, 1);
    }
    if (m_blurYEnabled)
    {
        SwapBuffers(commandList, descriptorAllocator);
        commandList->SetComputeRootSignature(pipelineStateManager->GetRootSignature());
        commandList->SetPipelineState(pipelineStateManager->GetPSO("gaussianBlurY"));
        commandList->Dispatch(m_width, (m_height + 63) / 64, 1);
    }
}

void PostProcessManager::SwapBuffers(ID3D12GraphicsCommandList* commandList, DescriptorAllocator* descriptorAllocator)
{
	m_currentOutputIndex = 1 - m_currentOutputIndex; // toggle between ping and pong
	ComputeTexture& inputTexture = m_currentOutputIndex == 0 ? m_postProcessPong : m_postProcessPing;
	ComputeTexture& outputTexture = m_currentOutputIndex == 0 ? m_postProcessPing : m_postProcessPong;

	D3D12_RESOURCE_BARRIER barriers[2] = {};
	barriers[0].Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION; 
	barriers[0].Transition.pResource = inputTexture.texture.Get();
	barriers[0].Transition.StateBefore = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
	barriers[0].Transition.StateAfter = D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
	barriers[0].Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

	barriers[1].Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION; 
	barriers[1].Transition.pResource = outputTexture.texture.Get();
	barriers[1].Transition.StateBefore = D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
	barriers[1].Transition.StateAfter = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
	barriers[1].Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

	commandList->ResourceBarrier(_countof(barriers), barriers);

	commandList->SetComputeRootDescriptorTable(LunarConstants::POST_PROCESS_INPUT_ROOT_PARAMETER_INDEX, descriptorAllocator->GetGPUHandle(m_postProcessPing.uavOffsetKey));
	commandList->SetComputeRootDescriptorTable(LunarConstants::POST_PROCESS_OUTPUT_ROOT_PARAMETER_INDEX, descriptorAllocator->GetGPUHandle(m_postProcessPong.uavOffsetKey));
}

} // namespace Lunar