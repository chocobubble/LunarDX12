#pragma once
#include <DirectXMath.h>
#include "LunarConstants.h"

namespace Lunar
{
	
class Camera
{
public:
	Camera();
	~Camera() = default;
	const DirectX::XMFLOAT4X4& GetViewMatrix();
    const DirectX::XMFLOAT4X4& GetProjMatrix();
	DirectX::XMFLOAT3          GetPosition() const;
    void                       UpdateViewDir();
    void                       UpdateRotationQuatFromMouse(float dx, float dy);
    void UpdatePosition(float dx, float dy, float dz);

private:
	DirectX::XMFLOAT3   m_position = {-3.5f, 0.5f, -3.5f};
	DirectX::XMFLOAT3   m_worldUpDir = {0.0f, 1.0f, 0.0f};
	DirectX::XMFLOAT3   m_viewDir = {0.0f, 0.0f, 1.0f};
	DirectX::XMFLOAT3   m_upDir = {0.0f, 1.0f, 0.0f};
	DirectX::XMFLOAT3   m_rightDir = {1.0f, 0.0f, 0.0f};
	DirectX::XMFLOAT4   m_qRotation = {0.0f, 0.0f, 0.0f, 1.0f};  // identity
	DirectX::XMFLOAT4X4 m_projMatrix = {};
	DirectX::XMFLOAT4X4 m_viewMatrix = {};

    float m_nearZ = Lunar::Constants::NEAR_PLANE;
    float m_farZ = Lunar::Constants::FAR_PLANE;
    float m_projFovAngleY = Lunar::Constants::FOV_ANGLE;
    float m_aspectRatio = Lunar::Constants::ASPECT_RATIO;
    float m_yaw = 0.0f;
    float m_pitch = 0.0f;
    float m_sensitivity = Lunar::Constants::MOUSE_SENSITIVITY; 
    float m_movementSpeed = Lunar::Constants::CAMERA_MOVE_SPEED;

    bool m_projDirty = true;
    bool m_viewDirty = true;
};
} // namespace Lunar
