#pragma once 

#include <d3d12.h>
#include <string>
#include <wrl/client.h>

namespace Lunar 
{
class DescriptorAllocator;
class PipelineStateManager;

struct ComputeTexture 
{
    Microsoft::WRL::ComPtr<ID3D12Resource> texture;
	std::string srvOffsetKey;
	std::string uavOffsetKey;
	UINT srvOffset;
	UINT uavOffset;
};

class PostProcessManager
{
    friend class PostProcessViewModel;
public:
    PostProcessManager();
    ~PostProcessManager() = default;

    void Initialize(ID3D12Device* device, DescriptorAllocator* descriptorAllocator);
    void ApplyPostEffects(ID3D12GraphicsCommandList* commandList, ID3D12Resource* sceneRenderTarget, PipelineStateManager* pipelineStateManager, DescriptorAllocator* descriptorAllocator);
    ComputeTexture& GetCurrentOutput() { return m_currentOutputIndex == 0 ? m_postProcessPing : m_postProcessPong; }

private:
    void SwapBuffers(ID3D12GraphicsCommandList* commandList, DescriptorAllocator* descriptorAllocator); 
	ComputeTexture m_postProcessPing;
	ComputeTexture m_postProcessPong;
    UINT m_width = 1280;
    UINT m_height = 720;
    UINT m_currentOutputIndex = 0;

    bool m_blurXEnabled = false;
    bool m_blurYEnabled = false;
};
} // namespace Lunar