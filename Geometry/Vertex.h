#pragma once
#include <DirectXMath.h>

namespace Lunar
{
struct Vertex
{
    DirectX::XMFLOAT3 pos;
    DirectX::XMFLOAT4 color;
    DirectX::XMFLOAT2 texCoord;
    DirectX::XMFLOAT3 normal;
};
}
