#include "PostProcessManager.h"
#include "Logger.h"
#include "Utils.h"
#include "imgui.h"
#include <d3dcompiler.h>

namespace Lunar
{

void PostProcessManager::BeginScene(ID3D12GraphicsCommandList* cmdList)
{
    ComputeTexture& sceneBuffer = GetCurrentInput();
    
    TransitionResource(cmdList, sceneBuffer.texture.Get(),
        D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
        D3D12_RESOURCE_STATE_RENDER_TARGET);
    
    D3D12_CPU_DESCRIPTOR_HANDLE sceneRTV = GetSceneRTV();
    D3D12_CPU_DESCRIPTOR_HANDLE sceneDSV = GetSceneDSV();
    
    cmdList->OMSetRenderTargets(1, &sceneRTV, FALSE, &sceneDSV);
    
    float clearColor[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
    cmdList->ClearRenderTargetView(sceneRTV, clearColor, 0, nullptr);
    cmdList->ClearDepthStencilView(sceneDSV, D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, nullptr);
}

void PostProcessManager::ApplyPostEffects(ID3D12GraphicsCommandList* cmdList)
{
    if (m_activeEffect == PostProcessEffect::None)
        return;
    
    UpdateConstantBuffer();
    
    cmdList->SetComputeRootSignature(m_computeRootSignature.Get());
    
    ID3D12DescriptorHeap* heaps[] = { m_srvUavHeap.Get() };
    cmdList->SetDescriptorHeaps(1, heaps);
    
    cmdList->SetComputeRootConstantBufferView(0, m_constantBuffer->GetGPUVirtualAddress());
    
    TransitionResource(cmdList, GetCurrentInput().texture.Get(),
        D3D12_RESOURCE_STATE_RENDER_TARGET,
        D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
    
    switch (m_activeEffect)
    {
    case PostProcessEffect::ToneMapping:
        ApplyToneMappingOptimized(cmdList);
        break;
    case PostProcessEffect::Bloom:
        ApplyBloomOptimized(cmdList);
        break;
    case PostProcessEffect::ToonShading:
        ApplyToonShadingOptimized(cmdList);
        break;
    case PostProcessEffect::EdgeDetection:
        ApplyEdgeDetectionOptimized(cmdList);
        break;
    }
}

void PostProcessManager::ApplyToneMappingOptimized(ID3D12GraphicsCommandList* cmdList)
{
    cmdList->SetPipelineState(m_computePSOs["ToneMapping"].Get());
    
    SetSRVTable(cmdList, 1, GetCurrentInput().srvHandle);
    
    SetUAVTable(cmdList, 2, m_ldrBuffer.uavHandle);
    
    UINT dispatchX = GetDispatchSize(m_width, 8);
    UINT dispatchY = GetDispatchSize(m_height, 8);
    cmdList->Dispatch(dispatchX, dispatchY, 1);
    
    UAVBarrier(cmdList, m_ldrBuffer.texture.Get());
}

void PostProcessManager::ApplyBloomOptimized(ID3D12GraphicsCommandList* cmdList)
{
    ApplyPass(cmdList, "BloomBrightPass");
    SwapBuffers();
    
    SetBlurDirection(1.0f, 0.0f);
    ApplyPass(cmdList, "GaussianBlur");
    SwapBuffers();
    
    SetBlurDirection(0.0f, 1.0f);
    ApplyPass(cmdList, "GaussianBlur");
    SwapBuffers();
    
    ApplyBloomComposite(cmdList);
}

void PostProcessManager::ApplyToonShadingOptimized(ID3D12GraphicsCommandList* cmdList)
{
    ApplyInPlace(cmdList, "ToonShading");
    
    ApplyToneMappingOptimized(cmdList);
}

void PostProcessManager::ApplyEdgeDetectionOptimized(ID3D12GraphicsCommandList* cmdList)
{
    ApplyPass(cmdList, "EdgeDetection");
    SwapBuffers();
    
    ApplyToneMappingOptimized(cmdList);
}

void PostProcessManager::ApplyPass(ID3D12GraphicsCommandList* cmdList, const std::string& shaderName)
{
    cmdList->SetPipelineState(m_computePSOs[shaderName].Get());
    
    SetSRVTable(cmdList, 1, GetCurrentInput().srvHandle);
    
    SetUAVTable(cmdList, 2, GetCurrentOutput().uavHandle);
    
    UINT dispatchX = GetDispatchSize(m_width, 8);
    UINT dispatchY = GetDispatchSize(m_height, 8);
    cmdList->Dispatch(dispatchX, dispatchY, 1);
    
    UAVBarrier(cmdList, GetCurrentOutput().texture.Get());
}

void PostProcessManager::ApplyInPlace(ID3D12GraphicsCommandList* cmdList, const std::string& shaderName)
{
    cmdList->SetPipelineState(m_computePSOs[shaderName].Get());
    
    SetSRVTable(cmdList, 1, GetCurrentInput().srvHandle);
    SetUAVTable(cmdList, 2, GetCurrentInput().uavHandle);
    
    UINT dispatchX = GetDispatchSize(m_width, 8);
    UINT dispatchY = GetDispatchSize(m_height, 8);
    cmdList->Dispatch(dispatchX, dispatchY, 1);
    
    UAVBarrier(cmdList, GetCurrentInput().texture.Get());
}

void PostProcessManager::ApplyBloomComposite(ID3D12GraphicsCommandList* cmdList)
{
    cmdList->SetPipelineState(m_computePSOs["BloomComposite"].Get());
    
    ApplyToneMappingOptimized(cmdList);
}

void PostProcessManager::SetBlurDirection(float x, float y)
{
    struct BlurConstants {
        float directionX, directionY;
        float padding[2];
    } blurConstants = { x, y, 0, 0 };
}

void PostProcessManager::SetSRVTable(ID3D12GraphicsCommandList* cmdList, UINT rootIndex, D3D12_CPU_DESCRIPTOR_HANDLE srvHandle)
{
    D3D12_GPU_DESCRIPTOR_HANDLE gpuHandle = m_srvUavHeap->GetGPUDescriptorHandleForHeapStart();
    gpuHandle.ptr += (srvHandle.ptr - m_srvUavHeap->GetCPUDescriptorHandleForHeapStart().ptr);
    cmdList->SetComputeRootDescriptorTable(rootIndex, gpuHandle);
}

void PostProcessManager::SetUAVTable(ID3D12GraphicsCommandList* cmdList, UINT rootIndex, D3D12_CPU_DESCRIPTOR_HANDLE uavHandle)
{
    D3D12_GPU_DESCRIPTOR_HANDLE gpuHandle = m_srvUavHeap->GetGPUDescriptorHandleForHeapStart();
    gpuHandle.ptr += (uavHandle.ptr - m_srvUavHeap->GetCPUDescriptorHandleForHeapStart().ptr);
    cmdList->SetComputeRootDescriptorTable(rootIndex, gpuHandle);
}

void PostProcessManager::UAVBarrier(ID3D12GraphicsCommandList* cmdList, ID3D12Resource* resource)
{
    D3D12_RESOURCE_BARRIER barrier = {};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
    barrier.UAV.pResource = resource;
    cmdList->ResourceBarrier(1, &barrier);
}

D3D12_CPU_DESCRIPTOR_HANDLE PostProcessManager::GetSceneRTV() const
{
    static bool rtvCreated = false;
    static D3D12_CPU_DESCRIPTOR_HANDLE sceneRTV = {};
    
    if (!rtvCreated)
    {
        sceneRTV = m_rtvHeap->GetCPUDescriptorHandleForHeapStart();
        sceneRTV.ptr += m_currentRtvIndex * m_rtvDescriptorSize;
        
        D3D12_RENDER_TARGET_VIEW_DESC rtvDesc = {};
        rtvDesc.Format = GetCurrentInput().format;
        rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
        rtvDesc.Texture2D.MipSlice = 0;
        
        m_device->CreateRenderTargetView(GetCurrentInput().texture.Get(), &rtvDesc, sceneRTV);
        rtvCreated = true;
    }
    
    return sceneRTV;
}

void PostProcessManager::EndScene(ID3D12GraphicsCommandList* cmdList)
{
    TransitionResource(cmdList, m_ldrBuffer.texture.Get(),
        D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
        D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
}

void PostProcessManager::PresentToBackBuffer(ID3D12GraphicsCommandList* cmdList, ID3D12Resource* backBuffer)
{
    TransitionResource(cmdList, backBuffer,
        D3D12_RESOURCE_STATE_PRESENT,
        D3D12_RESOURCE_STATE_COPY_DEST);
    
    cmdList->CopyResource(backBuffer, m_ldrBuffer.texture.Get());
    
    TransitionResource(cmdList, backBuffer,
        D3D12_RESOURCE_STATE_COPY_DEST,
        D3D12_RESOURCE_STATE_PRESENT);
}

void PostProcessManager::RenderGUI()
{
    if (ImGui::Begin("Post-Processing (Optimized)"))
    {
        ImGui::Text("Memory Usage: 40MB (2 HDR + 1 LDR)");
        ImGui::Text("Current Buffer: %d", m_currentBufferIndex);
        
        const char* effectNames[] = {
            "None", "Tone Mapping", "Bloom", "Toon Shading", 
            "Edge Detection", "Halftone", "Watercolor"
        };
        
        int currentEffect = static_cast<int>(m_activeEffect);
        if (ImGui::Combo("Effect", &currentEffect, effectNames, IM_ARRAYSIZE(effectNames)))
        {
            m_activeEffect = static_cast<PostProcessEffect>(currentEffect);
        }
        
        ImGui::Separator();
        
        switch (m_activeEffect)
        {
        case PostProcessEffect::ToneMapping:
            ImGui::SliderFloat("Exposure", &m_constants.exposure, 0.1f, 5.0f);
            ImGui::SliderFloat("Gamma", &m_constants.gamma, 1.0f, 3.0f);
            break;
            
        case PostProcessEffect::Bloom:
            ImGui::SliderFloat("Threshold", &m_constants.bloomThreshold, 0.0f, 2.0f);
            ImGui::SliderFloat("Intensity", &m_constants.bloomIntensity, 0.0f, 2.0f);
            ImGui::SliderFloat("Radius", &m_constants.bloomRadius, 0.5f, 3.0f);
            break;
            
        case PostProcessEffect::ToonShading:
            ImGui::SliderInt("Color Levels", (int*)&m_constants.colorLevels, 2, 16);
            break;
            
        case PostProcessEffect::EdgeDetection:
            ImGui::SliderFloat("Edge Threshold", &m_constants.edgeThreshold, 0.01f, 1.0f);
            break;
        }
        
        if (ImGui::Button("Swap Buffers (Debug)"))
        {
            SwapBuffers();
        }
    }
    ImGui::End();
}

} // namespace Lunar
