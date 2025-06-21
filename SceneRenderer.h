#pragma once
#include <vector>
#include <memory>
#include <map>
#include <unordered_map>
#include <string>
#include <DirectXMath.h>

class Geometry;
class MaterialManager;
struct ID3D12Device;
struct ID3D12GraphicsCommandList;

using namespace DirectX;

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
    XMFLOAT3 Location = {0.0f, 0.0f, 0.0f};
    XMFLOAT3 Rotation = {0.0f, 0.0f, 0.0f};
    XMFLOAT3 Scale = {1.0f, 1.0f, 1.0f};
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
    
    bool AddCube(const std::string& Name, const Transform& SpawnTransform = Transform(), RenderLayer layer = RenderLayer::World);
    bool AddSphere(const std::string& Name, const Transform& SpawnTransform = Transform(), float radius = 1.0f, RenderLayer layer = RenderLayer::World);
    bool AddPlane(const std::string& Name, const Transform& SpawnTransform = Transform(), float width = 10.0f, float height = 10.0f, RenderLayer layer = RenderLayer::World);
    
    const Geometry* FindGeometryByName(const std::string& Name) const;
    
    bool SetGeometryTransform(const std::string& Name, const Transform& NewTransform);
    
    Transform GetGeometryTransform(const std::string& Name) const;
    
    bool SetGeometryVisibility(const std::string& Name, bool bVisible);
    bool GetGeometryVisibility(const std::string& Name) const;
    
    bool RemoveGeometry(const std::string& Name);
    bool DoesGeometryExist(const std::string& Name) const;
    std::vector<std::string> GetAllGeometryNames() const;
    
    void InitializeScene(ID3D12Device* device, ID3D12GraphicsCommandList* commandList);
    void RenderScene(ID3D12GraphicsCommandList* commandList);
    void RenderLayer(ID3D12GraphicsCommandList* commandList, RenderLayer layer);
    
    void ClearScene();
    size_t GetGeometryCount() const;
    
private:
    std::map<RenderLayer, std::vector<std::shared_ptr<GeometryEntry>>> m_layeredGeometries;
    std::unordered_map<std::string, std::shared_ptr<GeometryEntry>> m_geometriesByName;
    std::unique_ptr<MaterialManager> m_materialManager;
    
    void ApplyTransformToGeometry(Geometry* geometry, const Transform& transform);
    std::shared_ptr<GeometryEntry> FindGeometryEntry(const std::string& Name);
    std::shared_ptr<const GeometryEntry> FindGeometryEntry(const std::string& Name) const;
};
}
