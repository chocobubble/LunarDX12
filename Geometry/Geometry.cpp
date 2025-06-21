#include "Geometry.h"
#include "../Utils.h" 
#include "../SceneRenderer.h"  // Transform 

using namespace DirectX;

namespace Lunar
{
Geometry::Geometry()
{
    XMStoreFloat4x4(&m_world, XMMatrixIdentity());
}

void Geometry::Initialize(ID3D12Device* device)
{
    CreateGeometry();  
    CreateBuffers(device); 
    m_objectCB = std::make_unique<UploadBuffer<ObjectConstants>>(device, sizeof(ObjectConstants));
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
    XMStoreFloat4x4(&m_world, world);
}

void Geometry::UpdateObjectConstants()
{
    if (!m_mappedObjectCB) 
    {
        LOG_ERROR("Object CB is not mapped!");
        return;
    }
    
    m_objectConstants.World = m_world;
    
    m_objectCB->CopyData(m_objectConstants, sizeof(ObjectConstants));
    m_needsConstantBufferUpdate = false;
}

void Geometry::BindObjectConstants(ID3D12GraphicsCommandList* commandList)
{
    if (m_objectConstantBuffer)
    {
        commandList->SetGraphicsRootConstantBufferView(
            Lunar::Constants::OBJECT_CONSTANTS_ROOT_PARAMETER_INDEX, 
            m_objectConstantBuffer->GetGPUVirtualAddress());
    }
}

void Geometry::CreateBuffers(ID3D12Device* device)
{
	const UINT vbByteSize = static_cast<UINT>(m_vertices.size() * sizeof(Vertex));
	const UINT ibByteSize = static_cast<UINT>(m_indices.size() * sizeof(uint16_t));

	// Create vertex buffer
	auto vbHeapProps = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
	auto vbResourceDesc = CD3DX12_RESOURCE_DESC::Buffer(vbByteSize);
	
	THROW_IF_FAILED(device->CreateCommittedResource(
		&vbHeapProps,
		D3D12_HEAP_FLAG_NONE,
		&vbResourceDesc,
		D3D12_RESOURCE_STATE_COMMON,
		nullptr,
		IID_PPV_ARGS(m_vertexBuffer.GetAddressOf())));

	// Create index buffer
	auto ibHeapProps = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
	auto ibResourceDesc = CD3DX12_RESOURCE_DESC::Buffer(ibByteSize);
	
	THROW_IF_FAILED(device->CreateCommittedResource(
		&ibHeapProps,
		D3D12_HEAP_FLAG_NONE,
		&ibResourceDesc,
		D3D12_RESOURCE_STATE_COMMON,
		nullptr,
		IID_PPV_ARGS(m_indexBuffer.GetAddressOf())));

	// Upload vertex data
	Microsoft::WRL::ComPtr<ID3D12Resource> vbUploadBuffer;
	auto vbUploadHeapProps = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
	auto vbUploadResourceDesc = CD3DX12_RESOURCE_DESC::Buffer(vbByteSize);
	
	THROW_IF_FAILED(device->CreateCommittedResource(
		&vbUploadHeapProps,
		D3D12_HEAP_FLAG_NONE,
		&vbUploadResourceDesc,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(vbUploadBuffer.GetAddressOf())));

	D3D12_SUBRESOURCE_DATA vbData = {};
	vbData.pData = m_vertices.data();
	vbData.RowPitch = vbByteSize;
	vbData.SlicePitch = vbData.RowPitch;

	// Upload index data
	Microsoft::WRL::ComPtr<ID3D12Resource> ibUploadBuffer;
	auto ibUploadHeapProps = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
	auto ibUploadResourceDesc = CD3DX12_RESOURCE_DESC::Buffer(ibByteSize);
	
	THROW_IF_FAILED(device->CreateCommittedResource(
		&ibUploadHeapProps,
		D3D12_HEAP_FLAG_NONE,
		&ibUploadResourceDesc,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(ibUploadBuffer.GetAddressOf())));

	D3D12_SUBRESOURCE_DATA ibData = {};
	ibData.pData = m_indices.data();
	ibData.RowPitch = ibByteSize;
	ibData.SlicePitch = ibData.RowPitch;

	// Create buffer views
	m_vertexBufferView.BufferLocation = m_vertexBuffer->GetGPUVirtualAddress();
	m_vertexBufferView.StrideInBytes = sizeof(Vertex);
	m_vertexBufferView.SizeInBytes = vbByteSize;

	m_indexBufferView.BufferLocation = m_indexBuffer->GetGPUVirtualAddress();
	m_indexBufferView.Format = DXGI_FORMAT_R16_UINT;
	m_indexBufferView.SizeInBytes = ibByteSize;
}

}
