#include "IBLManager.h"
#include "Logger.h"
#include "Utils.h"
#include "imgui.h"
#include <d3dcompiler.h>

namespace Lunar
{

bool IBLManager::Initialize(ID3D12Device* device)
{
    LOG_FUNCTION_ENTRY();
    
    m_device = device;
    
    m_srvDescriptorSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    m_uavDescriptorSize = m_srvDescriptorSize;
    
    if (!CreateDescriptorHeaps()) return false;
    if (!CreateConstantBuffer()) return false;
    if (!CreateRootSignature()) return false;
    if (!CreatePipelineStates()) return false;
    
    if (!CreateBRDFLUT()) return false;
    
    LOG_FUNCTION_EXIT();
    return true;
}

void IBLManager::Shutdown()
{
    if (m_constantBufferData)
    {
        m_constantBuffer->Unmap(0, nullptr);
        m_constantBufferData = nullptr;
    }
}

bool IBLManager::CreateDescriptorHeaps()
{
    D3D12_DESCRIPTOR_HEAP_DESC srvHeapDesc = {};
    srvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    srvHeapDesc.NumDescriptors = 32;
    srvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
    
    HRESULT hr = m_device->CreateDescriptorHeap(&srvHeapDesc, IID_PPV_ARGS(&m_srvHeap));
    if (FAILED(hr))
    {
        LOG_ERROR("Failed to create SRV descriptor heap");
        return false;
    }
    
    D3D12_DESCRIPTOR_HEAP_DESC uavHeapDesc = {};
    uavHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    uavHeapDesc.NumDescriptors = 32;
    uavHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
    
    hr = m_device->CreateDescriptorHeap(&uavHeapDesc, IID_PPV_ARGS(&m_uavHeap));
    if (FAILED(hr))
    {
        LOG_ERROR("Failed to create UAV descriptor heap");
        return false;
    }
    
    return true;
}

bool IBLManager::CreateConstantBuffer()
{
    UINT constantBufferSize = (sizeof(IBLConstants) + 255) & ~255;
    
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
    
    D3D12_RANGE readRange = {};
    hr = m_constantBuffer->Map(0, &readRange, &m_constantBufferData);
    if (FAILED(hr))
    {
        LOG_ERROR("Failed to map constant buffer");
        return false;
    }
    
    return true;
}

bool IBLManager::LoadEnvironmentMap(const std::string& hdrFilePath)
{
    if (!HDRLoader::LoadHDR(hdrFilePath, m_hdrImage))
    {
        LOG_ERROR("Failed to load HDR image: %s", hdrFilePath.c_str());
        return false;
    }
    
    if (!CreateEnvironmentCubemap()) return false;
    if (!CreateIrradianceMap()) return false;
    if (!CreatePrefilteredMap()) return false;
    
    return true;
}

bool IBLManager::GenerateIBLTextures(ID3D12GraphicsCommandList* cmdList)
{
    UpdateConstantBuffer();
    
    cmdList->SetComputeRootSignature(m_computeRootSignature.Get());
    
    ID3D12DescriptorHeap* heaps[] = { m_srvHeap.Get(), m_uavHeap.Get() };
    cmdList->SetDescriptorHeaps(2, heaps);
    
    cmdList->SetComputeRootConstantBufferView(0, m_constantBuffer->GetGPUVirtualAddress());
    
    ConvertEquirectangularToCubemap(cmdList);
    GenerateIrradianceMap(cmdList);
    GeneratePrefilteredMap(cmdList);
    
    return true;
}

bool IBLManager::CreateEnvironmentCubemap()
{
    D3D12_RESOURCE_DESC textureDesc = {};
    textureDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    textureDesc.Width = ENV_CUBEMAP_SIZE;
    textureDesc.Height = ENV_CUBEMAP_SIZE;
    textureDesc.DepthOrArraySize = 6;
    textureDesc.MipLevels = 1;
    textureDesc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
    textureDesc.SampleDesc.Count = 1;
    textureDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
    textureDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
    
    D3D12_HEAP_PROPERTIES heapProps = {};
    heapProps.Type = D3D12_HEAP_TYPE_DEFAULT;
    
    HRESULT hr = m_device->CreateCommittedResource(
        &heapProps,
        D3D12_HEAP_FLAG_NONE,
        &textureDesc,
        D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
        nullptr,
        IID_PPV_ARGS(&m_iblTextures.environmentCubemap));
    
    if (FAILED(hr))
    {
        LOG_ERROR("Failed to create environment cubemap");
        return false;
    }
    
    m_iblTextures.envSRV = AllocateSRVDescriptor();
    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE;
    srvDesc.TextureCube.MipLevels = 1;
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    
    m_device->CreateShaderResourceView(m_iblTextures.environmentCubemap.Get(), &srvDesc, m_iblTextures.envSRV);
    
    m_iblTextures.envUAV = AllocateUAVDescriptor();
    D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
    uavDesc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
    uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2DARRAY;
    uavDesc.Texture2DArray.ArraySize = 6;
    
    m_device->CreateUnorderedAccessView(m_iblTextures.environmentCubemap.Get(), nullptr, &uavDesc, m_iblTextures.envUAV);
    
    return true;
}

bool IBLManager::CreateIrradianceMap()
{
    D3D12_RESOURCE_DESC textureDesc = {};
    textureDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    textureDesc.Width = IRRADIANCE_SIZE;
    textureDesc.Height = IRRADIANCE_SIZE;
    textureDesc.DepthOrArraySize = 6;
    textureDesc.MipLevels = 1;
    textureDesc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
    textureDesc.SampleDesc.Count = 1;
    textureDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
    textureDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
    
    D3D12_HEAP_PROPERTIES heapProps = {};
    heapProps.Type = D3D12_HEAP_TYPE_DEFAULT;
    
    HRESULT hr = m_device->CreateCommittedResource(
        &heapProps,
        D3D12_HEAP_FLAG_NONE,
        &textureDesc,
        D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
        nullptr,
        IID_PPV_ARGS(&m_iblTextures.irradianceMap));
    
    if (FAILED(hr))
    {
        LOG_ERROR("Failed to create irradiance map");
        return false;
    }
    
    m_iblTextures.irradianceSRV = AllocateSRVDescriptor();
    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE;
    srvDesc.TextureCube.MipLevels = 1;
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    
    m_device->CreateShaderResourceView(m_iblTextures.irradianceMap.Get(), &srvDesc, m_iblTextures.irradianceSRV);
    
    m_iblTextures.irradianceUAV = AllocateUAVDescriptor();
    D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
    uavDesc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
    uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2DARRAY;
    uavDesc.Texture2DArray.ArraySize = 6;
    
    m_device->CreateUnorderedAccessView(m_iblTextures.irradianceMap.Get(), nullptr, &uavDesc, m_iblTextures.irradianceUAV);
    
    return true;
}

D3D12_CPU_DESCRIPTOR_HANDLE IBLManager::AllocateSRVDescriptor()
{
    D3D12_CPU_DESCRIPTOR_HANDLE handle = m_srvHeap->GetCPUDescriptorHandleForHeapStart();
    handle.ptr += m_currentSrvIndex * m_srvDescriptorSize;
    m_currentSrvIndex++;
    return handle;
}

D3D12_CPU_DESCRIPTOR_HANDLE IBLManager::AllocateUAVDescriptor()
{
    D3D12_CPU_DESCRIPTOR_HANDLE handle = m_uavHeap->GetCPUDescriptorHandleForHeapStart();
    handle.ptr += m_currentUavIndex * m_uavDescriptorSize;
    m_currentUavIndex++;
    return handle;
}

void IBLManager::UpdateConstantBuffer()
{
    memcpy(m_constantBufferData, &m_constants, sizeof(IBLConstants));
}

void IBLManager::TransitionResource(
    ID3D12GraphicsCommandList* cmdList,
    ID3D12Resource* resource,
    D3D12_RESOURCE_STATES before,
    D3D12_RESOURCE_STATES after)
{
    if (before == after) return;
    
    D3D12_RESOURCE_BARRIER barrier = {};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Transition.pResource = resource;
    barrier.Transition.StateBefore = before;
    barrier.Transition.StateAfter = after;
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    
    cmdList->ResourceBarrier(1, &barrier);
}

void IBLManager::UAVBarrier(ID3D12GraphicsCommandList* cmdList, ID3D12Resource* resource)
{
    D3D12_RESOURCE_BARRIER barrier = {};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
    barrier.UAV.pResource = resource;
    cmdList->ResourceBarrier(1, &barrier);
}

void IBLManager::RenderGUI()
{
    if (ImGui::Begin("IBL Manager"))
    {
        ImGui::Text("Environment Map: %s", m_hdrImage.IsValid() ? "Loaded" : "None");
        
        if (m_hdrImage.IsValid())
        {
            ImGui::Text("Resolution: %dx%d", m_hdrImage.width, m_hdrImage.height);
        }
        
        ImGui::Separator();
        
        ImGui::SliderFloat("Environment Intensity", &m_constants.envIntensity, 0.0f, 5.0f);
        ImGui::SliderFloat("Max Reflection LOD", &m_constants.maxReflectionLod, 0.0f, 8.0f);
        ImGui::SliderFloat("Exposure", &m_constants.exposure, 0.1f, 5.0f);
        ImGui::SliderFloat("Gamma", &m_constants.gamma, 1.0f, 3.0f);
        
        if (ImGui::Button("Load HDR Environment"))
        {
            // TODO: File dialog integration
            LOG_INFO("HDR file dialog not implemented yet");
        }
    }
    ImGui::End();
}

} // namespace Lunar
