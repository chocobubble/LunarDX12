// PostProcessManager_Implementation.cpp - 나머지 구현 부분

#include "PostProcessManager.h"
#include "Logger.h"
#include "Utils.h"
#include "imgui.h"
#include <d3dcompiler.h>

namespace Lunar
{

bool PostProcessManager::CreateRootSignature()
{
    // Root signature for compute shaders
    D3D12_ROOT_PARAMETER rootParams[3] = {};
    
    // Parameter 0: Constant buffer
    rootParams[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    rootParams[0].Descriptor.ShaderRegister = 0;
    rootParams[0].Descriptor.RegisterSpace = 0;
    rootParams[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
    
    // Parameter 1: SRV table (input textures)
    D3D12_DESCRIPTOR_RANGE srvRange = {};
    srvRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    srvRange.NumDescriptors = 8;
    srvRange.BaseShaderRegister = 0;
    srvRange.RegisterSpace = 0;
    srvRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;
    
    rootParams[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    rootParams[1].DescriptorTable.NumDescriptorRanges = 1;
    rootParams[1].DescriptorTable.pDescriptorRanges = &srvRange;
    rootParams[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
    
    // Parameter 2: UAV table (output textures)
    D3D12_DESCRIPTOR_RANGE uavRange = {};
    uavRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
    uavRange.NumDescriptors = 8;
    uavRange.BaseShaderRegister = 0;
    uavRange.RegisterSpace = 0;
    uavRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;
    
    rootParams[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    rootParams[2].DescriptorTable.NumDescriptorRanges = 1;
    rootParams[2].DescriptorTable.pDescriptorRanges = &uavRange;
    rootParams[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
    
    // Static sampler
    D3D12_STATIC_SAMPLER_DESC sampler = {};
    sampler.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
    sampler.AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    sampler.AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    sampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    sampler.MipLODBias = 0;
    sampler.MaxAnisotropy = 0;
    sampler.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
    sampler.BorderColor = D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK;
    sampler.MinLOD = 0.0f;
    sampler.MaxLOD = D3D12_FLOAT32_MAX;
    sampler.ShaderRegister = 0;
    sampler.RegisterSpace = 0;
    sampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
    
    D3D12_ROOT_SIGNATURE_DESC rootSigDesc = {};
    rootSigDesc.NumParameters = 3;
    rootSigDesc.pParameters = rootParams;
    rootSigDesc.NumStaticSamplers = 1;
    rootSigDesc.pStaticSamplers = &sampler;
    rootSigDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_NONE;
    
    ComPtr<ID3DBlob> signature;
    ComPtr<ID3DBlob> error;
    HRESULT hr = D3D12SerializeRootSignature(&rootSigDesc, D3D_ROOT_SIGNATURE_VERSION_1, &signature, &error);
    if (FAILED(hr))
    {
        if (error)
        {
            LOG_ERROR("Root signature serialization failed: %s", (char*)error->GetBufferPointer());
        }
        return false;
    }
    
    hr = m_device->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(&m_computeRootSignature));
    if (FAILED(hr))
    {
        LOG_ERROR("Failed to create compute root signature");
        return false;
    }
    
    return true;
}

bool PostProcessManager::CreatePipelineStates()
{
    // Helper lambda to create compute PSO
    auto CreateComputePSO = [this](const std::wstring& shaderPath, const std::string& entryPoint) -> ComPtr<ID3D12PipelineState>
    {
        ComPtr<ID3DBlob> computeShader;
        ComPtr<ID3DBlob> error;
        
        HRESULT hr = D3DCompileFromFile(
            shaderPath.c_str(),
            nullptr,
            D3D_COMPILE_STANDARD_FILE_INCLUDE,
            entryPoint.c_str(),
            "cs_5_1",
            D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION,
            0,
            &computeShader,
            &error);
        
        if (FAILED(hr))
        {
            if (error)
            {
                LOG_ERROR("Shader compilation failed: %s", (char*)error->GetBufferPointer());
            }
            return nullptr;
        }
        
        D3D12_COMPUTE_PIPELINE_STATE_DESC psoDesc = {};
        psoDesc.pRootSignature = m_computeRootSignature.Get();
        psoDesc.CS = { computeShader->GetBufferPointer(), computeShader->GetBufferSize() };
        
        ComPtr<ID3D12PipelineState> pso;
        hr = m_device->CreateComputePipelineState(&psoDesc, IID_PPV_ARGS(&pso));
        if (FAILED(hr))
        {
            LOG_ERROR("Failed to create compute pipeline state");
            return nullptr;
        }
        
        return pso;
    };
    
    // Create all compute PSOs
    m_computePSOs["ToneMapping"] = CreateComputePSO(L"Shaders/PostProcess/ToneMapping.hlsl", "CSMain");
    m_computePSOs["BloomBrightPass"] = CreateComputePSO(L"Shaders/PostProcess/BloomBrightPass.hlsl", "CSMain");
    m_computePSOs["GaussianBlur"] = CreateComputePSO(L"Shaders/PostProcess/GaussianBlur.hlsl", "CSMain");
    m_computePSOs["ToonShading"] = CreateComputePSO(L"Shaders/PostProcess/ToonShading.hlsl", "CSMain");
    m_computePSOs["EdgeDetection"] = CreateComputePSO(L"Shaders/PostProcess/EdgeDetection.hlsl", "CSMain");
    
    // Check if all PSOs were created successfully
    for (const auto& [name, pso] : m_computePSOs)
    {
        if (!pso)
        {
            LOG_ERROR("Failed to create PSO: %s", name.c_str());
            return false;
        }
    }
    
    return true;
}

void PostProcessManager::BeginScene(ID3D12GraphicsCommandList* cmdList)
{
    // Transition scene texture to render target state
    TransitionResource(cmdList, m_sceneTexture.texture.Get(),
        D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
        D3D12_RESOURCE_STATE_RENDER_TARGET);
    
    // Set render target
    D3D12_CPU_DESCRIPTOR_HANDLE sceneRTV = GetSceneRTV();
    D3D12_CPU_DESCRIPTOR_HANDLE sceneDSV = GetSceneDSV();
    
    cmdList->OMSetRenderTargets(1, &sceneRTV, FALSE, &sceneDSV);
    
    // Clear render target
    float clearColor[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
    cmdList->ClearRenderTargetView(sceneRTV, clearColor, 0, nullptr);
    cmdList->ClearDepthStencilView(sceneDSV, D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, nullptr);
}

void PostProcessManager::ApplyPostEffects(ID3D12GraphicsCommandList* cmdList)
{
    if (m_activeEffect == PostProcessEffect::None)
        return;
    
    // Update constant buffer
    UpdateConstantBuffer();
    
    // Set compute root signature
    cmdList->SetComputeRootSignature(m_computeRootSignature.Get());
    
    // Set descriptor heap
    ID3D12DescriptorHeap* heaps[] = { m_srvUavHeap.Get() };
    cmdList->SetDescriptorHeaps(1, heaps);
    
    // Set constant buffer
    cmdList->SetComputeRootConstantBufferView(0, m_constantBuffer->GetGPUVirtualAddress());
    
    // Apply selected effect
    switch (m_activeEffect)
    {
    case PostProcessEffect::ToneMapping:
        ApplyToneMapping(cmdList);
        break;
    case PostProcessEffect::Bloom:
        ApplyBloom(cmdList);
        break;
    case PostProcessEffect::ToonShading:
        ApplyToonShading(cmdList);
        break;
    case PostProcessEffect::EdgeDetection:
        ApplyEdgeDetection(cmdList);
        break;
    case PostProcessEffect::Halftone:
        ApplyHalftone(cmdList);
        break;
    case PostProcessEffect::Watercolor:
        ApplyWatercolor(cmdList);
        break;
    }
}

void PostProcessManager::ApplyToneMapping(ID3D12GraphicsCommandList* cmdList)
{
    // Transition scene texture to SRV
    TransitionResource(cmdList, m_sceneTexture.texture.Get(),
        D3D12_RESOURCE_STATE_RENDER_TARGET,
        D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
    
    // Transition final texture to UAV
    TransitionResource(cmdList, m_finalTexture.texture.Get(),
        D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
        D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
    
    // Set pipeline state
    cmdList->SetPipelineState(m_computePSOs["ToneMapping"].Get());
    
    // Set SRV table (input)
    D3D12_GPU_DESCRIPTOR_HANDLE srvTable = m_srvUavHeap->GetGPUDescriptorHandleForHeapStart();
    srvTable.ptr += (m_sceneTexture.srvHandle.ptr - m_srvUavHeap->GetCPUDescriptorHandleForHeapStart().ptr);
    cmdList->SetComputeRootDescriptorTable(1, srvTable);
    
    // Set UAV table (output)
    D3D12_GPU_DESCRIPTOR_HANDLE uavTable = m_srvUavHeap->GetGPUDescriptorHandleForHeapStart();
    uavTable.ptr += (m_finalTexture.uavHandle.ptr - m_srvUavHeap->GetCPUDescriptorHandleForHeapStart().ptr);
    cmdList->SetComputeRootDescriptorTable(2, uavTable);
    
    // Dispatch
    UINT dispatchX = GetDispatchSize(m_width, 8);
    UINT dispatchY = GetDispatchSize(m_height, 8);
    cmdList->Dispatch(dispatchX, dispatchY, 1);
    
    // UAV barrier
    D3D12_RESOURCE_BARRIER barrier = {};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
    barrier.UAV.pResource = m_finalTexture.texture.Get();
    cmdList->ResourceBarrier(1, &barrier);
}

void PostProcessManager::ApplyToonShading(ID3D12GraphicsCommandList* cmdList)
{
    // Similar to tone mapping but with toon shading shader
    TransitionResource(cmdList, m_sceneTexture.texture.Get(),
        D3D12_RESOURCE_STATE_RENDER_TARGET,
        D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
    
    cmdList->SetPipelineState(m_computePSOs["ToonShading"].Get());
    
    // Set descriptors and dispatch
    // ... (similar pattern to ApplyToneMapping)
}

void PostProcessManager::ApplyEdgeDetection(ID3D12GraphicsCommandList* cmdList)
{
    // Edge detection with depth buffer input
    TransitionResource(cmdList, m_sceneTexture.texture.Get(),
        D3D12_RESOURCE_STATE_RENDER_TARGET,
        D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
    
    TransitionResource(cmdList, m_depthBuffer.Get(),
        D3D12_RESOURCE_STATE_DEPTH_WRITE,
        D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
    
    cmdList->SetPipelineState(m_computePSOs["EdgeDetection"].Get());
    
    // Set both color and depth textures as input
    // ... (implementation details)
}

void PostProcessManager::EndScene(ID3D12GraphicsCommandList* cmdList)
{
    // Transition final texture back to SRV for presentation
    if (m_activeEffect != PostProcessEffect::None)
    {
        TransitionResource(cmdList, m_finalTexture.texture.Get(),
            D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
            D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
    }
    else
    {
        TransitionResource(cmdList, m_sceneTexture.texture.Get(),
            D3D12_RESOURCE_STATE_RENDER_TARGET,
            D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
    }
}

void PostProcessManager::RenderGUI()
{
    if (ImGui::Begin("Post-Processing"))
    {
        // Effect selection
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
        
        // Effect-specific parameters
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
    }
    ImGui::End();
}

} // namespace Lunar
