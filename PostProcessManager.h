#pragma once 
#include <d3d12.h>
#include <wrl/client.h>

namespace Lunar 
{

struct ComputeTexture 
{
    Microsoft::WRL::ComPtr<ID3D12Resource> texture;
    D3D12_GPU_DESCRIPTOR_HANDLE srvHandle;
    D3D12_GPU_DESCRIPTOR_HANDLE uavHandle;    
};

class PostProcessManager
{
public:
    PostProcessManager() = default;
    ~PostProcessManager() = default;

    void Initialize(ID3D12Device* device, D3D12_GPU_DESCRIPTOR_HANDLE handle, UINT offset);
    void ApplyPostEffects(ID3D12GraphicsCommandList* commandList, ID3D12Resource* sceneRenderTarget, ID3D12RootSignature* rootSignature);
    ComputeTexture& GetCurrentOutput() { return m_currentOutputIndex == 0 ? m_postProcessPing : m_postProcessPong; }

private:
	ComputeTexture m_postProcessPing;
	ComputeTexture m_postProcessPong;
    UINT m_width = 1280;
    UINT m_height = 720;
    UINT m_currentOutputIndex = 0;
};
} // namespace Lunar