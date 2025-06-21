#pragma once
#include <vector>
#include <d3d12.h>
#include <DirectXMath.h>
#include <wrl/client.h>

#include "Vertex.h"  

// Forward declaration for Transform
namespace Lunar { struct Transform; }

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
    
    // Unreal Engine style Transform methods
    void SetTransform(const Transform& transform);
    void SetLocation(const DirectX::XMFLOAT3& location);
    void SetRotation(const DirectX::XMFLOAT3& rotation);
    void SetScale(const DirectX::XMFLOAT3& scale);
    void SetColor(const DirectX::XMFLOAT4& color);
    
    // Getters
    const DirectX::XMFLOAT4X4& GetWorldMatrix() const { return m_world; }
    const Transform& GetTransform() const { return m_transform; }
    const DirectX::XMFLOAT3& GetLocation() const { return m_transform.Location; }
    const DirectX::XMFLOAT3& GetRotation() const { return m_transform.Rotation; }
    const DirectX::XMFLOAT3& GetScale() const { return m_transform.Scale; }
    const DirectX::XMFLOAT4& GetColor() const { return m_color; }
    
    // Backward compatibility (deprecated - use GetLocation instead)
    const DirectX::XMFLOAT3& GetPosition() const { return m_transform.Location; }
    void SetPosition(const DirectX::XMFLOAT3& position) { SetLocation(position); }
    
protected:
    std::vector<Vertex> m_vertices;
    std::vector<uint16_t> m_indices;
    
    DirectX::XMFLOAT4X4 m_world = {};  
    Transform m_transform;  // Unified transform data
    DirectX::XMFLOAT4 m_color = {1.0f, 1.0f, 1.0f, 1.0f};  
    
    Microsoft::WRL::ComPtr<ID3D12Resource> m_vertexBuffer;
    Microsoft::WRL::ComPtr<ID3D12Resource> m_indexBuffer;
    
    D3D12_VERTEX_BUFFER_VIEW m_vertexBufferView = {};
    D3D12_INDEX_BUFFER_VIEW m_indexBufferView = {};
    
    void UpdateWorldMatrix();
    void CreateBuffers(ID3D12Device* device);
};
}
