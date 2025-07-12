#pragma once 

#include <d3d12.h>
#include <wrl/client.h>

namespace Lunar 
{
    class PipelineStateManager;

struct ComputeTexture 
{
    Microsoft::WRL::ComPtr<ID3D12Resource> texture;
    D3D12_CPU_DESCRIPTOR_HANDLE cpuSrvHandle;
    D3D12_CPU_DESCRIPTOR_HANDLE cpuUavHandle;
    D3D12_GPU_DESCRIPTOR_HANDLE gpuSrvHandle;
    D3D12_GPU_DESCRIPTOR_HANDLE gpuUavHandle;    
};

class PostProcessManager
{
    friend class PostProcessViewModel;
public:
    PostProcessManager();
    ~PostProcessManager() = default;

    void Initialize(ID3D12Device* device, ID3D12DescriptorHeap* heap, UINT offset);
    void ApplyPostEffects(ID3D12GraphicsCommandList* commandList, ID3D12Resource* sceneRenderTarget, PipelineStateManager* pipelineStateManager);
    ComputeTexture& GetCurrentOutput() { return m_currentOutputIndex == 0 ? m_postProcessPing : m_postProcessPong; }

private:
    void SwapBuffers(ID3D12GraphicsCommandList* commandList); 
	ComputeTexture m_postProcessPing;
	ComputeTexture m_postProcessPong;
    UINT m_width = 1280;
    UINT m_height = 720;
    UINT m_currentOutputIndex = 0;

    bool m_blurXEnabled = true;
    bool m_blurYEnabled = true;
};
} // namespace Lunar