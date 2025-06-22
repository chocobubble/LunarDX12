#pragma once
#include <vector>
#include <memory>
#include <map>
#include <unordered_map>
#include <string>
#include <DirectXMath.h>

class Geometry;
class MaterialManager;
class SceneViewModel;
class LightingSystem;
struct ID3D12Device;
struct ID3D12GraphicsCommandList;

namespace Lunar
{
enum class RenderLayer : int32
{
    Background = 0,
    World = 1,
    Translucent = 2,
    UI = 3,
    COUNT = 4 // for looping enum
};

struct Transform
{
    DirectX::XMFLOAT3 Location = {0.0f, 0.0f, 0.0f};
    DirectX::XMFLOAT3 Rotation = {0.0f, 0.0f, 0.0f};
    DirectX::XMFLOAT3 Scale = {1.0f, 1.0f, 1.0f};
};

struct GeometryEntry
{
    std::unique_ptr<Geometry> GeometryData;
    std::string Name;
    RenderLayer Layer;
    bool IsVisible = true;
};

class SceneRenderer
{
public:
    SceneRenderer();
    ~SceneRenderer() = default;

    void InitializeScene(ID3D12Device* device, ID3D12GraphicsCommandList* commandList);
    void UpdateScene(float deltaTime);
    void RenderScene(ID3D12GraphicsCommandList* commandList);
    
    bool AddCube(const std::string& name, const Transform& spawnTransform = Transform(), RenderLayer layer = RenderLayer::World);
    bool AddSphere(const std::string& name, const Transform& spawnTransform = Transform(), RenderLayer layer = RenderLayer::World);
    bool AddPlane(const std::string& name, const Transform& spawnTransform = Transform(), float width = 10.0f, float height = 10.0f, RenderLayer layer = RenderLayer::World);
    
    bool SetGeometryTransform(const std::string& name, const Transform& newTransform);
    bool SetGeometryLocation(const std::string& name, const DirectX::XMFLOAT3& newLocation);
    bool SetGeometryVisibility(const std::string& name, bool visible);
    
    bool DoesGeometryExist(const std::string& name) const;
    const Transform GetGeometryTransform(const std::string& name) const;
    const GeometryEntry* GetGeometryEntry(const std::string& name) const;
    std::vector<std::string> GetGeometryNames() const;
    const MaterialManager* GetMaterialManager() const;
    const SceneViewModel* GetSceneViewModel() const;
    const LightingSystem* GetLightingSystem() const;
    
private:
    std::map<RenderLayer, std::vector<std::shared_ptr<GeometryEntry>>> m_layeredGeometries;
    std::unordered_map<std::string, std::shared_ptr<GeometryEntry>> m_geometriesByName;
    std::unique_ptr<MaterialManager> m_materialManager;
    std::unique_ptr<SceneViewModel> m_sceneViewModel;
    std::unique_ptr<LightingSystem> m_lightingSystem;
    
    void RenderLayers(ID3D12GraphicsCommandList* commandList, RenderLayer layer);
    bool GetGeometryVisibility(const std::string& name) const;
    GeometryEntry* GetGeometryEntry(const std::string& name);
};
}
