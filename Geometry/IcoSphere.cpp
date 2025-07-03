#include "IcoSphere.h"

using namespace std;
using namespace DirectX;

namespace Lunar 
{
void IcoSphere::CreateGeometry()
{
    float goldenRatio = 1.618;
    float inverseNormal = 1.0f / sqrtf(goldenRatio * goldenRatio + 1.0f);  

    goldenRatio *= inverseNormal;

    m_vertices = {
        {{-inverseNormal, goldenRatio, 0}, {1,1,1,1}, {0,0}, {0,0,0}},   // 0
        {{ inverseNormal, goldenRatio, 0}, {1,1,1,1}, {0,0}, {0,0,0}},   // 1
        {{-inverseNormal,-goldenRatio, 0}, {1,1,1,1}, {0,0}, {0,0,0}},   // 2
        {{ inverseNormal,-goldenRatio, 0}, {1,1,1,1}, {0,0}, {0,0,0}},   // 3
        {{ 0,-inverseNormal, goldenRatio}, {1,1,1,1}, {0,0}, {0,0,0}},   // 4
        {{ 0, inverseNormal, goldenRatio}, {1,1,1,1}, {0,0}, {0,0,0}},   // 5
        {{ 0,-inverseNormal,-goldenRatio}, {1,1,1,1}, {0,0}, {0,0,0}},   // 6
        {{ 0, inverseNormal,-goldenRatio}, {1,1,1,1}, {0,0}, {0,0,0}},   // 7
        {{ goldenRatio, 0,-inverseNormal}, {1,1,1,1}, {0,0}, {0,0,0}},   // 8
        {{ goldenRatio, 0, inverseNormal}, {1,1,1,1}, {0,0}, {0,0,0}},   // 9
        {{-goldenRatio, 0,-inverseNormal}, {1,1,1,1}, {0,0}, {0,0,0}},   // 10
        {{-goldenRatio, 0, inverseNormal}, {1,1,1,1}, {0,0}, {0,0,0}}    // 11
    };

    m_indices = {
        // top 5 
        0, 11, 5,    0, 5, 1,     0, 1, 7,     0, 7, 10,    0, 10, 11,
        // middle-top 5
        1, 5, 9,     5, 11, 4,    11, 10, 2,   10, 7, 6,    7, 1, 8,
        // middle-bottom 5
        3, 9, 4,     3, 4, 2,     3, 2, 6,     3, 6, 8,     3, 8, 9,
        // bottom 5
        4, 9, 5,     2, 4, 11,    6, 2, 10,    8, 6, 7,     9, 8, 1
    };

    for (int i = 0; i < m_subdivisionLevel; ++i)
    {
        Subdivide();
    }

    CalculateNormals();
    CalculateTexCoords();
    CalculateColors();
	FixSeamVertices();
	ComputeTangents();
}

uint16_t IcoSphere::GetMiddlePoint(UINT p1, UINT p2)
{
    if (p1 > p2) std::swap(p1, p2);
    auto key = std::make_pair(p1, p2);
    auto it = m_middlePointCache.find(key);
    if (it != m_middlePointCache.end()) return it->second;

    XMVECTOR pos1 = XMLoadFloat3(&m_vertices[p1].pos); 
    XMVECTOR pos2 = XMLoadFloat3(&m_vertices[p2].pos); 
    XMVECTOR middle = XMVectorScale(XMVectorAdd(pos1, pos2), 0.5f);

    // normalize for unit sphere
    middle = XMVector3Normalize(middle);

    Vertex middleVertex;
    XMStoreFloat3(&middleVertex.pos, middle);
    middleVertex.color = {1, 1, 1, 1};
    middleVertex.normal = {0, 0, 0};
    middleVertex.texCoord = {0, 0};

    uint16_t newIndex = static_cast<uint16_t>(m_vertices.size());
    m_vertices.push_back(middleVertex);
    m_middlePointCache[key] = newIndex;

    return newIndex;
}

void IcoSphere::Subdivide()
{
    std::vector<uint16_t> newIndices;

    for (size_t i = 0; i < m_indices.size(); i += 3) 
    {
        uint16_t index1 = m_indices[i];
        uint16_t index2 = m_indices[i + 1];
        uint16_t index3 = m_indices[i + 2];
        uint16_t index4 = GetMiddlePoint(index1, index2);
        uint16_t index5 = GetMiddlePoint(index2, index3);
        uint16_t index6 = GetMiddlePoint(index3, index1);
        
        newIndices.insert(newIndices.end(), {index1, index4, index6});
        newIndices.insert(newIndices.end(), {index4, index2, index5});
        newIndices.insert(newIndices.end(), {index6, index5, index3});
        newIndices.insert(newIndices.end(), {index4, index5, index6});
    }

    m_indices = move(newIndices);
    m_middlePointCache.clear();
}

void IcoSphere::CalculateNormals()
{
    // normal vector is same as position vector
    // because the radius of a sphere is 1
    for (auto& vertex : m_vertices)
    {
        vertex.normal = vertex.pos;
    }
}

void IcoSphere::CalculateTexCoords()
{
    for (auto& vertex : m_vertices)
    {
        XMVECTOR pos = XMLoadFloat3(&vertex.pos);
        float x = XMVectorGetX(pos);
        float y = XMVectorGetY(pos);
        float z = XMVectorGetZ(pos);

        float u = atan2f(x, z) / (2.0f * XM_PI) + 0.5f;
        float v = asinf(y) / XM_PI + 0.5f;

        vertex.texCoord = {u, v};
    }
}

void IcoSphere::CalculateColors()
{
    // for now, just set white color to all vertices
    for (auto& vertex : m_vertices)
    {
        vertex.color = {1, 1, 1, 1};
    }
}

void IcoSphere::FixSeamVertices()
{
	// for avoiding infinite loop
	vector<Vertex> newVertices = m_vertices;
	vector<uint16_t> newIndices;
	
	// to fix texture seam issue
	for (size_t i = 0; i < m_indices.size(); i+= 3)
	{
		uint16_t index0 = m_indices[i];
		uint16_t index1 = m_indices[i + 1];
		uint16_t index2 = m_indices[i + 2];

		float u0 = m_vertices[index0].texCoord.x;
		float u1 = m_vertices[index1].texCoord.x;
		float u2 = m_vertices[index2].texCoord.x;


		// Fix horizontal seam first
		if (abs(u0 - u1) > 0.5f || abs(u1 - u2) > 0.5f || abs(u2 - u0) > 0.5f)
		{
			if (u0 < 0.5f)
			{
				newVertices.push_back(m_vertices[index0]);
				newVertices.back().texCoord.x += 1.0f;
				index0 = newVertices.size() - 1;
				u0 += 1.0f;
			}
			if (u1 < 0.5f)
			{
				newVertices.push_back(m_vertices[index1]);
				newVertices.back().texCoord.x += 1.0f;
				index1 = newVertices.size() - 1;
				u1 += 1.0f;
			}
			if (u2 < 0.5f)
			{
				newVertices.push_back(m_vertices[index2]);
				newVertices.back().texCoord.x += 1.0f;
				index2 = newVertices.size() - 1;
				u2 += 1.0f;
			}
		}
		newIndices.push_back(index0);
		newIndices.push_back(index1);
		newIndices.push_back(index2);
	}

	m_vertices = move(newVertices);
	m_indices = move(newIndices);
}
} // namespace Lunar
