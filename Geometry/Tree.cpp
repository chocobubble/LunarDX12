#include "Tree.h"

namespace Lunar
{

void Tree::CreateGeometry()
{
	m_vertices = {
		{{ -10.0f, 0.0f, 10.0f }},
		{{ -5.0f, 0.0f, 10.0f }},
		{{ 5.0f, 0.0f, 10.0f }},
		{{ 10.0f, 0.0f, 10.0f }} 
	};
}

void Tree::Draw(ID3D12GraphicsCommandList* commandList)
{
	if (m_needsConstantBufferUpdate) UpdateObjectConstants();
	BindObjectConstants(commandList);
	commandList->IASetVertexBuffers(0, 1, &m_vertexBufferView);
	commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_POINTLIST);
	commandList->DrawInstanced(1, 4, 0, 0);
}
} // namespace Lunar