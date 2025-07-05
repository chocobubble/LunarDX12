#pragma once

#include <DirectXMath.h>

namespace Lunar
{
struct Transform
{
	DirectX::XMFLOAT3 Location = {0.0f, 0.0f, 0.0f};
	DirectX::XMFLOAT3 Rotation = {0.0f, 0.0f, 0.0f};
	DirectX::XMFLOAT3 Scale = {1.0f, 1.0f, 1.0f};
};
} // namespace Lunar
