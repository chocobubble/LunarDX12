#include "Plane.h"

#include "../MathUtils.h"

using namespace DirectX;

namespace Lunar
{
Plane::Plane(float width, float height, int widthSegments, int heightSegments)
    : m_width(width), m_height(height), m_widthSegments(widthSegments), m_heightSegments(heightSegments)
{
	m_planeEquation = XMFLOAT4(0.0f, 1.0f, 0.0f, 0.0f);
}

void Plane::CreateGeometry()
{
    CreatePlaneVertices();
    CreatePlaneIndices();
}

XMFLOAT4 Plane::GetPlaneEquation()
{
	CalculatePlaneEquation();
	return m_planeEquation;
}

void Plane::CreatePlaneVertices()
{
    m_vertices.clear();
    
    float halfWidth = m_width * 0.5f;
    float halfHeight = m_height * 0.5f;
    
    float dx = m_width / m_widthSegments;
    float dz = m_height / m_heightSegments;
    
    float du = 1.0f / m_widthSegments;
    float dv = 1.0f / m_heightSegments;

    for (int i = 0; i <= m_heightSegments; ++i)
    {
        float z = halfHeight - i * dz;
        for (int j = 0; j <= m_widthSegments; ++j)
        {
            float x = -halfWidth + j * dx;

            Vertex vertex;
            vertex.pos = XMFLOAT3(x, 0.0f, z);
            vertex.normal = XMFLOAT3(0.0f, 1.0f, 0.0f);  
            vertex.texCoord = XMFLOAT2(j * du, i * dv);
            
            // chess board pattern
            bool isEven = ((i + j) % 2) == 0;
            vertex.color = isEven ? 
                XMFLOAT4(0.8f, 0.8f, 0.8f, 1.0f) : 
                XMFLOAT4(0.4f, 0.4f, 0.4f, 1.0f);

            m_vertices.push_back(vertex);
        }
    }
}

void Plane::CreatePlaneIndices()
{
    m_indices.clear();
    
    int verticesPerRow = m_widthSegments + 1;

    for (int i = 0; i < m_heightSegments; ++i)
    {
        for (int j = 0; j < m_widthSegments; ++j)
        {
            int topLeft = i * verticesPerRow + j;
            int topRight = topLeft + 1;
            int bottomLeft = (i + 1) * verticesPerRow + j;
            int bottomRight = bottomLeft + 1;

            m_indices.push_back(bottomLeft);
            m_indices.push_back(topLeft);
            m_indices.push_back(topRight);

            m_indices.push_back(topRight);
            m_indices.push_back(bottomRight);
            m_indices.push_back(bottomLeft);
        }
    }
}

void Plane::CalculatePlaneEquation()
{
	XMMATRIX worldMatrix = XMLoadFloat4x4(&m_objectConstants.World);
	XMMATRIX invTransposeMatrix = XMMatrixTranspose(XMMatrixInverse(nullptr, worldMatrix));
	XMVECTOR localNormal = XMVectorSet(m_planeEquation.x, m_planeEquation.y, m_planeEquation.z, 0);
	float localDistance = m_planeEquation.w;
	XMVECTOR worldNormal = XMVector3TransformNormal(localNormal, invTransposeMatrix);
	worldNormal = XMVector3Normalize(worldNormal);
	XMVECTOR localPoint = XMVectorSet(0, localDistance, 0, 1);	 // point on the plane
	XMVECTOR worldPoint = XMVector3TransformCoord(localPoint, worldMatrix);
	float worldDistance = -XMVectorGetX(XMVector3Dot(worldNormal, worldPoint));
	XMFLOAT3 wn;
	XMStoreFloat3(&wn, worldNormal);
	m_planeEquation = XMFLOAT4(wn.x, wn.y, wn.z, worldDistance);
}
}
