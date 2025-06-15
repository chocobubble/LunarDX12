#include "Camera.h"
#include <DirectXMath.h>
#include <algorithm>

#include "Logger.h"
#include "MathUtils.h"

using namespace DirectX;

namespace Lunar
{

const XMFLOAT4X4& Camera::GetViewMatrix()
{
    if (m_viewDirty)
    {
        // The view matrix transforms objects from world coordinates to the camera's coordinate system.
        // 1. Get the world matrix of the camera.(W = RT)
        //   - The camera doesn't have a 'size', so the scaling would not be considered. 
        // 2. Reverse the world matrix. (V = (RT)^{-1}) 
        XMMATRIX cameraRotaion = Lunar::MathUtils::CreateRotationMatrixFromQuaternion(m_qRotation); 
        XMMATRIX transposedCameraRotation = XMMatrixTranspose(cameraRotaion);
        XMVECTOR reversedCameraPosition = XMVectorNegate(XMLoadFloat3(&m_position));
        XMMATRIX cameraTranslation = XMMatrixTranslationFromVector(reversedCameraPosition); 
        XMMATRIX viewMatrix = XMMatrixMultiply(transposedCameraRotation, cameraTranslation); 
        XMStoreFloat4x4(&m_viewMatrix, viewMatrix);
        m_viewDirty = false;
    }
    return m_viewMatrix;
}

const XMFLOAT4X4& Camera::GetProjMatrix()
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
            0.0f, 0.0f, -m_nearZ * m_farZ / distFarToScreen, 0.0f
        };
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
	// LOG_FUNCTION_ENTRY();
    m_qRotation = Lunar::MathUtils::CreateRotationQuatFromRollPitchYaw(0.0f, m_pitch, m_yaw);
    XMVECTOR forward = XMVector3Rotate(XMVectorSet(0, 0, 1, 0), XMLoadFloat4(&m_qRotation));
    XMVECTOR right = XMVector3Normalize(XMVector3Cross(forward, XMLoadFloat3(&m_worldUpDir)));
    XMVECTOR up = XMVector3Normalize(XMVector3Cross(right, forward));
    XMStoreFloat3(&m_viewDir, forward);
    XMStoreFloat3(&m_upDir, up);
    XMStoreFloat3(&m_rightDir, right);
    m_viewDirty = true;
}

void Camera::UpdateRotationQuatFromMouse(float dx, float dy)
{
    m_yaw = -dy * m_sensitivity;
    m_pitch = dx * m_sensitivity;
    m_yaw = std::clamp(m_yaw, -XM_PIDIV2 + 0.01f, XM_PIDIV2 - 0.01f); // -1/2pi ~ 1/2pi
    UpdateViewDir();
}

} // namespace Lunar