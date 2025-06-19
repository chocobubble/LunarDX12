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
        XMMATRIX inversedCameraRotation = XMMatrixTranspose(cameraRotaion);
        XMMATRIX reversedCameraTranslationMatrix = XMMatrixTranslation(-m_position.x, -m_position.y, -m_position.z);
        XMMATRIX viewMatrix = XMMatrixMultiply(reversedCameraTranslationMatrix, inversedCameraRotation);
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
    XMVECTOR right = XMVector3Normalize(XMVector3Cross(XMLoadFloat3(&m_worldUpDir), forward));
    XMVECTOR up = XMVector3Normalize(XMVector3Cross(forward, right));
    XMStoreFloat3(&m_viewDir, forward);
    XMStoreFloat3(&m_upDir, up);
    XMStoreFloat3(&m_rightDir, right);
    m_viewDirty = true;
}

void Camera::UpdateRotationQuatFromMouse(float dx, float dy)
{
    m_yaw += dx * m_sensitivity;
    m_pitch += dy * m_sensitivity;
    m_pitch = std::clamp(m_pitch, -XM_PIDIV2 + 0.01f, XM_PIDIV2 - 0.01f); // -1/2pi ~ 1/2pi
	LOG_DEBUG("yaw : ", m_yaw, ", pitch : ", m_pitch);
    UpdateViewDir();
}

void Camera::UpdatePosition(float dx, float dy, float dz)
{
	XMVECTOR posVec = XMLoadFloat3(&m_position);
	XMVECTOR rightVec = XMLoadFloat3(&m_rightDir);
	XMVECTOR upVec = XMLoadFloat3(&m_upDir);
	XMVECTOR viewVec = XMLoadFloat3(&m_viewDir);
    
	posVec = XMVectorAdd(posVec, XMVectorScale(rightVec, m_movementSpeed * dx));
	posVec = XMVectorAdd(posVec, XMVectorScale(upVec, m_movementSpeed * dy));
	posVec = XMVectorAdd(posVec, XMVectorScale(viewVec, m_movementSpeed * dz));
    
	XMStoreFloat3(&m_position, posVec);
	
    m_viewDirty = true;
}

} // namespace Lunar