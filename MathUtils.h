#pragma once

namespace Lunar
{
class MathUtils
{
public:
    // Create a quaternion from an axis and an angle. The axis must be normalized.
    static XMFLOAT4 CreateFromAxisAngle(float ax, float ay, float az, float angle);
    static XMMATRIX CreateRotationMatrixFromQuaternion(const XMFLOAT4& quaternion);
    static XMFLOAT4 CreateRotationQuatFromRollPitchYaw(float roll, float pitch, float yaw);
};
} // namespace Lunar
