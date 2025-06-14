#include "Camera.h"

namespace Lunar
{

XMFLOAT4X4& Camera::GetViewMatrix() const
{
    if (m_viewDirty)
    {
        // The view matrix transforms objects from world coordinates to the camera's coordinate system.
        // 1. Get the world matrix of the camera.(W = RT)
        //   - The camera doesn't have a 'size', so the scaling would not be considered. 
        // 2. Reverse the world matrix. (V = (RT)^{-1}) 
        XMMATRIX cameraRotaion = Lunar::MathUtils::CreateRotationMatrixFromQuaternion(XMLoadFloat4(&m_qRotation)); 
        XMMATRIX transposedCameraRotation = XMMatrixTranspose(cameraRotaion);
        XMVECTOR reversedCameraPosition = XMVectorNegate(XMLoadFloat4(&m_position));
        XMMATRIX cameraTranslation = XMMatrixTranslationFromVector(reverseCameraPosition); 
        XMMATRIX viewMatrix = XMMatrixMultiply(transposedCameraRotation, cameraTranslation); 
        XMStoreFloat4x4(&m_viewMatrix, viewMatrix);
        m_viewDirty = false;
    }
    return m_viewMatrix;
}

XMFLOAT4X4& Camera::GetProjMatrix() const
{
    if (m_projDirty)
    {
        float distFarToScreen = m_farZ - m_nearZ;
        float yScale = 1.0f / tanf(m_projFovAngleY * 0.5f);
        float xScale = yScale / m_aspectRatio;
        m_projMatrix = {
            xScale, 0.0f, 0.0f, 0.0f,
            0.0f, yScale, 0.0f, 0.0f,
            0.0f, 0.0f, m_farZ / distFarToScreen, 1.0f,
            0.0f, 0.0f, m_nearZ * m_farZ / distFarToScreen, 0.0f
        }
        m_projDirty = false;
    }
    return m_projMatrix;
}

XMFLOAT3 Camera::GetPosition() const
{
    return m_position;
}

void Camera::UpdateViewDir()
{
    m_qRotation = Lunar::CreateRotationQuatFromRollPitchYaw(0.0f, m_pitch, m_yaw);
    XMVECTOR forward = XMVector3Rotate(XMVectorSet(0, 0, 1, 0), m_qRotation);
    XMVECTOR right = XMVector3Normalize(XMVector3Cross(forward, m_worldUpDir));
    XMVECTOR up = XMVector3Normalize(XMVector3Cross(right, forward));
    XMStoreFloat3(&m_viewDir, forward);
    XMStoreFloat3(&m_upDir, up);
    XMStoreFloat3(&m_rightDir, right);
    m_viewDirty = true;
}

void Camera::UpdateRotationQuatFromMouse(float dx, float dy)
{
    m_yaw = dx * m_sensitivity;
    m_pitch = dy * m_sensitivity;
    m_pitch = std::clamp(m_pitch, -XM_PIDIV2 + 0.01f, XM_PIDIV2 - 0.01f); // -1/2pi ~ 1/2pi
    UpdateViewDir();
}

} // namespace Lunar