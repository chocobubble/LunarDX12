#pragma once
#include <d3d12.h>
#include <vector>
#include <memory>
#include <map>
#include <unordered_map>
#include <string>
#include <DirectXMath.h>

#include "Utils/Logger.h"
#include "MaterialManager.h"
#include "UI/SceneViewModel.h"
#include "Geometry/Transform.h"
#include "Geometry/Geometry.h"

namespace Lunar
{

class ShadowViewModel;
class LightViewModel;
class TextureManager;
class ShadowManager;
class PipelineStateManager;
class LunarGui;
class LightingSystem;
class ConstantBuffer;
class ParticleSystem;
class DebugViewModel;
class DescriptorAllocator;
struct BasicConstants;

enum class RenderLayer
{
    Background,
    World,
	Tessellation,
	Mirror,
	Reflect,
	Billboard,
    Translucent,
	Normal,
    Debug,
    UI
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
	friend SceneViewModel;
public:
    SceneRenderer();
    ~SceneRenderer();

    void InitializeScene(ID3D12Device* device, LunarGui* gui, PipelineStateManager* pipelineManager);
	void CreateDSVDescriptorHeap(ID3D12Device* device);
	void CreateDepthStencilView(ID3D12Device* device);
	void InitializeTextures(ID3D12Device* device, ID3D12GraphicsCommandList* commandList, DescriptorAllocator* descriptorAllocator);

	void RenderShadowMap(ID3D12GraphicsCommandList* commandList);
	void UpdateScene(float deltaTime);
    void UpdateParticleSystem(float deltaTime, ID3D12GraphicsCommandList* commandList);
    void RenderScene(ID3D12GraphicsCommandList* commandList);
    void RenderParticles(ID3D12GraphicsCommandList* commandList);
    
    void EmitParticles(const DirectX::XMFLOAT3& position);
    
    bool SetGeometryTransform(const std::string& name, const Transform& newTransform);
    bool SetGeometryLocation(const std::string& name, const DirectX::XMFLOAT3& newLocation);
    bool SetGeometryVisibility(const std::string& name, bool visible);
    
    bool DoesGeometryExist(const std::string& name) const;
    const Transform GetGeometryTransform(const std::string& name) const;
    const GeometryEntry* GetGeometryEntry(const std::string& name) const;
    std::vector<std::string> GetGeometryNames() const;
    const MaterialManager* GetMaterialManager() const { return m_materialManager.get();}
    const SceneViewModel* GetSceneViewModel() const { return m_sceneViewModel.get(); }
    const LightViewModel* GetLightViewModel() const { return m_lightViewModel.get(); }
    const LightingSystem* GetLightingSystem() const { return m_lightingSystem.get(); }
    LightingSystem* GetLightingSystem() { return m_lightingSystem.get(); }
	BasicConstants& GetBasicConstants() { return m_basicConstants; }
	ID3D12DescriptorHeap* GetDSVHeap() { return m_dsvHeap.Get(); };
	ParticleSystem* GetParticleSystem() { return m_particleSystem.get(); };
    
private:
    std::map<RenderLayer, std::vector<std::shared_ptr<GeometryEntry>>> m_layeredGeometries;
    std::unordered_map<std::string, std::shared_ptr<GeometryEntry>> m_geometriesByName;
    std::unique_ptr<MaterialManager> m_materialManager;
    std::unique_ptr<TextureManager> m_textureManager;
	std::unique_ptr<ShadowManager> m_shadowManager;
	std::unique_ptr<ShadowViewModel> m_shadowViewModel;
    std::unique_ptr<SceneViewModel> m_sceneViewModel;
    std::unique_ptr<LightViewModel> m_lightViewModel;
    std::unique_ptr<DebugViewModel> m_debugViewModel;
    std::unique_ptr<LightingSystem> m_lightingSystem;
    std::unique_ptr<ParticleSystem> m_particleSystem;
    std::unique_ptr<ConstantBuffer> m_basicCB;
	PipelineStateManager* m_pipelineStateManager = nullptr;

	BasicConstants m_basicConstants;
    void RenderLayers(ID3D12GraphicsCommandList* commandList);
	void RenderWireframeOnly(ID3D12GraphicsCommandList* commandList);
    bool GetGeometryVisibility(const std::string& name) const;
    GeometryEntry* GetGeometryEntry(const std::string& name);

	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> m_dsvHeap;
	D3D12_CPU_DESCRIPTOR_HANDLE m_dsvHandle;
	UINT m_dsvDescriptorSize;
	Microsoft::WRL::ComPtr<ID3D12Resource> m_depthStencilBuffer;
	D3D12_CPU_DESCRIPTOR_HANDLE m_srvHandle;
	UINT m_srvDescriptorSize;

	bool m_wireFrameRender = false;
	bool m_lightVisualization = false;

// TODO: Move to proper location
public: // Template Section
    template<typename T>
    bool AddGeometry(const std::string& name, 
        const Transform& spawnTransform = Transform(), 
        RenderLayer layer = RenderLayer::World, 
        const DirectX::XMFLOAT4& color = {1.0f, 1.0f, 1.0f, 1.0f},
        const std::string& materialName = "default")
    {
    	LOG_FUNCTION_ENTRY();
    	
        if (DoesGeometryExist(name))
        {
            LOG_ERROR("Geometry with name " + name + " already exists");
            return false; 
        }
        
        auto geometry = std::make_unique<T>();
        geometry->SetTransform(spawnTransform);
        geometry->SetColor(color);
        geometry->SetMaterialName(materialName);
    	
    	if (layer == RenderLayer::Tessellation) geometry->SetTopologyType(D3D_PRIMITIVE_TOPOLOGY_3_CONTROL_POINT_PATCHLIST);
        
        auto entry = std::make_shared<GeometryEntry>(GeometryEntry{std::move(geometry), name, layer});
    	
        m_layeredGeometries[layer].push_back(entry);
        m_geometriesByName[name] = entry;
        
        return true;
    }
    
public:  // Debug Section 
    // Light Visualization
    void CreateLightVisualizationCubes();
    void UpdateLightVisualization();

};
}
