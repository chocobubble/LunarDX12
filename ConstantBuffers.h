#pragma once
#include <DirectXMath.h>

using namespace DirectX;

namespace Lunar
{

struct BasicConstants
{
	XMFLOAT4X4 model;
	XMFLOAT4X4 view;
	XMFLOAT4X4 projection;
	XMFLOAT3 eyePos;
	float dummy;
};
	
} // namespace Lunar
