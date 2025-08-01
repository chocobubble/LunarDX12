#include "DescriptorAllocator.h"
#include "LunarConstants.h"
#include "Utils/Logger.h"
#include "Utils/Utils.h"
#include <algorithm>
#include <iomanip>

using namespace Microsoft::WRL;
using namespace std;

namespace Lunar
{


void DescriptorAllocator::Initialize(ID3D12Device* device) 
{
    LOG_FUNCTION_ENTRY();
    
    m_device = device;
    
    D3D12_DESCRIPTOR_HEAP_DESC heapDesc = {};
    heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    heapDesc.NumDescriptors = LunarConstants::TOTAL_DESCRIPTORS;
    heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
    heapDesc.NodeMask = 0;
    
    THROW_IF_FAILED(device->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&m_descriptorHeap)))
    
    m_descriptorSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    m_cpuStart = m_descriptorHeap->GetCPUDescriptorHandleForHeapStart();
    m_gpuStart = m_descriptorHeap->GetGPUDescriptorHandleForHeapStart();
    
    // Reserve all predefined ranges from the array
    for (size_t i = 0; i < LunarConstants::RANGE_COUNT; ++i) 
    {
        const auto& rangeInfo = LunarConstants::RANGES[i];
        
        DescriptorRange range;
        range.rangeType = rangeInfo.type;
        range.startIndex = rangeInfo.start;
        range.count = rangeInfo.count;
        range.currentIndex = range.startIndex;
        
        m_enumRanges[rangeInfo.type] = range;
    }
    
    LOG_DEBUG("DescriptorAllocator initialized with range-based allocation");
    LogRangeUsage();
}

UINT DescriptorAllocator::AllocateInRange(LunarConstants::RangeType rangeType, const string& resourceName) 
{
    auto it = m_enumRanges.find(rangeType);
    if (it == m_enumRanges.end()) 
    {
        throw std::runtime_error("Range type not found");
    }
    
    DescriptorRange& range = it->second;
    if (range.IsFull()) 
    {
        throw std::runtime_error("Range (type index)'" + to_string((int)range.rangeType) + "' is full! (" + to_string(range.count) + " slots used)");
    }
    
    UINT index = range.currentIndex++;
    {
        std::lock_guard<std::mutex> lock(m_mapMutex);
        m_nameToIndex[resourceName] = index;
    }
    
    return index;
}

const DescriptorRange* DescriptorAllocator::GetRange(LunarConstants::RangeType rangeType) const 
{
    auto it = m_enumRanges.find(rangeType);
    return (it != m_enumRanges.end()) ? &it->second : nullptr;
}

void DescriptorAllocator::LogRangeUsage() const 
{
    LOG_DEBUG("=== Descriptor Range Usage ===");
    for (const auto& [rangeType, range] : m_enumRanges) 
    {
        float usage = range.GetUsagePercent();
        LOG_DEBUG("Range (Type Index)'", to_string((int)rangeType), "': ", range.GetUsedCount(), "/", range.count, 
                " (", std::fixed, std::setprecision(1), usage, "%)");
        
        if (usage > 80.0f) 
        {
            LOG_WARNING("Range (Type Index)'", to_string((int)rangeType), "' is nearly full!");
        }
    }
}

/*
// DEPRECATED: Use CreateSRVAtRangeIndex for fixed indices to avoid race conditions
UINT DescriptorAllocator::CreateSRV(LunarConstants::RangeType rangeType, 
                                   const string& resourceName,
                                   ID3D12Resource* resource, 
                                   const D3D12_SHADER_RESOURCE_VIEW_DESC* desc) 
{
    const auto* rangeInfo = LunarConstants::FindRange(rangeType);
    if (!rangeInfo) 
    {
        throw std::runtime_error("Invalid range type");
    }
    
    UINT index = AllocateInRange(rangeType, resourceName);
    
    D3D12_CPU_DESCRIPTOR_HANDLE handle = GetCPUHandle(index);
    
    D3D12_SHADER_RESOURCE_VIEW_DESC defaultDesc = {};
    if (!desc) 
    {
        auto resourceDesc = resource->GetDesc();
        defaultDesc.Format = resourceDesc.Format;
        defaultDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
        defaultDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        defaultDesc.Texture2D.MipLevels = resourceDesc.MipLevels;
        desc = &defaultDesc;
    }
    
    m_device->CreateShaderResourceView(resource, desc, handle);

    LOG_DEBUG("Created SRV '", resourceName, "' at index ", index);
    return index;
}
*/

/*
// DEPRECATED: Use CreateUAVAtRangeIndex for fixed indices to avoid race conditions
UINT DescriptorAllocator::CreateUAV(LunarConstants::RangeType rangeType,
                                   const string& resourceName,
                                   ID3D12Resource* resource,
                                   const D3D12_UNORDERED_ACCESS_VIEW_DESC* desc) 
{
    const auto* rangeInfo = LunarConstants::FindRange(rangeType);
    if (!rangeInfo) 
    {
        throw std::runtime_error("Invalid range type");
    }
    
    UINT index = AllocateInRange(rangeType, resourceName);
    
    D3D12_CPU_DESCRIPTOR_HANDLE handle = GetCPUHandle(index);
    
    D3D12_UNORDERED_ACCESS_VIEW_DESC defaultDesc = {};
    if (!desc) 
    {
        auto resourceDesc = resource->GetDesc();
        defaultDesc.Format = resourceDesc.Format;
        defaultDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
        defaultDesc.Texture2D.MipSlice = 0;
        desc = &defaultDesc;
    }
    
    m_device->CreateUnorderedAccessView(resource, nullptr, desc, handle);
    
    LOG_DEBUG("Created UAV '", resourceName, "' at index ", index);
    return index;
}
*/

void DescriptorAllocator::CreateSRVAtRangeIndex(LunarConstants::RangeType rangeType, UINT relativeIndex, const std::string& resourceName, ID3D12Resource* resource, const D3D12_SHADER_RESOURCE_VIEW_DESC* desc)
{
    
    const auto* rangeInfo = LunarConstants::FindRange(rangeType);
    UINT absoluteIndex = rangeInfo->start + relativeIndex;
    
    CreateSRVAtAbsoluteIndex(absoluteIndex, resourceName, resource, desc);
    
    LOG_DEBUG("Created SRV '", resourceName, "' at range index ", relativeIndex, " (absolute: ", absoluteIndex, ")");
}

void DescriptorAllocator::CreateUAVAtRangeIndex(LunarConstants::RangeType rangeType, UINT relativeIndex, const std::string& resourceName, ID3D12Resource* resource, const D3D12_UNORDERED_ACCESS_VIEW_DESC* desc)
{
    const auto* rangeInfo = LunarConstants::FindRange(rangeType);
    UINT absoluteIndex = rangeInfo->start + relativeIndex;
    
    CreateUAVAtAbsoluteIndex(absoluteIndex, resourceName, resource, desc);
    
    LOG_DEBUG("Created UAV '", resourceName, "' at range index ", relativeIndex, " (absolute: ", absoluteIndex, ")");
}

void DescriptorAllocator::CreateUAVAtAbsoluteIndex(UINT absoluteIndex, const std::string& resourceName, ID3D12Resource* resource, const D3D12_UNORDERED_ACCESS_VIEW_DESC* desc)
{
    std::lock_guard<std::mutex> lock(m_mapMutex);  
    
    // check the index is used 
    auto existingIt = std::find_if(m_nameToIndex.begin(), m_nameToIndex.end(),
        [absoluteIndex](const auto& pair) { return pair.second == absoluteIndex; });
    
    if (existingIt != m_nameToIndex.end()) 
    {
        LOG_ERROR("Overwriting existing UAV '", existingIt->first, "' at index ", absoluteIndex, " with '", resourceName, "'");
    }
    
    D3D12_CPU_DESCRIPTOR_HANDLE handle = GetCPUHandle(absoluteIndex);
    
    D3D12_UNORDERED_ACCESS_VIEW_DESC defaultDesc = {};
    if (!desc) 
    {
        auto resourceDesc = resource->GetDesc();
        defaultDesc.Format = resourceDesc.Format;
        defaultDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
        defaultDesc.Texture2D.MipSlice = 0;
        desc = &defaultDesc;
    }
    
    m_device->CreateUnorderedAccessView(resource, nullptr, desc, handle);
    m_nameToIndex[resourceName] = absoluteIndex;
    
    LOG_DEBUG("Created UAV '", resourceName, "' at absolute index ", absoluteIndex);
}

void DescriptorAllocator::CreateSRVAtAbsoluteIndex(UINT absoluteIndex, const std::string& resourceName, ID3D12Resource* resource, const D3D12_SHADER_RESOURCE_VIEW_DESC* desc)
{
    std::lock_guard<std::mutex> lock(m_mapMutex);  
    
    // check the index is used 
    auto existingIt = std::find_if(m_nameToIndex.begin(), m_nameToIndex.end(),
        [absoluteIndex](const auto& pair) { return pair.second == absoluteIndex; });
    
    if (existingIt != m_nameToIndex.end()) 
    {
        LOG_ERROR("Overwriting existing SRV '", existingIt->first, "' at index ", absoluteIndex, " with '", resourceName, "'");
    }
    
    D3D12_CPU_DESCRIPTOR_HANDLE handle = GetCPUHandle(absoluteIndex);
    
    D3D12_SHADER_RESOURCE_VIEW_DESC defaultDesc = {};
    if (!desc) 
    {
        auto resourceDesc = resource->GetDesc();
        defaultDesc.Format = resourceDesc.Format;
        defaultDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
        defaultDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        defaultDesc.Texture2D.MipLevels = resourceDesc.MipLevels;
        desc = &defaultDesc;
    }
    
    m_device->CreateShaderResourceView(resource, desc, handle);
    m_nameToIndex[resourceName] = absoluteIndex;
    
    LOG_DEBUG("Created SRV '", resourceName, "' at absolute index ", absoluteIndex);
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

D3D12_CPU_DESCRIPTOR_HANDLE DescriptorAllocator::GetCPUHandle(const string& name)
{
    auto it = m_nameToIndex.find(name);
    if (it != m_nameToIndex.end())
    {
        return GetCPUHandle(it->second);
    }
    
    LOG_ERROR("Descriptor not found: ", name);
    return {};
}

D3D12_GPU_DESCRIPTOR_HANDLE DescriptorAllocator::GetGPUHandle(const string& name)
{
    auto it = m_nameToIndex.find(name);
    if (it != m_nameToIndex.end())
    {
        return GetGPUHandle(it->second);
    }
    
    LOG_ERROR("Descriptor not found: ", name);
    return {};
}

// void DescriptorAllocator::CreateSRV(ID3D12Resource* resource, const D3D12_SHADER_RESOURCE_VIEW_DESC* desc, const string& name)
// {
//     auto it = m_nameToIndex.find(name);
//     if (it == m_nameToIndex.end())
//     {
//         LOG_ERROR("Descriptor not allocated: ", name);
//         return;
//     }
//     
//     m_device->CreateShaderResourceView(resource, desc, GetCPUHandle(it->second));
//     LOG_DEBUG("SRV created for '", name, "' at index ", it->second);
// }
//
// void DescriptorAllocator::CreateUAV(ID3D12Resource* resource, const D3D12_UNORDERED_ACCESS_VIEW_DESC* desc, const string& name)
// {
//     auto it = m_nameToIndex.find(name);
//     if (it == m_nameToIndex.end())
//     {
//         LOG_ERROR("Descriptor not allocated: ", name);
//         return;
//     }
//     
//     m_device->CreateUnorderedAccessView(resource, nullptr, desc, GetCPUHandle(it->second));
//     LOG_DEBUG("UAV created for '", name, "' at index ", it->second);
// }

UINT DescriptorAllocator::GetDescriptorIndex(const string& name) const
{
    auto it = m_nameToIndex.find(name);
    return (it != m_nameToIndex.end()) ? it->second : UINT_MAX;
}

} // namespace Lunar
