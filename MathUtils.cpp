#include "MathUtils.h"
#include <DirectXMath.h>

using namespace DirectX;
namespace Lunar 
{

XMFLOAT4 MathUtils::CreateFromAxisAngle(float ax, float ay, float az, float angle)
{
    float halfAngle = angle * 0.5f;
    float sinHalfAngle = sinf(halfAngle);
    float cosHalfAngle = cosf(halfAngle);

    return XMFLOAT4(ax * sinHalfAngle, ay * sinHalfAngle, az * sinHalfAngle, cosHalfAngle);
}

XMMATRIX MathUtils::CreateRotationMatrixFromQuaternion(const XMFLOAT4& quaternion)
{
    float xx = quaternion.x * quaternion.x;
    float yy = quaternion.y * quaternion.y;
    float zz = quaternion.z * quaternion.z;
    float xy = quaternion.x * quaternion.y;
    float xz = quaternion.x * quaternion.z;
    float yz = quaternion.y * quaternion.z;
    float wx = quaternion.w * quaternion.x;
    float wy = quaternion.w * quaternion.y;
    float wz = quaternion.w * quaternion.z;

    return XMMATRIX(
        1.0f - 2.0f * (yy + zz), 2.0f * (xy + wz), 2.0f * (xz - wy), 0.0f,
        2.0f * (xy - wz), 1.0f - 2.0f * (xx + zz), 2.0f * (yz + wx), 0.0f,
        2.0f * (xz + wy), 2.0f * (yz - wx), 1.0f - 2.0f * (xx + yy), 0.0f,
        0.0f, 0.0f, 0.0f, 1.0f);
}
	
XMFLOAT4 MathUtils::CreateRotationQuatFromRollPitchYaw(float roll, float pitch, float yaw)
{
	float halfRoll = roll * 0.5f;
	float halfPitch = pitch * 0.5f;
	float halfYaw = yaw * 0.5f;

	float sinRoll = sinf(halfRoll);
	float cosRoll = cosf(halfRoll);
	float sinPitch = sinf(halfPitch);
	float cosPitch = cosf(halfPitch);
	float sinYaw = sinf(halfYaw);
	float cosYaw = cosf(halfYaw);

	// roll -> pitch -> yaw
	return XMFLOAT4(
		cosRoll * sinPitch * cosYaw + sinRoll * cosPitch * sinYaw,
		cosRoll * cosPitch * sinYaw - sinRoll * sinPitch * cosYaw,
		sinRoll * cosPitch * cosYaw - cosRoll * sinPitch * sinYaw,
		cosRoll * cosPitch * cosYaw + sinRoll * sinPitch * sinYaw
	);
}

XMMATRIX MathUtils::MakeReflectionMatrix(float a, float b, float c, float d)
{
	return XMMATRIX(
		1-2*a*a, 	-2*a*b,	-2*a*c,	-2*a*d,
		-2*a*b,	   1-2*b*b,	-2*b*c,	-2*b*d,
		-2*a*c,	-2*b*c,	   1-2*c*c,	-2*c*d,
		0.0f,	0.0f, 0.0f, 1.0f
	);
}
} //namespace Lunar