#include "ShadowManager.h"

#include <d3d12.h>

#include "DescriptorAllocator.h"
#include "Utils/Utils.h"
#include "Utils/MathUtils.h"

using namespace Microsoft::WRL;
using namespace DirectX;
using namespace std;

namespace Lunar
{

void ShadowManager::Initialize(ID3D12Device* device)
{
	m_shadowCB = make_unique<ConstantBuffer>(device, sizeof(BasicConstants));
	m_viewport = { 0.0f, 0.0f, static_cast<float>(m_shadowMapWidth), static_cast<float>(m_shadowMapHeight), 0.0f, 1.0f };
	m_scissorRect = { 0, 0, static_cast<int>(m_shadowMapWidth), static_cast<int>(m_shadowMapHeight)};
	CreateShadowMapTexture(device);
}

void ShadowManager::CreateShadowMapTexture(ID3D12Device* device)
{
	D3D12_RESOURCE_DESC texDesc;
	texDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	texDesc.Alignment = 0;
	texDesc.Width = m_shadowMapWidth;
	texDesc.Height = m_shadowMapHeight;
	texDesc.DepthOrArraySize = 1;
	texDesc.MipLevels = 1;
	texDesc.Format = DXGI_FORMAT_R24G8_TYPELESS;
	texDesc.SampleDesc.Count = 1;
	texDesc.SampleDesc.Quality = 0;
	texDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
	texDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
	
	D3D12_HEAP_PROPERTIES defaultHeapProperties = {};
	defaultHeapProperties.Type = D3D12_HEAP_TYPE_DEFAULT;
	defaultHeapProperties.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
	defaultHeapProperties.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
	defaultHeapProperties.CreationNodeMask = 1;
	defaultHeapProperties.VisibleNodeMask = 1;

	D3D12_CLEAR_VALUE clearValue = {};
	clearValue.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
	clearValue.DepthStencil.Depth = 1.0f;
	clearValue.DepthStencil.Stencil = 0.0f;
	
	THROW_IF_FAILED(device->CreateCommittedResource(
		&defaultHeapProperties,
		D3D12_HEAP_FLAG_NONE,
		&texDesc,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		&clearValue,
		IID_PPV_ARGS(m_shadowTexture.GetAddressOf())))
}

void ShadowManager::CreateDSV(ID3D12Device* device, ID3D12DescriptorHeap* dsvHeap)
{
	D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc = {};
	dsvDesc.Texture2D.MipSlice = 0;
	dsvDesc.Flags = D3D12_DSV_FLAG_NONE;
	dsvDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
	dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
	m_dsvHandle = dsvHeap->GetCPUDescriptorHandleForHeapStart();
	m_dsvHandle.ptr += device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);
	device->CreateDepthStencilView(
		m_shadowTexture.Get(),
		&dsvDesc,
		m_dsvHandle);
}

void ShadowManager::CreateSRV(ID3D12Device* device, DescriptorAllocator* descriptorAllocator)
{
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Format = DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Texture2D.MipLevels = 1;
	srvDesc.Texture2D.MostDetailedMip = 0;
	srvDesc.Texture2D.PlaneSlice = 0;
	srvDesc.Texture2D.ResourceMinLODClamp = 0;
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;

	// descriptorAllocator->AllocateDescriptor("ShadowMap");
	descriptorAllocator->CreateSRV(LunarConstants::RangeType::SHADOW, "ShadowMap", m_shadowTexture.Get(), &srvDesc);
}

void ShadowManager::UpdateShadowCB(const BasicConstants& basicConstants)
{
	m_basicConstants = {};
	// const float* dir = LunarConstants::LIGHT_INFO[0].direction;
	XMVECTOR lightDir = XMVectorSet(m_posX, m_posY, m_posZ, 0.0f);
	XMVECTOR lightPos = -2.0f * m_sceneRadius * lightDir;

	// TODO: Refactor
	// Calculate view matrix
	XMVECTOR forward = XMVector3Normalize(lightDir);
	XMVECTOR worldUp = XMVectorSet(0, 1, 0, 0);
	XMVECTOR right = XMVector3Cross(worldUp, forward);
	XMVECTOR up = XMVector3Cross(forward, right);

	// inversed rotation matrix (transposed)
	XMMATRIX inversedRotation = XMMatrixSet(
		XMVectorGetX(right), XMVectorGetY(right), XMVectorGetZ(right), 0,
		XMVectorGetX(up), XMVectorGetY(up), XMVectorGetZ(up), 0,
		XMVectorGetX(forward), XMVectorGetY(forward), XMVectorGetZ(forward), 0,
		0, 0, 0, 1
		);
	
	XMMATRIX inversedTranslation = XMMatrixTranslation(-XMVectorGetX(lightPos), -XMVectorGetY(lightPos), -XMVectorGetZ(lightPos));
	
	XMMATRIX viewMatrix = inversedRotation * inversedTranslation;

	XMFLOAT3 sceneCenterLS; // scene center in light space
	XMStoreFloat3(&sceneCenterLS, XMVector3TransformCoord(XMLoadFloat3(&m_sceneCenterPosition), viewMatrix));

	// TODO: use calculated min/max values of objects
	float l = sceneCenterLS.x - m_sceneRadius;
	float b = sceneCenterLS.y - m_sceneRadius;
	float n = sceneCenterLS.z - m_sceneRadius;
	float r = sceneCenterLS.x + m_sceneRadius;
	float t = sceneCenterLS.y + m_sceneRadius;
	float f = sceneCenterLS.z + m_sceneRadius;	
	
	XMMATRIX projectionMatrix = MathUtils::CreateOrthographicOffCenterLH(l, r, b, t, n, f);
	XMMATRIX ndcToTexture = MathUtils::CreateNDCToTextureTransform();
	
	XMStoreFloat4x4(&m_basicConstants.view, XMMatrixTranspose(viewMatrix));
	XMStoreFloat4x4(&m_basicConstants.projection, XMMatrixTranspose(projectionMatrix));
	XMStoreFloat4x4(&m_shadowTransform, XMMatrixTranspose(viewMatrix * projectionMatrix * ndcToTexture));

	m_shadowCB->CopyData(&m_basicConstants, sizeof(BasicConstants));
}
} // namespace Lunar