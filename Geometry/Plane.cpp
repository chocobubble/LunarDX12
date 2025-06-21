#include "Plane.h"

namespace Lunar
{
Plane::Plane(float width, float height, int widthSegments, int heightSegments)
    : m_width(width), m_height(height), m_widthSegments(widthSegments), m_heightSegments(heightSegments) { }

void Plane::CreateGeometry()
{
    CreatePlaneVertices();
    CreatePlaneIndices();
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

            m_indices.push_back(topLeft);
            m_indices.push_back(bottomLeft);
            m_indices.push_back(topRight);

            m_indices.push_back(topRight);
            m_indices.push_back(bottomLeft);
            m_indices.push_back(bottomRight);
        }
    }
}
}
