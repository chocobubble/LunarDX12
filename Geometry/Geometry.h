#pragma once
#include <vector>
#include <d3d12.h>
#include <DirectXMath.h>
#include <wrl/client.h>

#include "Vertex.h"  

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
    
    void SetPosition(const DirectX::XMFLOAT3& position);
    void SetRotation(const DirectX::XMFLOAT3& rotation);
    void SetScale(float scale);
    void SetColor(const DirectX::XMFLOAT4& color);
    
    // Getter
    const DirectX::XMFLOAT4X4& GetWorldMatrix() const { return m_world; }
    const DirectX::XMFLOAT3& GetPosition() const { return m_position; }
    const DirectX::XMFLOAT3& GetRotation() const { return m_rotation; }
    float GetScale() const { return m_scale; }
    const DirectX::XMFLOAT4& GetColor() const { return m_color; }
    
protected:
    std::vector<Vertex> m_vertices;
    std::vector<uint16_t> m_indices;
    
    DirectX::XMFLOAT4X4 m_world = {};  
    DirectX::XMFLOAT3 m_position = {0.0f, 0.0f, 0.0f};    
    DirectX::XMFLOAT3 m_rotation = {0.0f, 0.0f, 0.0f};    
    float m_scale = 1.0f;                           
    DirectX::XMFLOAT4 m_color = {1.0f, 1.0f, 1.0f, 1.0f};  
    
    Microsoft::WRL::ComPtr<ID3D12Resource> m_vertexBuffer;
    Microsoft::WRL::ComPtr<ID3D12Resource> m_indexBuffer;
    
    D3D12_VERTEX_BUFFER_VIEW m_vertexBufferView = {};
    D3D12_INDEX_BUFFER_VIEW m_indexBufferView = {};
    
    void UpdateWorldMatrix();
    void CreateBuffers(ID3D12Device* device);
};
}
