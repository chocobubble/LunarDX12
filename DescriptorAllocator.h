#pragma once
#include <d3d12.h>
#include <wrl/client.h>
#include <vector>
#include <string>
#include <unordered_map>

namespace Lunar
{

class DescriptorAllocator
{
public:
    void Initialize(ID3D12Device* device, UINT maxDescriptors = 1000);
    
    UINT AllocateDescriptor(const std::string& name);
    
    D3D12_CPU_DESCRIPTOR_HANDLE GetCPUHandle(UINT index);
    D3D12_GPU_DESCRIPTOR_HANDLE GetGPUHandle(UINT index);
    D3D12_CPU_DESCRIPTOR_HANDLE GetCPUHandle(const std::string& name);
    D3D12_GPU_DESCRIPTOR_HANDLE GetGPUHandle(const std::string& name);
    
    void CreateSRV(ID3D12Resource* resource, const D3D12_SHADER_RESOURCE_VIEW_DESC* desc, const std::string& name);
    void CreateUAV(ID3D12Resource* resource, const D3D12_UNORDERED_ACCESS_VIEW_DESC* desc, const std::string& name);
    
    ID3D12DescriptorHeap* GetHeap() const { return m_descriptorHeap.Get(); }
    
    void PrintAllocation();
    
    UINT GetDescriptorIndex(const std::string& name) const;
    
private:
    Microsoft::WRL::ComPtr<ID3D12Device> m_device;
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> m_descriptorHeap;
	UINT m_currentIndex = 0;
    UINT m_descriptorSize;
    D3D12_CPU_DESCRIPTOR_HANDLE m_cpuStart;
    D3D12_GPU_DESCRIPTOR_HANDLE m_gpuStart;
    
    std::vector<bool> m_allocated; 
    std::unordered_map<std::string, UINT> m_nameToIndex; 
    
    UINT FindFreeRange(UINT count);
};

} // namespace Lunar
