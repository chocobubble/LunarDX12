#pragma once
#include <vector>
#include <d3d12.h>
#include <DirectXMath.h>
#include <wrl/client.h>

using namespace DirectX;

namespace Lunar
{
	struct Vertex;
}

namespace Lunar
{
class Cube
{
public:
	Cube();
	~Cube() = default;
	
	void Initialize(ID3D12Device* device, ID3D12GraphicsCommandList* commandList);
	void Draw(ID3D12GraphicsCommandList* commandList);
	
	void SetPosition(const XMFLOAT3& position);
	void SetRotation(const XMFLOAT3& rotation);
	void SetScale(const float scale);
	void SetColor(const XMFLOAT4& color);
	
	const XMFLOAT4X4& GetWorldMatrix() const { return m_world; }
	const XMFLOAT3& GetPosition() const { return m_position; }
	const XMFLOAT3& GetRotation() const { return m_rotation; }
	float GetScale() const { return m_scale; }
	const XMFLOAT4& GetColor() const { return m_color; }
	
private:
	std::vector<Lunar::Vertex> m_vertices;
	std::vector<uint16_t> m_indices;

	XMFLOAT4X4 m_world;
	XMFLOAT4 m_color;
	float m_scale;
	XMFLOAT3 m_position;
	XMFLOAT3 m_rotation;

	Microsoft::WRL::ComPtr<ID3D12Resource> m_vertexBuffer;
	Microsoft::WRL::ComPtr<ID3D12Resource> m_indexBuffer;
	D3D12_VERTEX_BUFFER_VIEW               m_vertexBufferView;
	D3D12_INDEX_BUFFER_VIEW                m_indexBufferView;

	void CreateGeometry();
	void UpdateWorldMatrix();
};
} // namespace Lunar