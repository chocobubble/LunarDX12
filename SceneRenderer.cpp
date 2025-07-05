#include "SceneRenderer.h"
#include "Geometry/GeometryFactory.h"
#include "MaterialManager.h"
#include "LightingSystem.h"
#include "ConstantBuffers.h"
#include "LunarGUI.h"
#include "MathUtils.h"
#include "PipelineStateManager.h"
#include "ShadowManager.h"
#include "TextureManager.h"
#include "Utils.h"
#include "Geometry/Plane.h"
#include "ShadowViewModel.h"
#include "ParticleSystem.h"

using namespace DirectX;
using namespace std;

namespace Lunar
{

SceneRenderer::SceneRenderer()
{
	m_layeredGeometries.clear();
	m_geometriesByName.clear();
    m_materialManager = make_unique<MaterialManager>();
    m_sceneViewModel = make_unique<SceneViewModel>();
    m_lightingSystem = make_unique<LightingSystem>();
	m_shadowManager = make_unique<ShadowManager>();
	m_textureManager = make_unique<TextureManager>();
	m_shadowViewModel = make_unique<ShadowViewModel>();
    m_particleSystem = make_unique<ParticleSystem>();
}

SceneRenderer::~SceneRenderer() = default;

void SceneRenderer::InitializeScene(ID3D12Device* device, LunarGui* gui, PipelineStateManager* pipelineManager)
{
	m_dsvDescriptorSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);
	m_srvDescriptorSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	
	m_shadowManager->Initialize(device);
	m_shadowViewModel->Initialize(gui, m_shadowManager.get());
	CreateDSVDescriptorHeap(device);
	CreateDepthStencilView(device);
	CreateSRVDescriptorHeap(LunarConstants::TEXTURE_INFO.size(), device);
	
	m_pipelineStateManager = pipelineManager;
    m_basicCB = make_unique<ConstantBuffer>(device, sizeof(BasicConstants));
    m_lightingSystem->Initialize(device, LunarConstants::LIGHT_COUNT);
    m_sceneViewModel->Initialize(gui, this);
	m_materialManager->Initialize(device);
    for (auto& [layer, geometryEntries] : m_layeredGeometries)
    {
        for (auto& entry : geometryEntries)
        {
            entry->GeometryData->Initialize(device);
        }
    }

	if (m_layeredGeometries.find(RenderLayer::Mirror) != m_layeredGeometries.end())
	{
		auto mirror = m_geometriesByName.find("Mirror0");
		Plane* plane = static_cast<Plane*>(mirror->second->GeometryData.get());
		XMFLOAT4 mirrorPlane = plane->GetPlaneEquation();
		XMMATRIX R = MathUtils::MakeReflectionMatrix(mirrorPlane.x, mirrorPlane.y, mirrorPlane.z, mirrorPlane.w);
		for (auto& entry : m_layeredGeometries[RenderLayer::World])
		{
			LOG_DEBUG("Processing: " + entry->Name);
			Geometry* geometry = entry->GeometryData.get();	
			XMFLOAT4X4 geometryWorld = geometry->GetWorldMatrix();
			XMMATRIX W = XMMatrixTranspose(XMLoadFloat4x4(&geometryWorld));
			XMMATRIX reflectedWorld =  W * R;
			XMFLOAT4X4 reflectedWorldMatrix;
			XMStoreFloat4x4(&reflectedWorldMatrix, XMMatrixTranspose(reflectedWorld));
			
			auto reflectedGeometry = move(GeometryFactory::CloneGeometry(geometry));
			reflectedGeometry->SetWorldMatrix(reflectedWorldMatrix);
			
			reflectedGeometry->Initialize(device);
			string reflectedName = entry->Name + "_reflected";
			auto reflectedEntry = make_shared<GeometryEntry>(GeometryEntry{move(reflectedGeometry), reflectedName, RenderLayer::Reflect});
			m_layeredGeometries[RenderLayer::Reflect].push_back(reflectedEntry);
			m_geometriesByName[reflectedName] = reflectedEntry;
		}
	}
}

void SceneRenderer::CreateDSVDescriptorHeap(ID3D12Device* device)
{
	D3D12_DESCRIPTOR_HEAP_DESC dsvHeapDesc = {};
	dsvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
	dsvHeapDesc.NumDescriptors = 2; // TODO : refactoring
	dsvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	THROW_IF_FAILED(device->CreateDescriptorHeap(&dsvHeapDesc, IID_PPV_ARGS(m_dsvHeap.GetAddressOf())));
}

void SceneRenderer::CreateDepthStencilView(ID3D12Device* device)
{
	D3D12_RESOURCE_DESC depthStencilDesc = {};
	depthStencilDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	depthStencilDesc.Width = Utils::GetDisplayWidth();
	depthStencilDesc.Height = Utils::GetDisplayHeight();
	depthStencilDesc.DepthOrArraySize = 1;
	depthStencilDesc.MipLevels = 1;
	depthStencilDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
	depthStencilDesc.SampleDesc.Count = 1;
	depthStencilDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

	D3D12_CLEAR_VALUE depthOptimizedClearValue = {};
	depthOptimizedClearValue.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
	depthOptimizedClearValue.DepthStencil.Depth = 1.0f;
	depthOptimizedClearValue.DepthStencil.Stencil = 0;

	D3D12_HEAP_PROPERTIES heapProps = {};
	heapProps.Type = D3D12_HEAP_TYPE_DEFAULT;

	THROW_IF_FAILED(device->CreateCommittedResource(
		&heapProps,
		D3D12_HEAP_FLAG_NONE,
		&depthStencilDesc,
		D3D12_RESOURCE_STATE_DEPTH_WRITE,
		&depthOptimizedClearValue,
		IID_PPV_ARGS(m_depthStencilBuffer.GetAddressOf())));

	m_dsvHandle = m_dsvHeap->GetCPUDescriptorHandleForHeapStart();
	device->CreateDepthStencilView(m_depthStencilBuffer.Get(), nullptr, m_dsvHandle);
	m_shadowManager->CreateDSV(device, m_dsvHeap.Get());
}

void SceneRenderer::CreateSRVDescriptorHeap(UINT textureNums, ID3D12Device* device)
{
	LOG_FUNCTION_ENTRY();
	D3D12_DESCRIPTOR_HEAP_DESC srvHeapDesc = {};
	srvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	srvHeapDesc.NumDescriptors = static_cast<UINT>(textureNums) + 3;
	srvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	srvHeapDesc.NodeMask = 0;
	THROW_IF_FAILED(device->CreateDescriptorHeap(&srvHeapDesc, IID_PPV_ARGS(m_srvHeap.GetAddressOf())))
	m_srvHandle = m_srvHeap->GetCPUDescriptorHandleForHeapStart();

	// m_shadowManager->CreateSRV(device, m_srvHeap.Get());
}

void SceneRenderer::InitializeTextures(ID3D12Device* device, ID3D12GraphicsCommandList* commandList)
{
	m_textureManager->Initialize(device, commandList, m_srvHandle);
	m_shadowManager->CreateSRV(device, m_srvHeap.Get());

	// REFACTORING: Rename or refactor this method
	m_particleSystem->Initialize(device, commandList);
    // REFACTORING: To calculate the descriptor handle increment size
    m_particleSystem->AddSRVToDescriptorHeap(device, m_srvHeap.Get(), LunarConstants::TEXTURE_INFO.size() + 2);
}

void SceneRenderer::RenderShadowMap(ID3D12GraphicsCommandList* commandList)
{
	// To write depth
	D3D12_RESOURCE_BARRIER barrier = {};
	barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
	barrier.Transition.pResource = m_shadowManager->GetShadowTexture();
	barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_GENERIC_READ;
	barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_DEPTH_WRITE;
	commandList->ResourceBarrier(1, &barrier);

	commandList->RSSetViewports(1, &m_shadowManager->GetViewport());
	commandList->RSSetScissorRects(1, &m_shadowManager->GetScissorRect());

	commandList->OMSetRenderTargets(0, nullptr, FALSE, &m_shadowManager->GetDSVHandle());
	commandList->ClearDepthStencilView(m_shadowManager->GetDSVHandle(), D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);
	commandList->SetPipelineState(m_pipelineStateManager->GetPSO("shadowMap"));

	commandList->SetGraphicsRootConstantBufferView(
		LunarConstants::BASIC_CONSTANTS_ROOT_PARAMETER_INDEX,
		m_shadowManager->GetShadowCB()->GetResource()->GetGPUVirtualAddress());

	for (auto& entry : m_layeredGeometries[RenderLayer::World])
	{
		if (entry->IsVisible)
		{
			string materialName = entry->GeometryData->GetMaterialName();
			m_materialManager->BindConstantBuffer(materialName, commandList);
			entry->GeometryData->Draw(commandList);
		}
	}

	barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_DEPTH_WRITE;
	barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_GENERIC_READ;
	commandList->ResourceBarrier(1, &barrier);
}

void SceneRenderer::UpdateScene(float deltaTime)
{
    m_lightingSystem->UpdateLightData(m_basicConstants);
	m_shadowManager->UpdateShadowCB(m_basicConstants);
	m_basicConstants.shadowTransform = m_shadowManager->GetShadowTransform();
    m_basicCB->CopyData(&m_basicConstants, sizeof(BasicConstants));
}

void SceneRenderer::RenderScene(ID3D12GraphicsCommandList* commandList)
{
    commandList->SetGraphicsRootConstantBufferView(
        Lunar::LunarConstants::BASIC_CONSTANTS_ROOT_PARAMETER_INDEX, 
        m_basicCB->GetResource()->GetGPUVirtualAddress());

    RenderLayers(commandList);
}

bool SceneRenderer::AddCube(const string& name, const Transform& spawnTransform, RenderLayer layer)
{
    if (DoesGeometryExist(name))
    {
        LOG_ERROR("Geometry with name " + name + " already exists");
        return false; 
    }
    
    auto cube = GeometryFactory::CreateCube();
    cube->SetTransform(spawnTransform);
    
    auto entry = make_shared<GeometryEntry>(GeometryEntry{move(cube), name, layer});
    
    m_layeredGeometries[layer].push_back(entry);
    m_geometriesByName[name] = entry;
    
    return true;
}

bool SceneRenderer::AddSphere(const string& name, const Transform& spawnTransform, RenderLayer layer)
{
    if (DoesGeometryExist(name))
    {
        LOG_ERROR("Geometry with name " + name + " already exists");
        return false; 
    }
    
    auto sphere = GeometryFactory::CreateSphere();
    sphere->SetTransform(spawnTransform);
    
    auto entry = make_shared<GeometryEntry>(GeometryEntry{move(sphere), name, layer});
    
    m_layeredGeometries[layer].push_back(entry);
    m_geometriesByName[name] = entry;
    
    return true;
}

bool SceneRenderer::AddPlane(const string& name, const Transform& spawnTransform, float width, float height, RenderLayer layer)
{
    if (DoesGeometryExist(name))
    {
        LOG_ERROR("Geometry with name " + name + " already exists");
        return false; 
    }
    
    auto plane = GeometryFactory::CreatePlane(width, height);
    plane->SetTransform(spawnTransform);
    
    auto entry = make_shared<GeometryEntry>(GeometryEntry{move(plane), name, layer});
    
    // TODO: Need a debugging system to check the entry are in both containers
    m_layeredGeometries[layer].push_back(entry);
    m_geometriesByName[name] = entry;
    
    return true;
}

bool SceneRenderer::AddTree(const std::string& name, const Transform& spawnTransform, RenderLayer layer)
{
	if (DoesGeometryExist(name))
	{
		LOG_ERROR("Geometry with name " + name + " already exists");
		return false;
	}

	auto tree = GeometryFactory::CreateTree();
	auto entry = make_shared<GeometryEntry>(GeometryEntry{move(tree), name, layer});
	
	m_layeredGeometries[layer].push_back(entry);
	m_geometriesByName[name] = entry;
    
	return true;
}

void SceneRenderer::EmitParticles(const XMFLOAT3& position)
{
    m_particleSystem->EmitParticles(position);
}

bool SceneRenderer::SetGeometryTransform(const string& name, const Transform& newTransform)
{
    Geometry* geometry = GetGeometryEntry(name)->GeometryData.get();
    if (geometry)
    {
        geometry->SetTransform(newTransform);
        return true;
    }
    else
    {
        LOG_ERROR("Geometry with name " + name + " not found");
        return false;
    }
}

const Transform SceneRenderer::GetGeometryTransform(const string& name) const
{
    Geometry* geometry = GetGeometryEntry(name)->GeometryData.get();
    if (geometry)
    {
        return geometry->GetTransform();
    }
    else 
    {
        LOG_ERROR("Geometry with name " + name + " not found");
        return Transform(); 
    }
}

bool SceneRenderer::SetGeometryLocation(const string& name, const XMFLOAT3& newLocation)
{
    Geometry* geometry = GetGeometryEntry(name)->GeometryData.get();
    if (geometry)
    {
        geometry->SetLocation(newLocation);
        return true;
    }
    else
    {
        LOG_ERROR("Geometry with name " + name + " not found");
        return false;
    }
}

bool SceneRenderer::SetGeometryVisibility(const string& name, bool visible)
{
    auto entry = GetGeometryEntry(name);
    if (entry)
    {
        entry->IsVisible = visible;
        return true;
    }
    else 
    {
        LOG_ERROR("Geometry Entry with Geometry name " + name + " not found");
        return false;
    }
}

bool SceneRenderer::GetGeometryVisibility(const string& name) const
{
    auto entry = GetGeometryEntry(name);
    return entry ? entry->IsVisible : false;
}

bool SceneRenderer::DoesGeometryExist(const std::string& name) const
{
    return m_geometriesByName.find(name) != m_geometriesByName.end();
}

void SceneRenderer::RenderLayers(ID3D12GraphicsCommandList* commandList)
{
    for (auto& it : m_layeredGeometries)
    {
    	switch (it.first)
    	{
    		case RenderLayer::Mirror :
				commandList->OMSetStencilRef(1);
				commandList->SetPipelineState(m_pipelineStateManager->GetPSO("mirror"));
    			break;
    		case RenderLayer::Reflect :
				commandList->OMSetStencilRef(1);
				commandList->SetPipelineState(m_pipelineStateManager->GetPSO("reflect"));
    			break;
    		case RenderLayer::Background :
    			commandList->OMSetStencilRef(1);
    			commandList->SetPipelineState(m_pipelineStateManager->GetPSO("background"));
    			break;
    		case RenderLayer::World :
    			commandList->OMSetStencilRef(0);
    			commandList->SetPipelineState(m_pipelineStateManager->GetPSO("opaque"));	
    			break;
    		case RenderLayer::Billboard :
    			commandList->OMSetStencilRef(0);
    			commandList->SetPipelineState(m_pipelineStateManager->GetPSO("billboard"));
    			break;
			default:
    			LOG_ERROR("Not Handled RenderLayerType");
	    }
    	
        for (auto& entry : it.second)
        {
            if (entry->IsVisible)
            {
                string materialName = entry->GeometryData->GetMaterialName();
                m_materialManager->BindConstantBuffer(materialName, commandList);
                entry->GeometryData->Draw(commandList);
            }
        }
    }
}

GeometryEntry* SceneRenderer::GetGeometryEntry(const string& name)
{
    auto it = m_geometriesByName.find(name);
    return (it != m_geometriesByName.end()) ? it->second.get() : nullptr;
}

const GeometryEntry* SceneRenderer::GetGeometryEntry(const string& name) const
{
    auto it = m_geometriesByName.find(name);
    return (it != m_geometriesByName.end()) ? it->second.get() : nullptr;
}

std::vector<std::string> SceneRenderer::GetGeometryNames() const
{
    std::vector<std::string> names;
    for (const auto& [name, entry] : m_geometriesByName)
    {
        names.push_back(name);
    }

    return names;
}

}
