#pragma once
#include <vector>
#include <d3d12.h>
#include <DirectXMath.h>
#include <memory>
#include <string>
#include <wrl/client.h>

#include "Transform.h"
#include "Vertex.h"
#include "../ConstantBuffers.h"

struct ObjectConstants;

namespace Lunar
{
class Geometry
{
public:
    Geometry();
    virtual ~Geometry() = default;

    virtual void CreateGeometry() = 0;
    
    virtual void Initialize(ID3D12Device* device);
    virtual void Draw(ID3D12GraphicsCommandList* commandList);
	virtual void DrawNormals(ID3D12GraphicsCommandList* commandList);

	void SetWorldMatrix(DirectX::XMFLOAT4X4 worldMatrix);
    void SetTransform(const Transform& transform);
    void SetLocation(const DirectX::XMFLOAT3& location);
    void SetRotation(const DirectX::XMFLOAT3& rotation);
    void SetScale(const DirectX::XMFLOAT3& scale);
    void SetColor(const DirectX::XMFLOAT4& color);  // TODO: delete
	void SetTextureIndex(int index);
    void SetMaterialName(const std::string& materialName);
	void SetTopologyType(D3D_PRIMITIVE_TOPOLOGY topologyType) { m_topologyType = topologyType; }
    
    DirectX::XMFLOAT4X4 GetWorldMatrix() { return m_objectConstants.World; }
    const Transform& GetTransform() const { return m_transform; }
    const DirectX::XMFLOAT3& GetLocation() const { return m_transform.Location; }
    const DirectX::XMFLOAT3& GetRotation() const { return m_transform.Rotation; }
    const DirectX::XMFLOAT3& GetScale() const { return m_transform.Scale; }
    const std::string& GetMaterialName() const { return m_materialName; }
    
    void UpdateObjectConstants();
    void BindObjectConstants(ID3D12GraphicsCommandList* commandList);
	void ComputeTangents();
    
protected:
    std::vector<Vertex> m_vertices;
    std::vector<uint16_t> m_indices;

	ObjectConstants m_objectConstants;
    Transform m_transform; 
    
    Microsoft::WRL::ComPtr<ID3D12Resource> m_vertexBuffer;
    Microsoft::WRL::ComPtr<ID3D12Resource> m_indexBuffer;
    std::unique_ptr<ConstantBuffer>        m_objectCB;
    
    D3D12_VERTEX_BUFFER_VIEW m_vertexBufferView = {};
    D3D12_INDEX_BUFFER_VIEW m_indexBufferView = {};
    
    bool m_needsConstantBufferUpdate = true;

    std::string m_materialName = "default";
	D3D_PRIMITIVE_TOPOLOGY m_topologyType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
    
    void UpdateWorldMatrix();
    void CreateBuffers(ID3D12Device* device);
};
}
