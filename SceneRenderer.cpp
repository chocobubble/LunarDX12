#include "SceneRenderer.h"
#include "Geometry/GeometryFactory.h"
#include "MaterialManager.h"
#include "LightingSystem.h"
#include "ConstantBuffers.h"
#include "LunarGUI.h"
#include "PipelineStateManager.h"

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
    m_lightingSystem->Initialize(device, Lunar::Constants::LIGHT_COUNT);
    m_sceneViewModel->Initialize(gui, this);
	m_materialManager->Initialize(device);
    for (auto& [layer, geometryEntries] : m_layeredGeometries)
    {
        for (auto& entry : geometryEntries)
        {
            entry->GeometryData->Initialize(device);
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
        Lunar::Constants::BASIC_CONSTANTS_ROOT_PARAMETER_INDEX, 
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
    	if (it.first == RenderLayer::Mirror)
    	{
    		m_basicConstants.textureIndex = 0;
    		commandList->OMSetStencilRef(1);
    		commandList->SetPipelineState(m_pipelineStateManager->GetPSO("mirror"));
    	}
    	else if (it.first == RenderLayer::World)
    	{
    		continue;
    	}
    	else if (it.first == RenderLayer::Reflect)
    	{
    		m_basicConstants.textureIndex = 0;
    		// commandList->OMSetStencilRef(1);
    		// commandList->SetPipelineState(m_pipelineStateManager->GetPSO("reflect"));
    		commandList->OMSetStencilRef(0);
    		commandList->SetPipelineState(m_pipelineStateManager->GetPSO("opaque"));

    		auto mirror = m_geometriesByName.find("Mirror0");
    		Plane* plane = static_cast<Plane*>(mirror->second->GeometryData.get());
    		XMFLOAT4 mirrorPlane = plane->GetPlaneEquation();
    		XMMATRIX R = MathUtils::MakeReflectionMatrix(mirrorPlane.x, mirrorPlane.y, mirrorPlane.z, mirrorPlane.w);
    		for (auto& entry : m_layeredGeometries[RenderLayer::World])
    		{
    			if (entry->IsVisible)
    			{
    				LOG_DEBUG(entry->Name);
    				Geometry* geometry = entry->GeometryData.get();	
    				XMFLOAT4X4 geometryWorld = geometry->GetWorldMatrix();
    				XMMATRIX W = XMMatrixTranspose(XMLoadFloat4x4(&geometryWorld));
    				XMMATRIX reflectedWorld = W * R;
    				XMFLOAT4X4 reflectedWorldMatrix;
    				XMStoreFloat4x4(&reflectedWorldMatrix, XMMatrixTranspose(reflectedWorld));
    				geometry->SetWorldMatrix(reflectedWorldMatrix);
    				string materialName = entry->GeometryData->GetMaterialName();
    				m_materialManager->BindConstantBuffer(materialName, commandList);
    				entry->GeometryData->Draw(commandList);
    				geometry->SetWorldMatrix(geometryWorld);
    			}
    		}
    	}
    	else
    	{
    		m_basicConstants.textureIndex = 0;
    		commandList->OMSetStencilRef(0);	
    		commandList->SetPipelineState(m_pipelineStateManager->GetPSO("opaque"));	
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
