#pragma once
#include <d3d12.h>
#include <wrl/client.h>
#include <string>
#include <unordered_map>
#include "LunarConstants.h"

namespace Lunar
{

struct DescriptorRange 
{
    LunarConstants::RangeType rangeType;
    UINT startIndex;
    UINT count;
    UINT currentIndex;
    
    bool IsFull() const { return currentIndex >= startIndex + count; }
    UINT GetUsedCount() const { return currentIndex - startIndex; }
    float GetUsagePercent() const { return (float)GetUsedCount() / count * 100.0f; }
};

class DescriptorAllocator
{
public:
    void Initialize(ID3D12Device* device);  
    
    UINT CreateSRV(LunarConstants::RangeType rangeType, const std::string& resourceName,
                   ID3D12Resource* resource, const D3D12_SHADER_RESOURCE_VIEW_DESC* desc = nullptr);
    
    UINT CreateUAV(LunarConstants::RangeType rangeType, const std::string& resourceName,
                   ID3D12Resource* resource, const D3D12_UNORDERED_ACCESS_VIEW_DESC* desc = nullptr);
    
    void CreateSRVAtRangeIndex(LunarConstants::RangeType rangeType,
                              UINT relativeIndex,
                              const std::string& resourceName,
                              ID3D12Resource* resource,
                              const D3D12_SHADER_RESOURCE_VIEW_DESC* desc = nullptr);
    
    void CreateSRVAtAbsoluteIndex(UINT absoluteIndex,
                                 const std::string& resourceName,
                                 ID3D12Resource* resource,
                                 const D3D12_SHADER_RESOURCE_VIEW_DESC* desc = nullptr);
    
    const DescriptorRange* GetRange(LunarConstants::RangeType rangeType) const;
    
    void LogRangeUsage() const;
    
    D3D12_CPU_DESCRIPTOR_HANDLE GetCPUHandle(UINT index);
    D3D12_GPU_DESCRIPTOR_HANDLE GetGPUHandle(UINT index);
    D3D12_CPU_DESCRIPTOR_HANDLE GetCPUHandle(const std::string& name);
    D3D12_GPU_DESCRIPTOR_HANDLE GetGPUHandle(const std::string& name);
    ID3D12DescriptorHeap* GetHeap() const { return m_descriptorHeap.Get(); }
    UINT GetDescriptorIndex(const std::string& name) const;
    
    // Obsolete
    // void CreateSRV(ID3D12Resource* resource, const D3D12_SHADER_RESOURCE_VIEW_DESC* desc, const std::string& name);
    // void CreateUAV(ID3D12Resource* resource, const D3D12_UNORDERED_ACCESS_VIEW_DESC* desc, const std::string& name);
    
private:
    Microsoft::WRL::ComPtr<ID3D12Device> m_device; // TODO: Delete
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> m_descriptorHeap;
	UINT m_currentIndex = 0;
    UINT m_descriptorSize;
    D3D12_CPU_DESCRIPTOR_HANDLE m_cpuStart;
    D3D12_GPU_DESCRIPTOR_HANDLE m_gpuStart;
    
    std::unordered_map<std::string, UINT> m_nameToIndex;
    std::unordered_map<LunarConstants::RangeType, DescriptorRange> m_enumRanges;

    UINT AllocateInRange(LunarConstants::RangeType rangeType, const std::string& resourceName);
};

} // namespace Lunar
