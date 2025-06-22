#include "Geometry.h"
#include "../Utils.h" 
#include "../Logger.h"

using namespace DirectX;

namespace Lunar
{
Geometry::Geometry()
{
    XMStoreFloat4x4(&m_objectConstants.World, XMMatrixIdentity());
}

void Geometry::Initialize(ID3D12Device* device)
{
    CreateGeometry();  
    CreateBuffers(device); 
    m_objectCB = std::make_unique<ConstantBuffer>(device, sizeof(ObjectConstants));
}

void Geometry::Draw(ID3D12GraphicsCommandList* commandList)
{
    if (m_needsConstantBufferUpdate)
    {
        UpdateObjectConstants();
    }
    
    BindObjectConstants(commandList);
    commandList->IASetVertexBuffers(0, 1, &m_vertexBufferView);
    commandList->IASetIndexBuffer(&m_indexBufferView);
    commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    commandList->DrawIndexedInstanced(static_cast<UINT>(m_indices.size()), 1, 0, 0, 0);
}

void Geometry::SetTransform(const Transform& transform)
{
    m_transform = transform;
    UpdateWorldMatrix();
    m_needsConstantBufferUpdate = true;
}

void Geometry::SetLocation(const XMFLOAT3& location)
{
    m_transform.Location = location;
    UpdateWorldMatrix();
    m_needsConstantBufferUpdate = true;
}

void Geometry::SetRotation(const XMFLOAT3& rotation)
{
    m_transform.Rotation = rotation;
    UpdateWorldMatrix();
    m_needsConstantBufferUpdate = true;
}

void Geometry::SetScale(const XMFLOAT3& scale)
{
    m_transform.Scale = scale;
    UpdateWorldMatrix();
    m_needsConstantBufferUpdate = true;
}

void Geometry::SetColor(const XMFLOAT4& color)
{
    m_color = color;
}

void Geometry::UpdateWorldMatrix()
{
    XMMATRIX S = XMMatrixScaling(m_transform.Scale.x, m_transform.Scale.y, m_transform.Scale.z);
    XMMATRIX R = XMMatrixRotationRollPitchYaw(m_transform.Rotation.x, m_transform.Rotation.y, m_transform.Rotation.z);
    XMMATRIX T = XMMatrixTranslation(m_transform.Location.x, m_transform.Location.y, m_transform.Location.z);
    
    XMMATRIX world = S * R * T;
    XMStoreFloat4x4(&m_objectConstants.World, world);
}

void Geometry::UpdateObjectConstants()
{
    m_objectCB->CopyData(&m_objectConstants, sizeof(ObjectConstants));
    m_needsConstantBufferUpdate = false;
}

void Geometry::BindObjectConstants(ID3D12GraphicsCommandList* commandList)
{
    if (m_objectCB)
    {
        commandList->SetGraphicsRootConstantBufferView(
            Lunar::Constants::OBJECT_CONSTANTS_ROOT_PARAMETER_INDEX, 
            m_objectCB->GetResource()->GetGPUVirtualAddress());
    }
}

void Geometry::CreateBuffers(ID3D12Device* device)
{
	const UINT vbByteSize = static_cast<UINT>(m_vertices.size() * sizeof(Vertex));

	/*
	typedef struct D3D12_HEAP_PROPERTIES
	{
		D3D12_HEAP_TYPE Type;
		D3D12_CPU_PAGE_PROPERTY CPUPageProperty;
		D3D12_MEMORY_POOL MemoryPoolPreference;
		UINT CreationNodeMask;
		UINT VisibleNodeMask;
	} 	D3D12_HEAP_PROPERTIES;
	*/
	D3D12_HEAP_PROPERTIES heapProperties= {};
	heapProperties.Type = D3D12_HEAP_TYPE_UPLOAD;
	heapProperties.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
	heapProperties.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
	heapProperties.CreationNodeMask = 1;
	heapProperties.VisibleNodeMask = 1;

	/*
	typedef struct D3D12_RESOURCE_DESC
	{
		D3D12_RESOURCE_DIMENSION Dimension;
		UINT64 Alignment;
		UINT64 Width;
		UINT Height;
		UINT16 DepthOrArraySize;
		UINT16 MipLevels;
		DXGI_FORMAT Format;
		DXGI_SAMPLE_DESC SampleDesc;
		D3D12_TEXTURE_LAYOUT Layout;
		D3D12_RESOURCE_FLAGS Flags;
	} 	D3D12_RESOURCE_DESC;
	*/
	D3D12_RESOURCE_DESC vbDesc = {};
	vbDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	vbDesc.Alignment = 0;
	vbDesc.Width = vbByteSize;
	vbDesc.Height = 1;
	vbDesc.DepthOrArraySize = 1;
	vbDesc.MipLevels = 1;
	vbDesc.Format = DXGI_FORMAT_UNKNOWN;
	vbDesc.SampleDesc.Count = 1;
	vbDesc.SampleDesc.Quality = 0;
	vbDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
	vbDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

	THROW_IF_FAILED(device->CreateCommittedResource(
		&heapProperties,
		D3D12_HEAP_FLAG_NONE,
		&vbDesc,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(m_vertexBuffer.GetAddressOf())
		))

	// copy vertex data
	UINT8* pVertexDataBegin = nullptr;
	m_vertexBuffer->Map(0, nullptr, reinterpret_cast<void**>(&pVertexDataBegin));
	memcpy(pVertexDataBegin, m_vertices.data(), vbByteSize);
	m_vertexBuffer->Unmap(0, nullptr);
	
	/*
	typedef struct D3D12_VERTEX_BUFFER_VIEW
	{
		D3D12_GPU_VIRTUAL_ADDRESS BufferLocation;
		UINT SizeInBytes;
		UINT StrideInBytes;
	} 	D3D12_VERTEX_BUFFER_VIEW;
	*/
	m_vertexBufferView.BufferLocation = m_vertexBuffer->GetGPUVirtualAddress();
	m_vertexBufferView.StrideInBytes = sizeof(Vertex);
	m_vertexBufferView.SizeInBytes = vbByteSize;

	const UINT ibByteSize = static_cast<UINT>(m_indices.size() * sizeof(uint16_t));
	D3D12_RESOURCE_DESC ibDesc = {};
	ibDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	ibDesc.Width = ibByteSize;
	ibDesc.Height = 1;
	ibDesc.DepthOrArraySize = 1;
	ibDesc.MipLevels = 1;
	ibDesc.Format = DXGI_FORMAT_UNKNOWN;
	ibDesc.SampleDesc.Count = 1;
	ibDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

	THROW_IF_FAILED(device->CreateCommittedResource(
		&heapProperties,
		D3D12_HEAP_FLAG_NONE,
		&ibDesc,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(m_indexBuffer.GetAddressOf())
		))

	// copy index data
	BYTE* pIndexDataBegin = nullptr;
	m_indexBuffer->Map(0, nullptr, reinterpret_cast<void**>(&pIndexDataBegin));
	memcpy(pIndexDataBegin, m_indices.data(), ibByteSize);
	m_indexBuffer->Unmap(0, nullptr);

	/*
	typedef struct D3D12_INDEX_BUFFER_VIEW
	{
		D3D12_GPU_VIRTUAL_ADDRESS BufferLocation;
		UINT SizeInBytes;
		DXGI_FORMAT Format;
	} 	D3D12_INDEX_BUFFER_VIEW;
	*/
	m_indexBufferView.BufferLocation = m_indexBuffer->GetGPUVirtualAddress();
	m_indexBufferView.Format = DXGI_FORMAT_R16_UINT;
	m_indexBufferView.SizeInBytes = ibByteSize;
}

}
