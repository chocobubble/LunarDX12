#pragma once

namespace Lunar
{
	
class Camera
{
public:
	const XMFLOAT4X4& GetViewMatrix();
    const XMFLOAT4X4& GetProjMatrix();
    XMFLOAT3 GetPos();
    void UpdateViewDir();
    void UpdateRotationQuatFromMouse(float dx, float dy);

private:
    XMFLOAT3 m_position = {0.0f, 0.0f, -0.5f}; 
    XMFLOAT3 m_worldUpDir = {0.0f, 1.0f, 0.0f};
    XMFLOAT3 m_viewDir = {0.0f, 0.0f, 1.0f};
    XMFLOAT3 m_upDir = {0.0f, 1.0f, 0.0f};
    XMFLOAT3 m_rightDir = {1.0f, 0.0f, 0.0f};
    XMFLOAT4 m_qRotation = {0.0f, 0.0f, 0.0f, 1.0f};  // identity
    XMFLOAT4X4 m_projMatrix = {};
    XMFLOAT4X4 m_viewMatrix = {};

    float m_nearZ = LunarConstants::NEAR_PLANE;
    float m_farZ = LunarConstants::FAR_PLANE;
    float m_projFovAngleY = LunarConstants::FOV_ANGLE;
    float m_aspect = LunarConstants::ASPECT_RATIO;
    float m_yaw = 0.0f;
    float m_pitch = 0.0f;
    float m_sensitivity = LunarConstants::CAMERA_SENSITIVITY; 

    bool m_projDirty = true;
    bool m_viewDirty = true;
};
} // namespace Lunar
