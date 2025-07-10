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
    textureDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
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
	// barriers[1].Transition.pResource = m_postProcessPing.texture.Get();
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

	// reuse barriers 
	barriers[0].Transition.pResource = inputTexture.texture.Get();
	barriers[0].Transition.StateBefore = D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
	barriers[0].Transition.StateAfter = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;

	barriers[1].Transition.pResource = outputTexture.texture.Get();
	barriers[1].Transition.StateBefore = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
	barriers[1].Transition.StateAfter = D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
	commandList->ResourceBarrier(_countof(barriers), barriers);

}

} // namespace Lunar