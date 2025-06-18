#include "Cube.h"

#include "Logger.h"
#include "MainApp.h"
#include "Utils.h"

namespace Lunar
{
Cube::Cube() :
	m_color(1.0f, 1.0f, 1.0f, 1.0f),
	m_scale(1.0f),
	m_position(0.0f, 0.0f, 0.0f),
	m_rotation(0.0f, 0.0f, 0.0f)
{
	XMStoreFloat4x4(&m_world, XMMatrixIdentity());
	CreateGeometry();
}

void Cube::Initialize(ID3D12Device* device, ID3D12GraphicsCommandList* commandList)
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

	// 인덱스 데이터 복사
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

void Cube::Draw(ID3D12GraphicsCommandList* commandList)
{
	commandList->IASetVertexBuffers(0, 1, &m_vertexBufferView);
	commandList->IASetIndexBuffer(&m_indexBufferView);
	commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	commandList->DrawIndexedInstanced(static_cast<UINT>(m_indices.size()), 1, 0, 0, 0);
}

void Cube::SetPosition(const XMFLOAT3& position)
{
	m_position = position;
	UpdateWorldMatrix();
}

void Cube::SetRotation(const XMFLOAT3& rotation)
{
	m_rotation = rotation;
	UpdateWorldMatrix();
}

void Cube::SetScale(const float scale)
{
	m_scale = scale;
	UpdateWorldMatrix();
}

void Cube::SetColor(const XMFLOAT4& color)
{
	m_color = color;
}

void Cube::CreateGeometry()
{
	// definite 8 vertices of a cube for smooth normal
	m_vertices = {
		{ { -0.5f, -0.5f, -0.5f },    { 0.0f, 0.0f, 0.0f, 1.0f },   { 0.0f, 0.0f },   { 0.0f, 0.0f, 0.0f } }, // 0
		{ { -0.5f, +0.5f, -0.5f },    { 0.0f, 1.0f, 0.0f, 1.0f },   { 0.0f, 1.0f },   { 0.0f, 0.0f, 0.0f } }, // 1
		{ { +0.5f, +0.5f, -0.5f },    { 1.0f, 1.0f, 0.0f, 1.0f },   { 1.0f, 1.0f },   { 0.0f, 0.0f, 0.0f } }, // 2
		{ { +0.5f, -0.5f, -0.5f },    { 1.0f, 0.0f, 0.0f, 1.0f },   { 1.0f, 0.0f },   { 0.0f, 0.0f, 0.0f } }, // 3

		{ { -0.5f, -0.5f, +0.5f },    { 0.0f, 0.0f, 1.0f, 1.0f },   { 0.0f, 0.0f },   { 0.0f, 0.0f, 0.0f } }, // 4
		{ { -0.5f, +0.5f, +0.5f },    { 0.0f, 1.0f, 1.0f, 1.0f },   { 0.0f, 1.0f },   { 0.0f, 0.0f, 0.0f } }, // 5
		{ { +0.5f, +0.5f, +0.5f },    { 1.0f, 1.0f, 1.0f, 1.0f },   { 1.0f, 1.0f },   { 0.0f, 0.0f, 0.0f } }, // 6
		{ { +0.5f, -0.5f, +0.5f },    { 1.0f, 0.0f, 1.0f, 1.0f },   { 1.0f, 0.0f },   { 0.0f, 0.0f, 0.0f } }, // 7
	};
	
	 m_indices = {
		// front
		0,1,2, 0,2,3,
		// back
		4,7,6, 4,6,5,
		// top
		1,5,6, 1,6,2,
		// bottom
		4,0,3, 4,3,7,
		// left
		4,5,1, 4,1,0,
		// right
		3,2,6, 3,6,7
	};
	

	std::vector<XMFLOAT3> normalSum(m_vertices.size(), {0, 0, 0});
	for (size_t i = 0; i < m_indices.size(); i += 3)
	{
		uint16_t index0 = m_indices[i];
		uint16_t index1 = m_indices[i + 1];
		uint16_t index2 = m_indices[i + 2];

		Vertex& v0 = m_vertices[index0];
		Vertex& v1 = m_vertices[index1];
		Vertex& v2 = m_vertices[index2];

		XMVECTOR p0 = XMLoadFloat3(&m_vertices[index0].pos);
		XMVECTOR p1 = XMLoadFloat3(&m_vertices[index1].pos);
		XMVECTOR p2 = XMLoadFloat3(&m_vertices[index2].pos);

		XMVECTOR e1 = p1 - p0;
		XMVECTOR e2 = p2 - p0;

		XMVECTOR faceNormal = XMVector3Normalize(XMVector3Cross(e1, e2));

		XMFLOAT3 fn;
		XMStoreFloat3(&fn, faceNormal);
		auto accumulate = [&normalSum, &fn](uint16_t idx) {
			normalSum[idx].x += fn.x;
			normalSum[idx].y += fn.y;
			normalSum[idx].z += fn.z;
		};
		accumulate(index0);
		accumulate(index1);
		accumulate(index2);
	}
	for (size_t i = 0; i < m_vertices.size(); ++i)
	{
		LOG_DEBUG("normal.x : ", normalSum[i].x, ", normal.y : ", normalSum[i].y, ", normal.z : ", normalSum[i].z);
		XMVECTOR normal = XMVector3Normalize(XMLoadFloat3(&normalSum[i]));
		XMStoreFloat3(&m_vertices[i].normal, normal);
	}
}

void Cube::UpdateWorldMatrix()
{
	// W = SRT
	XMMATRIX scale = XMMatrixScaling(m_scale, m_scale, m_scale);
	XMMATRIX rotation = XMMatrixRotationRollPitchYaw(m_rotation.x, m_rotation.y, m_rotation.z);
	XMMATRIX translation = XMMatrixTranslation(m_position.x, m_position.y, m_position.z);
	XMMATRIX world = scale * rotation * translation;
	XMStoreFloat4x4(&m_world, world);
}
} // namespace Lunar