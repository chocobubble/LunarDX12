#include "ConstantBuffers.h"

#include <d3d12.h>

#include "Utils.h"

namespace Lunar
{

ConstantBuffer::ConstantBuffer(ID3D12Device* device, UINT elementByteSize, ID3D12DescriptorHeap* cbvHeap)
{
	elementByteSize = Utils::CalculateConstantBufferByteSize(elementByteSize);
	
	D3D12_HEAP_PROPERTIES heapProperties = {};
	heapProperties.Type = D3D12_HEAP_TYPE_UPLOAD;
	heapProperties.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
	heapProperties.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
	heapProperties.CreationNodeMask = 1;
	heapProperties.VisibleNodeMask = 1;

	D3D12_RESOURCE_DESC bufferDesc = {};
	bufferDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	bufferDesc.Alignment = 0;
	bufferDesc.Width = elementByteSize;
	bufferDesc.Height = 1;
	bufferDesc.DepthOrArraySize = 1;
	bufferDesc.MipLevels = 1;
	bufferDesc.Format = DXGI_FORMAT_UNKNOWN;
	bufferDesc.SampleDesc.Count = 1;
	bufferDesc.SampleDesc.Quality = 0;
	bufferDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
	bufferDesc.Flags = D3D12_RESOURCE_FLAG_NONE;
	
	THROW_IF_FAILED(device->CreateCommittedResource(
		&heapProperties,
		D3D12_HEAP_FLAG_NONE,
		&bufferDesc,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(m_constantBuffer.GetAddressOf())
	))

	THROW_IF_FAILED(m_constantBuffer->Map(0, nullptr, reinterpret_cast<void**>(&m_mappedData)))
}


ConstantBuffer::~ConstantBuffer()
{
	if (m_constantBuffer) {
		m_constantBuffer->Unmap(0, nullptr);
	}
	m_mappedData = nullptr;
}

void ConstantBuffer::CopyData(void* data, UINT size)
{
	memcpy(m_mappedData, data, size);
}
} // namespace Lunar