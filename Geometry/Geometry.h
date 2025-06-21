#pragma once
#include <vector>
#include <d3d12.h>
#include <DirectXMath.h>
#include <wrl/client.h>

#include "Vertex.h"  

struct Transform;
struct ObjectConstants;
class ConstantBuffer;

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
    
    void SetTransform(const Transform& transform);
    void SetLocation(const DirectX::XMFLOAT3& location);
    void SetRotation(const DirectX::XMFLOAT3& rotation);
    void SetScale(const DirectX::XMFLOAT3& scale);
    void SetColor(const DirectX::XMFLOAT4& color);  // TODO: delete
    void SetMaterialName(const std::string& materialName);
    
    const DirectX::XMFLOAT4X4& GetWorldMatrix() const { return m_world; }
    const Transform& GetTransform() const { return m_transform; }
    const DirectX::XMFLOAT3& GetLocation() const { return m_transform.Location; }
    const DirectX::XMFLOAT3& GetRotation() const { return m_transform.Rotation; }
    const DirectX::XMFLOAT3& GetScale() const { return m_transform.Scale; }
    const DirectX::XMFLOAT4& GetColor() const { return m_color; }
    const std::string& GetMaterialName() const { return m_materialName; }
    
    void UpdateObjectConstants();
    void BindObjectConstants(ID3D12GraphicsCommandList* commandList, UINT rootParameterIndex);
    
protected:
    std::vector<Vertex> m_vertices;
    std::vector<uint16_t> m_indices;
    
    DirectX::XMFLOAT4X4 m_world = {};  
    Transform m_transform;  
    DirectX::XMFLOAT4 m_color = {1.0f, 1.0f, 1.0f, 1.0f};  // TODO: delete 
    
    Microsoft::WRL::ComPtr<ID3D12Resource> m_vertexBuffer;
    Microsoft::WRL::ComPtr<ID3D12Resource> m_indexBuffer;
    std::unique_ptr<ConstantBuffer> m_objectCB;
    
    D3D12_VERTEX_BUFFER_VIEW m_vertexBufferView = {};
    D3D12_INDEX_BUFFER_VIEW m_indexBufferView = {};
    
    bool m_needsConstantBufferUpdate = true;

    std::string m_materialName = "default";
    
    void UpdateWorldMatrix();
    void CreateBuffers(ID3D12Device* device);
};
}
