#include "SceneRenderer.h"
#include "Geometry/GeometryFactory.h"
#include "MaterialManager.h"

using namespace DirectX;
using namespace std;

namespace Lunar
{

SceneRenderer::SceneRenderer()
{
    m_materialManager = make_unique<MaterialManager>();
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
    
    auto entry = make_shared<GeometryEntry>(move(cube), name, layer);
    
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
    
    auto entry = make_shared<GeometryEntry>(move(sphere), name, layer);
    
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
    
    auto entry = make_shared<GeometryEntry>(move(plane), name, layer);
    
    // TODO: Need a debugging system to check the entry are in both containers
    m_layeredGeometries[layer].push_back(entry);
    m_geometriesByName[name] = entry;
    
    return true;
}

bool SceneRenderer::SetGeometryTransform(const string& name, const Transform& newTransform)
{
    Geometry* geometry = GetGeometryByName(name);
    if (geometry)
    {
        geometry->SetTransform(newTransform);
        return true;
    }
    else
    {
        LOG_ERROR("Geometry with name " + name + " not found")
        return false;
    }
}

const Transform SceneRenderer::GetGeometryTransform(const string& name) const
{
    const Geometry* geometry = GetGeometryByName(name);
    if (geometry)
    {
        return geometry->GetTransform();
    }
    else 
    {
        LOG_ERROR("Geometry with name " + name + " not found")
        return Transform(); 
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
        LOG_ERROR("Geometry Entry with Geometry name " + name + " not found")
        return false;
    }
}

bool SceneRenderer::GetGeometryVisibility(const string& name) const
{
    auto entry = GetGeometryEntry(name);
    return entry ? entry->IsVisible : false;
}

bool SceneRenderer::DoesGeometryExist(const string& name) const
{
    return m_geometriesByName.find(name) != m_geometriesByName.end();
}

void SceneRenderer::InitializeScene(ID3D12Device* device, ID3D12GraphicsCommandList* commandList)
{
    for (auto& [layer, geometryEntries] : m_layeredGeometries)
    {
        for (auto& entry : geometryEntries)
        {
            entry->GeometryData->Initialize(device, commandList);
        }
    }
}

void SceneRenderer::RenderScene(ID3D12GraphicsCommandList* commandList)
{
    RenderLayers(commandList);
}

void SceneRenderer::RenderLayers(ID3D12GraphicsCommandList* commandList)
{
    for (auto& it : m_layeredGeometries)
    {
        for (auto& entry : it->second)
        {
            if (entry->IsVisible)
            {
                string materialName = entry->GeometryData->GetMaterialName();
                m_materialManager->BindConstantBuffer(commandList, materialName);
                entry->GeometryData->Draw(commandList);
            }
        }
    }
}

GeometryEntry* SceneRenderer::GetGeometryEntry(const string& name)
{
    auto it = m_geometriesByName.find(name);
    return (it != m_geometriesByName.end()) ? it->second : nullptr;
}

const GeometryEntry* SceneRenderer::GetGeometryEntry(const string& name) const
{
    auto it = m_geometriesByName.find(name);
    return (it != m_geometriesByName.end()) ? it->second : nullptr;
}

}
