#pragma once
#include <DirectXMath.h>

namespace Lunar
{
struct Vertex
{
    XMFLOAT3 pos;
    XMFLOAT4 color;
    XMFLOAT2 texCoord;
    XMFLOAT3 normal;
};
}
