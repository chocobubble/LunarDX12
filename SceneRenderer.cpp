#include "SceneRenderer.h"
#include "Geometry/GeometryFactory.h"
#include "MaterialManager.h"
#include "LightingSystem.h"
#include "ConstantBuffers.h"
#include "LunarGUI.h"
#include "MathUtils.h"
#include "PipelineStateManager.h"
#include "Geometry/Plane.h"

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
}

void SceneRenderer::InitializeScene(ID3D12Device* device, LunarGui* gui, PipelineStateManager* pipelineManager)
{
	m_pipelineStateManager = pipelineManager;
    m_basicCB = make_unique<ConstantBuffer>(device, sizeof(BasicConstants));
    m_lightingSystem->Initialize(device, Lunar::LunarConstants::LIGHT_COUNT);
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

void SceneRenderer::UpdateScene(float deltaTime)
{
    m_lightingSystem->UpdateLightData(m_basicConstants);
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
