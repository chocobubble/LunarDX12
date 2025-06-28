#pragma once
#include <DirectXMath.h>

namespace Lunar
{
class MathUtils
{
public:
    // Create a quaternion from an axis and an angle. The axis must be normalized.
    static DirectX::XMFLOAT4 CreateFromAxisAngle(float ax, float ay, float az, float angle);
    static DirectX::XMMATRIX CreateRotationMatrixFromQuaternion(const DirectX::XMFLOAT4& quaternion);
    static DirectX::XMFLOAT4 CreateRotationQuatFromRollPitchYaw(float roll, float pitch, float yaw);
	static DirectX::XMMATRIX MakeReflectionMatrix(float a, float b, float c, float d);
};
} // namespace Lunar
