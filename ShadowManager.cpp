#include "ShadowManager.h"

#include <d3d12.h>

#include "Utils.h"

using namespace Microsoft::WRL;

namespace Lunar
{

void ShadowManager::Initialize(ID3D12Device* device)
{
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
	dsvDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
	dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
	m_dsvHandle = dsvHeap->GetCPUDescriptorHandleForHeapStart();
	m_dsvHandle.ptr += device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);
	device->CreateDepthStencilView(
		m_shadowTexture.Get(),
		&dsvDesc,
		m_dsvHandle);
}

void ShadowManager::CreateSRV(ID3D12Device* device, ID3D12DescriptorHeap* srvHeap)
{
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Format = DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Texture2D.MipLevels = 1;
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	D3D12_CPU_DESCRIPTOR_HANDLE srvHandle = srvHeap->GetCPUDescriptorHandleForHeapStart();
	srvHandle.ptr += device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	device->CreateShaderResourceView(
		m_shadowTexture.Get(),
		&srvDesc,
		srvHandle);
}
} // namespace Lunar