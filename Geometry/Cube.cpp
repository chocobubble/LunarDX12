#include "Cube.h"
#include "../Logger.h"

using namespace std;
using namespace DirectX;

namespace Lunar
{

void Cube::CreateGeometry()
{
    CreateCubeVertices();
    CreateCubeIndices();
    
    // use smooth normals
    vector<XMFLOAT3> normalSum(m_vertices.size(), {0, 0, 0});
    for (size_t i = 0; i < m_indices.size(); i += 3)
    {
        uint16_t index0 = m_indices[i];
        uint16_t index1 = m_indices[i + 1];
        uint16_t index2 = m_indices[i + 2];

        XMVECTOR p0 = XMLoadFloat3(&m_vertices[index0].pos);
        XMVECTOR p1 = XMLoadFloat3(&m_vertices[index1].pos);
        XMVECTOR p2 = XMLoadFloat3(&m_vertices[index2].pos);

        XMVECTOR e1 = p1 - p0;
        XMVECTOR e2 = p2 - p0;

        XMVECTOR faceNormal = XMVector3Normalize(XMVector3Cross(e1, e2));

        XMFLOAT3 fn;
        XMStoreFloat3(&fn, faceNormal);
        
        auto accumulate = [&normalSum, &fn](uint16_t idx) {
            normalSum[idx].x += fn.x;
            normalSum[idx].y += fn.y;
            normalSum[idx].z += fn.z;
        };
        
        accumulate(index0);
        accumulate(index1);
        accumulate(index2);
    }
    
    for (size_t i = 0; i < m_vertices.size(); ++i)
    {
        XMVECTOR normal = XMVector3Normalize(XMLoadFloat3(&normalSum[i]));
        XMStoreFloat3(&m_vertices[i].normal, normal);
    }
}

void Cube::CreateCubeVertices()
{
    m_vertices = {
        { { -0.5f, -0.5f, -0.5f }, { 0.0f, 0.0f, 0.0f, 1.0f }, { 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f } }, // 0
        { { -0.5f, +0.5f, -0.5f }, { 0.0f, 1.0f, 0.0f, 1.0f }, { 0.0f, 1.0f }, { 0.0f, 0.0f, 0.0f } }, // 1
        { { +0.5f, +0.5f, -0.5f }, { 1.0f, 1.0f, 0.0f, 1.0f }, { 1.0f, 1.0f }, { 0.0f, 0.0f, 0.0f } }, // 2
        { { +0.5f, -0.5f, -0.5f }, { 1.0f, 0.0f, 0.0f, 1.0f }, { 1.0f, 0.0f }, { 0.0f, 0.0f, 0.0f } }, // 3
        { { -0.5f, -0.5f, +0.5f }, { 0.0f, 0.0f, 1.0f, 1.0f }, { 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f } }, // 4
        { { -0.5f, +0.5f, +0.5f }, { 0.0f, 1.0f, 1.0f, 1.0f }, { 0.0f, 1.0f }, { 0.0f, 0.0f, 0.0f } }, // 5
        { { +0.5f, +0.5f, +0.5f }, { 1.0f, 1.0f, 1.0f, 1.0f }, { 1.0f, 1.0f }, { 0.0f, 0.0f, 0.0f } }, // 6
        { { +0.5f, -0.5f, +0.5f }, { 1.0f, 0.0f, 1.0f, 1.0f }, { 1.0f, 0.0f }, { 0.0f, 0.0f, 0.0f } }, // 7
    };
}

void Cube::CreateCubeIndices()
{
    m_indices = {
        // front face
        0, 1, 2,  0, 2, 3,
        // back face
        4, 7, 6,  4, 6, 5,
        // top face
        1, 5, 6,  1, 6, 2,
        // bottom face
        4, 0, 3,  4, 3, 7,
        // left face
        4, 5, 1,  4, 1, 0,
        // right face
        3, 2, 6,  3, 6, 7
    };
}
}
