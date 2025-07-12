#include "Geometry.h"
#include "../Utils/Utils.h" 
#include "../Utils/Logger.h"

using namespace DirectX;
using namespace std;

namespace Lunar
{
Geometry::Geometry()
{
    XMStoreFloat4x4(&m_objectConstants.World, XMMatrixIdentity());
	// UpdateWorldMatrix();
}

void Geometry::Initialize(ID3D12Device* device)
{
	LOG_FUNCTION_ENTRY();
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
    commandList->IASetPrimitiveTopology(m_topologyType);
    commandList->DrawIndexedInstanced(static_cast<UINT>(m_indices.size()), 1, 0, 0, 0);
}

void Geometry::DrawNormals(ID3D12GraphicsCommandList* commandList)
{
	if (m_needsConstantBufferUpdate)
	{
		UpdateObjectConstants();
	}
    
	BindObjectConstants(commandList);
	commandList->IASetVertexBuffers(0, 1, &m_vertexBufferView);
	commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_POINTLIST);
	commandList->DrawInstanced(static_cast<UINT>(m_vertices.size()), 1, 0, 0);
}

void Geometry::SetWorldMatrix(DirectX::XMFLOAT4X4 worldMatrix)
{
	m_objectConstants.World = worldMatrix;
	m_needsConstantBufferUpdate = true;
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
    for (auto& vertex : m_vertices)
    {
        vertex.color = color;
    }
}

void Geometry::SetTextureIndex(int index)
{
	m_objectConstants.textureIndex = index;
	m_needsConstantBufferUpdate = true;
}

void Geometry::SetMaterialName(const std::string& materialName)
{
	m_materialName = materialName;
}

void Geometry::UpdateWorldMatrix()
{
    XMMATRIX S = XMMatrixScaling(m_transform.Scale.x, m_transform.Scale.y, m_transform.Scale.z);
    XMMATRIX R = XMMatrixRotationRollPitchYaw(m_transform.Rotation.x, m_transform.Rotation.y, m_transform.Rotation.z);
    XMMATRIX T = XMMatrixTranslation(m_transform.Location.x, m_transform.Location.y, m_transform.Location.z);
    
    XMMATRIX world = S * R * T;
    XMStoreFloat4x4(&m_objectConstants.World, XMMatrixTranspose(world));
}

void Geometry::UpdateObjectConstants()
{
    XMMATRIX worldMatrix = XMLoadFloat4x4(&m_objectConstants.World);
	worldMatrix.r[3] = XMVectorSet(0.0f, 0.0f, 0.0f, 1.0f);
	XMMATRIX worldInv = XMMatrixInverse(nullptr, worldMatrix);
	XMMATRIX worldInvTranspose = XMMatrixTranspose(worldInv);
	XMStoreFloat4x4(&m_objectConstants.WorldInvTranspose, XMMatrixTranspose(worldInvTranspose));
    m_objectCB->CopyData(&m_objectConstants, sizeof(ObjectConstants));
    m_needsConstantBufferUpdate = false;
}

void Geometry::BindObjectConstants(ID3D12GraphicsCommandList* commandList)
{
    if (m_objectCB)
    {
        commandList->SetGraphicsRootConstantBufferView(
            Lunar::LunarConstants::OBJECT_CONSTANTS_ROOT_PARAMETER_INDEX, 
            m_objectCB->GetResource()->GetGPUVirtualAddress());
    }
}

void Geometry::ComputeTangents()
{
	auto outVerts = m_vertices;
	vector<XMFLOAT3> tanAccum(outVerts.size(), {0.0f, 0.0f, 0.0f});
	for (size_t i = 0; i < m_indices.size(); i += 3)
	{
		uint16_t index0 = m_indices[i];
		uint16_t index1 = m_indices[i + 1];
		uint16_t index2 = m_indices[i + 2];

		XMVECTOR pos0 = XMLoadFloat3(&outVerts[index0].pos);
		XMVECTOR pos1 = XMLoadFloat3(&outVerts[index1].pos);
		XMVECTOR pos2 = XMLoadFloat3(&outVerts[index2].pos);

		XMVECTOR uv0 = XMLoadFloat2(&outVerts[index0].texCoord);
		XMVECTOR uv1 = XMLoadFloat2(&outVerts[index1].texCoord);
		XMVECTOR uv2 = XMLoadFloat2(&outVerts[index2].texCoord);

		XMVECTOR deltaPos1 = pos1 - pos0;
		XMVECTOR deltaPos2 = pos2 - pos0;

		float deltaU1 = XMVectorGetX(uv1) - XMVectorGetX(uv0);
		float deltaU2 = XMVectorGetX(uv2) - XMVectorGetX(uv0);
		float deltaV1 = XMVectorGetY(uv1) - XMVectorGetY(uv0);
		float deltaV2 = XMVectorGetY(uv2) - XMVectorGetY(uv0);

		float det = deltaU1 * deltaV2 - deltaU2 * deltaV1;
		if (abs(det) < 1e-6) continue;
		float r = 1.0f / det;
		
		XMVECTOR tangent = (deltaPos1 * deltaV2 - deltaPos2 * deltaV1) * r;

		for (uint16_t idx : {index0, index1, index2})
		{
			XMVECTOR accum = XMLoadFloat3(&tanAccum[idx]) + tangent;
			XMStoreFloat3(&tanAccum[idx], accum);
		}
	}

	for (size_t i = 0; i < outVerts.size(); ++i)
	{
		XMVECTOR n = XMLoadFloat3(&outVerts[i].normal);
		XMVECTOR t = XMLoadFloat3(&tanAccum[i]);

		t = XMVector3Normalize(t - n * XMVector3Dot(n, t));
		XMStoreFloat3(&outVerts[i].tangentU, t);
	}

	m_vertices = move(outVerts);
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

	if (m_indices.empty()) return;
	
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

} // namespace Lunar
