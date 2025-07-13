#include "DescriptorAllocator.h"
#include "Utils/Logger.h"
#include "Utils/Utils.h"
#include <algorithm>

using namespace Microsoft::WRL;

namespace Lunar
{

void DescriptorAllocator::Initialize(ID3D12Device* device, UINT maxDescriptors)
{
    LOG_FUNCTION_ENTRY();
    
    m_device = device;
    
    D3D12_DESCRIPTOR_HEAP_DESC heapDesc = {};
    heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    heapDesc.NumDescriptors = maxDescriptors;
    heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
    heapDesc.NodeMask = 0;
    
    THROW_IF_FAILED(device->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&m_descriptorHeap)))
    
    m_descriptorSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    m_cpuStart = m_descriptorHeap->GetCPUDescriptorHandleForHeapStart();
    m_gpuStart = m_descriptorHeap->GetGPUDescriptorHandleForHeapStart();
    
    m_allocated.resize(maxDescriptors, false);
    
    LOG_DEBUG("DynamicDescriptorAllocator initialized with ", maxDescriptors, " descriptors");
}

UINT DescriptorAllocator::AllocateDescriptor(const std::string& name)
{
    if (m_nameToIndex.find(name) != m_nameToIndex.end())
    {
        LOG_ERROR("Descriptor already allocated: ", name);
        return m_nameToIndex[name];
    }
    
    m_allocated[m_currentIndex] = true;
    m_nameToIndex[name] = m_currentIndex;
    
    LOG_DEBUG("Allocated descriptor '", name, "' at index ", m_currentIndex);
    return m_currentIndex++;
}
	
D3D12_CPU_DESCRIPTOR_HANDLE DescriptorAllocator::GetCPUHandle(UINT index)
{
    D3D12_CPU_DESCRIPTOR_HANDLE handle = m_cpuStart;
    handle.ptr += static_cast<size_t>(index) * m_descriptorSize;
    return handle;
}

D3D12_GPU_DESCRIPTOR_HANDLE DescriptorAllocator::GetGPUHandle(UINT index)
{
    D3D12_GPU_DESCRIPTOR_HANDLE handle = m_gpuStart;
    handle.ptr += static_cast<size_t>(index) * m_descriptorSize;
    return handle;
}

D3D12_CPU_DESCRIPTOR_HANDLE DescriptorAllocator::GetCPUHandle(const std::string& name)
{
    auto it = m_nameToIndex.find(name);
    if (it != m_nameToIndex.end())
    {
        return GetCPUHandle(it->second);
    }
    
    LOG_ERROR("Descriptor not found: ", name);
    return {};
}

D3D12_GPU_DESCRIPTOR_HANDLE DescriptorAllocator::GetGPUHandle(const std::string& name)
{
    auto it = m_nameToIndex.find(name);
    if (it != m_nameToIndex.end())
    {
        return GetGPUHandle(it->second);
    }
    
    LOG_ERROR("Descriptor not found: ", name);
    return {};
}

void DescriptorAllocator::CreateSRV(ID3D12Resource* resource, const D3D12_SHADER_RESOURCE_VIEW_DESC* desc, const std::string& name)
{
    auto it = m_nameToIndex.find(name);
    if (it == m_nameToIndex.end())
    {
        LOG_ERROR("Descriptor not allocated: ", name);
        return;
    }
    
    m_device->CreateShaderResourceView(resource, desc, GetCPUHandle(it->second));
    LOG_DEBUG("SRV created for '", name, "' at index ", it->second);
}

void DescriptorAllocator::CreateUAV(ID3D12Resource* resource, const D3D12_UNORDERED_ACCESS_VIEW_DESC* desc, const std::string& name)
{
    auto it = m_nameToIndex.find(name);
    if (it == m_nameToIndex.end())
    {
        LOG_ERROR("Descriptor not allocated: ", name);
        return;
    }
    
    m_device->CreateUnorderedAccessView(resource, nullptr, desc, GetCPUHandle(it->second));
    LOG_DEBUG("UAV created for '", name, "' at index ", it->second);
}

void DescriptorAllocator::PrintAllocation()
{
    LOG_DEBUG("=== Descriptor Allocation Status ===");
    
    UINT totalAllocated = 0;
    for (bool allocated : m_allocated)
    {
        if (allocated) totalAllocated++;
    }
    
    LOG_DEBUG("Total allocated: ", totalAllocated, "/", m_allocated.size());
    
    LOG_DEBUG("Individual descriptors:");
    for (const auto& pair : m_nameToIndex)
    {
        LOG_DEBUG("  ", pair.first, " -> ", pair.second);
    }
    
    LOG_DEBUG("=====================================");
}

UINT DescriptorAllocator::GetDescriptorIndex(const std::string& name) const
{
    auto it = m_nameToIndex.find(name);
    return (it != m_nameToIndex.end()) ? it->second : UINT_MAX;
}
} // namespace Lunar
