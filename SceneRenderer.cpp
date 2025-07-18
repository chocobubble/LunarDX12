#include "SceneRenderer.h"
#include "Geometry/GeometryFactory.h"
#include "MaterialManager.h"
#include "LightingSystem.h"
#include "ConstantBuffers.h"
#include "DescriptorAllocator.h"
#include "UI/LunarGUI.h"
#include "Utils/MathUtils.h"
#include "PipelineStateManager.h"
#include "ShadowManager.h"
#include "TextureManager.h"
#include "Utils/Utils.h"
#include "Geometry/Plane.h"
#include "UI/ShadowViewModel.h"
#include "ParticleSystem.h"
#include "UI/LightViewModel.h"
#include "UI/DebugViewModel.h"
#include "Geometry/Cube.h"
#include "Geometry/IcoSphere.h"

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
    m_lightViewModel = make_unique<LightViewModel>();
    m_lightingSystem = make_unique<LightingSystem>();
	m_shadowManager = make_unique<ShadowManager>();
	m_textureManager = make_unique<TextureManager>();
	m_shadowViewModel = make_unique<ShadowViewModel>();
    m_particleSystem = make_unique<ParticleSystem>();
    m_debugViewModel = make_unique<DebugViewModel>();

    // set default debug mode
    m_basicConstants.debugFlags = 
        LunarConstants::DebugFlags::NORMAL_MAP_ENABLED | 
        LunarConstants::DebugFlags::PBR_ENABLED |
        LunarConstants::DebugFlags::SHADOWS_ENABLED |
        LunarConstants::DebugFlags::AO_ENABLED |
        LunarConstants::DebugFlags::HEIGHT_MAP_ENABLED |
        LunarConstants::DebugFlags::METALLIC_MAP_ENABLED |
        LunarConstants::DebugFlags::ROUGHNESS_MAP_ENABLED |
        LunarConstants::DebugFlags::ALBEDO_MAP_ENABLED;
}

SceneRenderer::~SceneRenderer() = default;

void SceneRenderer::InitializeScene(ID3D12Device* device, LunarGui* gui, PipelineStateManager* pipelineManager)
{
	LOG_FUNCTION_ENTRY();
	
	m_dsvDescriptorSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);
	m_srvDescriptorSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	
	m_shadowManager->Initialize(device);
	m_shadowViewModel->Initialize(gui, m_shadowManager.get());
    m_debugViewModel->Initialize(gui, m_basicConstants);
	CreateDSVDescriptorHeap(device);
	CreateDepthStencilView(device);
	
	m_pipelineStateManager = pipelineManager;
    m_basicCB = make_unique<ConstantBuffer>(device, sizeof(BasicConstants));
    m_lightingSystem->Initialize(device, LunarConstants::LIGHT_COUNT);
    m_sceneViewModel->Initialize(gui, this);
    m_lightViewModel->Initialize(gui, m_lightingSystem.get(), this);
    
    // Debugging
    // CreateLightVisualizationCubes();
    
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

void SceneRenderer::InitializeTextures(ID3D12Device* device, ID3D12GraphicsCommandList* commandList, DescriptorAllocator* descriptorAllocator)
{
	m_textureManager->Initialize(device, commandList, descriptorAllocator, m_pipelineStateManager);
	m_shadowManager->CreateSRV(device, descriptorAllocator);

	// REFACTORING: Rename or refactor this method
	m_particleSystem->Initialize(device, commandList);
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

void SceneRenderer::UpdateParticleSystem(float deltaTime, ID3D12GraphicsCommandList* commandList)
{
	m_particleSystem->Update(deltaTime, commandList);
}

void SceneRenderer::RenderScene(ID3D12GraphicsCommandList* commandList)
{
    commandList->SetGraphicsRootConstantBufferView(
        Lunar::LunarConstants::BASIC_CONSTANTS_ROOT_PARAMETER_INDEX, 
        m_basicCB->GetResource()->GetGPUVirtualAddress());

	if (m_wireFrameRender) RenderWireframeOnly(commandList);
    else RenderLayers(commandList);
}

void SceneRenderer::RenderParticles(ID3D12GraphicsCommandList* commandList)
{
    commandList->SetPipelineState(m_pipelineStateManager->GetPSO("particles"));
    m_particleSystem->DrawParticles(commandList);
}

void SceneRenderer::EmitParticles(const XMFLOAT3& position)
{
    m_particleSystem->EmitParticles(position);
}

void SceneRenderer::CreateLightVisualizationCubes()
{
    LOG_FUNCTION_ENTRY();

	m_lightVisualization = true;
    
    // Directional Light - Yellow cube
    const LightData* dirLight = m_lightingSystem->GetLight("SunLight");
    if (dirLight) 
    {
        Transform dirTransform;
        dirTransform.Location = dirLight->Position;
        dirTransform.Scale = {0.3f, 0.3f, 0.3f};
        AddGeometry<IcoSphere>("LightViz_Directional", dirTransform, RenderLayer::Debug, LunarConstants::LightVizColors::DIRECTIONAL_LIGHT);
        
        // direction arrow - Orange
        for (int i = 1; i <= 3; ++i) 
        {
            XMFLOAT3 arrowPos = {
                dirLight->Position.x + dirLight->Direction.x * i * 0.5f,
                dirLight->Position.y + dirLight->Direction.y * i * 0.5f,
                dirLight->Position.z + dirLight->Direction.z * i * 0.5f
            };
            Transform arrowTransform;
            arrowTransform.Location = arrowPos;
            arrowTransform.Scale = {0.1f, 0.1f, 0.1f};
            AddGeometry<Cube>("LightViz_DirArrow" + std::to_string(i), arrowTransform, RenderLayer::Debug, LunarConstants::LightVizColors::DIRECTIONAL_ARROW);
        }
    }
    
    // Point Light - Red cube
    auto* pointLight = m_lightingSystem->GetLight("RoomLight");
    if (pointLight) 
    {
        Transform pointTransform;
        pointTransform.Location = pointLight->Position;
        pointTransform.Scale = {0.3f, 0.3f, 0.3f};
        AddGeometry<Cube>("LightViz_Point", pointTransform, RenderLayer::Debug, LunarConstants::LightVizColors::POINT_LIGHT);
    }
    
    // Spot Light - Blue cube
    auto* spotLight = m_lightingSystem->GetLight("FlashLight");
    if (spotLight) {
        Transform spotTransform;
        spotTransform.Location = spotLight->Position;
        spotTransform.Scale = {0.3f, 0.3f, 0.3f};
        AddGeometry<Cube>("LightViz_Spot", spotTransform, RenderLayer::Debug, LunarConstants::LightVizColors::SPOT_LIGHT);
        
        // direction arrow - sky-blue
        for (int i = 1; i <= 2; ++i) 
        {
            XMFLOAT3 spotArrowPos = {
                spotLight->Position.x + spotLight->Direction.x * i * 0.5f,
                spotLight->Position.y + spotLight->Direction.y * i * 0.5f,
                spotLight->Position.z + spotLight->Direction.z * i * 0.5f
            };
            Transform spotArrowTransform;
            spotArrowTransform.Location = spotArrowPos;
            spotArrowTransform.Scale = {0.1f, 0.1f, 0.1f};
            AddGeometry<Cube>("LightViz_SpotArrow" + std::to_string(i), spotArrowTransform, RenderLayer::Debug, LunarConstants::LightVizColors::SPOT_ARROW);
        }
    }
    
    LOG_FUNCTION_EXIT();
}

void SceneRenderer::UpdateLightVisualization()
{
	if (!m_lightVisualization) return;
	
    // Directional Light 
    auto* dirLight = m_lightingSystem->GetLight("SunLight");
    if (dirLight) 
    {
        SetGeometryLocation("LightViz_Directional", dirLight->Position);
        
        // arrow 
        for (int i = 1; i <= 3; ++i) 
        {
            XMFLOAT3 arrowPos = {
                dirLight->Position.x + dirLight->Direction.x * i * 0.5f,
                dirLight->Position.y + dirLight->Direction.y * i * 0.5f,
                dirLight->Position.z + dirLight->Direction.z * i * 0.5f
            };
            SetGeometryLocation("LightViz_DirArrow" + std::to_string(i), arrowPos);
        }
    }
    
    // Point Light 
    auto* pointLight = m_lightingSystem->GetLight("RoomLight");
    if (pointLight)
    {
        SetGeometryLocation("LightViz_Point", pointLight->Position);
    }
    
    // Spot Light 
    auto* spotLight = m_lightingSystem->GetLight("FlashLight");
    if (spotLight) 
    {
        SetGeometryLocation("LightViz_Spot", spotLight->Position);
        
        // arrow
        for (int i = 1; i <= 2; ++i) 
        {
            XMFLOAT3 spotArrowPos = {
                spotLight->Position.x + spotLight->Direction.x * i * 0.5f,
                spotLight->Position.y + spotLight->Direction.y * i * 0.5f,
                spotLight->Position.z + spotLight->Direction.z * i * 0.5f
            };
            SetGeometryLocation("LightViz_SpotArrow" + std::to_string(i), spotArrowPos);
        }
    }
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
			case RenderLayer::Tessellation :
				commandList->OMSetStencilRef(0);
				commandList->SetPipelineState(m_pipelineStateManager->GetPSO("tessellation"));
				break;
			case RenderLayer::Billboard :
				commandList->OMSetStencilRef(0);
				commandList->SetPipelineState(m_pipelineStateManager->GetPSO("billboard"));
				break;
			case RenderLayer::Normal :
				if (m_basicConstants.debugFlags & LunarConstants::DebugFlags::SHOW_NORMALS)
				{
					commandList->OMSetStencilRef(0);
					commandList->SetPipelineState(m_pipelineStateManager->GetPSO("normal"));
					// Refactor
					for (auto& entry : m_layeredGeometries[RenderLayer::World])
					{
						entry->GeometryData->DrawNormals(commandList);
					}
				}
				continue;
			case RenderLayer::Debug :
				commandList->OMSetStencilRef(0);
				commandList->SetPipelineState(m_pipelineStateManager->GetPSO("opaque"));
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

void SceneRenderer::RenderWireframeOnly(ID3D12GraphicsCommandList* commandList)
{
	commandList->SetPipelineState(m_pipelineStateManager->GetPSO("opaque_wireframe"));
	for (auto& geoEntry : m_layeredGeometries[RenderLayer::World])
	{
		if (geoEntry->IsVisible)
		{
			string materialName = geoEntry->GeometryData->GetMaterialName();
			m_materialManager->BindConstantBuffer(materialName, commandList);
			geoEntry->GeometryData->Draw(commandList);
		}
	}

	commandList->SetPipelineState(m_pipelineStateManager->GetPSO("tessellation_wireframe"));
	for (auto& geoEntry : m_layeredGeometries[RenderLayer::Tessellation])
	{
		if (geoEntry->IsVisible)
		{
			string materialName = geoEntry->GeometryData->GetMaterialName();
			m_materialManager->BindConstantBuffer(materialName, commandList);
			geoEntry->GeometryData->Draw(commandList);
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

} // namespace Lunar
