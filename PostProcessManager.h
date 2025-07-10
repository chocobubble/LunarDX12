#pragma once

#include <d3d12.h>
#include <dxgi1_6.h>
#include <wrl/client.h>
#include <vector>
#include <memory>
#include <unordered_map>
#include <string>
#include "ConstantBuffers.h"

using Microsoft::WRL::ComPtr;

namespace Lunar
{

struct ComputeTexture
{
    ComPtr<ID3D12Resource> texture;
    D3D12_CPU_DESCRIPTOR_HANDLE srvHandle = {};
    D3D12_CPU_DESCRIPTOR_HANDLE uavHandle = {};
    DXGI_FORMAT format = DXGI_FORMAT_UNKNOWN;
    UINT width = 0;
    UINT height = 0;
};

enum class PostProcessEffect
{
    None,
    ToneMapping,
    Bloom,
    ToonShading,
    EdgeDetection,
    Halftone,
    Watercolor,
    Count
};

class PostProcessManager
{
public:
    PostProcessManager() = default;
    ~PostProcessManager() = default;

    bool Initialize(
        ID3D12Device* device, 
        UINT width, 
        UINT height,
        ID3D12DescriptorHeap* cbvSrvUavHeap,  // 외부 통합 Heap 사용
        ID3D12DescriptorHeap* rtvHeap,        // 외부 RTV Heap 사용
        UINT& srvUavStartIndex,               // 시작 인덱스 (참조로 전달)
        UINT& rtvStartIndex                   // 시작 인덱스 (참조로 전달)
    );
    void Shutdown();
    
    void BeginScene(ID3D12GraphicsCommandList* cmdList);
    void ApplyPostEffects(ID3D12GraphicsCommandList* cmdList);
    void EndScene(ID3D12GraphicsCommandList* cmdList);
    
    ID3D12Resource* GetSceneRenderTarget() const { return m_sceneTexture.texture.Get(); }
    D3D12_CPU_DESCRIPTOR_HANDLE GetSceneRTV() const;
    D3D12_CPU_DESCRIPTOR_HANDLE GetSceneDSV() const;
    
    ComputeTexture& GetCurrentInput() { return m_hdrBuffer[m_currentBufferIndex]; }
    ComputeTexture& GetCurrentOutput() { return m_hdrBuffer[1 - m_currentBufferIndex]; }
    ComputeTexture& GetFinalOutput() { return m_ldrBuffer; }
    void SwapBuffers() { m_currentBufferIndex = 1 - m_currentBufferIndex; }
    
    void SetActiveEffect(PostProcessEffect effect) { m_activeEffect = effect; }
    PostProcessEffect GetActiveEffect() const { return m_activeEffect; }
    
    PostProcessConstants& GetConstants() { return m_constants; }
    const PostProcessConstants& GetConstants() const { return m_constants; }
    
    void RenderGUI();

private:
    ID3D12Device* m_device = nullptr;
    UINT m_width = 0;
    UINT m_height = 0;
    
    // 외부 Heap 참조 (소유하지 않음)
    ID3D12DescriptorHeap* m_externalSrvUavHeap = nullptr;
    ID3D12DescriptorHeap* m_externalRtvHeap = nullptr;
    
    // 할당된 인덱스 범위
    UINT m_srvUavStartIndex = 0;
    UINT m_rtvStartIndex = 0;
    UINT m_allocatedSrvUavCount = 0;
    UINT m_allocatedRtvCount = 0;
    
    UINT m_srvUavDescriptorSize = 0;
    UINT m_rtvDescriptorSize = 0;
    
    ComputeTexture m_hdrBuffer[2];
    ComputeTexture m_ldrBuffer;
    ComputeTexture m_sceneTexture;  // Scene Render Target 추가
    int m_currentBufferIndex = 0;
    
    ComPtr<ID3D12Resource> m_depthBuffer;
    D3D12_CPU_DESCRIPTOR_HANDLE m_depthDSV = {};
    D3D12_CPU_DESCRIPTOR_HANDLE m_depthSRV = {};
    
    ComPtr<ID3D12RootSignature> m_computeRootSignature;
    std::unordered_map<std::string, ComPtr<ID3D12PipelineState>> m_computePSOs;
    
    ComPtr<ID3D12Resource> m_constantBuffer;
    PostProcessConstants m_constants;
    void* m_constantBufferData = nullptr;
    
    PostProcessEffect m_activeEffect = PostProcessEffect::None;
    
    bool CreateTextures();
    bool CreateDepthBuffer();
    bool CreateConstantBuffer();
    bool CreateRootSignature();
    bool CreatePipelineStates();
    
    bool CreateComputeTexture(DXGI_FORMAT format, ComputeTexture& texture);
    D3D12_CPU_DESCRIPTOR_HANDLE AllocateSrvUavDescriptor();
    D3D12_CPU_DESCRIPTOR_HANDLE AllocateRtvDescriptor();
    
    void TransitionResource(
        ID3D12GraphicsCommandList* cmdList,
        ID3D12Resource* resource,
        D3D12_RESOURCE_STATES before,
        D3D12_RESOURCE_STATES after);
    
    void UpdateConstantBuffer();
    UINT GetDispatchSize(UINT textureSize, UINT threadGroupSize) const;
    
    void ApplyPass(ID3D12GraphicsCommandList* cmdList, const std::string& shaderName);
    void ApplyInPlace(ID3D12GraphicsCommandList* cmdList, const std::string& shaderName);
    void SetBlurDirection(float x, float y);
    void UAVBarrier(ID3D12GraphicsCommandList* cmdList, ID3D12Resource* resource);
    void CopySceneToHDR(ID3D12GraphicsCommandList* cmdList);
    void CopyToBackBuffer(ID3D12GraphicsCommandList* cmdList);
};

} // namespace Lunar
