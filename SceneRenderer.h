#pragma once
#include <d3d12.h>
#include <vector>
#include <memory>
#include <map>
#include <unordered_map>
#include <string>
#include <DirectXMath.h>

#include "MaterialManager.h"
#include "SceneViewModel.h"
#include "Geometry/Transform.h"
#include "Geometry/Geometry.h"

namespace Lunar
{

class ShadowViewModel;
class TextureManager;
class ShadowManager;
class PipelineStateManager;
class LunarGui;
class LightingSystem;
class ConstantBuffer;
class ParticleSystem;
struct BasicConstants;

enum class RenderLayer
{
    Background,
    World,
	Mirror,
	Reflect,
	Billboard,
    Translucent,
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
public:
    SceneRenderer();
    ~SceneRenderer();

    void InitializeScene(ID3D12Device* device, LunarGui* gui, PipelineStateManager* pipelineManager);
	void CreateDSVDescriptorHeap(ID3D12Device* device);
	void CreateDepthStencilView(ID3D12Device* device);
	void CreateSRVDescriptorHeap(UINT textureNums, ID3D12Device* device);
	void InitializeTextures(ID3D12Device* device, ID3D12GraphicsCommandList* commandList);

	void RenderShadowMap(ID3D12GraphicsCommandList* commandList);
	void UpdateScene(float deltaTime);
    void RenderScene(ID3D12GraphicsCommandList* commandList);
    
    bool AddCube(const std::string& name, const Transform& spawnTransform = Transform(), RenderLayer layer = RenderLayer::World);
    bool AddSphere(const std::string& name, const Transform& spawnTransform = Transform(), RenderLayer layer = RenderLayer::World);
    bool AddPlane(const std::string& name, const Transform& spawnTransform = Transform(), float width = 10.0f, float height = 10.0f, RenderLayer layer = RenderLayer::World);
	bool AddTree(const std::string& name, const Transform& spawnTransform = Transform(), RenderLayer layer = RenderLayer::Billboard);

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
    const LightingSystem* GetLightingSystem() const { return m_lightingSystem.get(); }
	BasicConstants& GetBasicConstants() { return m_basicConstants; }
	ID3D12DescriptorHeap* GetSRVHeap() { return m_srvHeap.Get(); };
	ID3D12DescriptorHeap* GetDSVHeap() { return m_dsvHeap.Get(); };
    
private:
    std::map<RenderLayer, std::vector<std::shared_ptr<GeometryEntry>>> m_layeredGeometries;
    std::unordered_map<std::string, std::shared_ptr<GeometryEntry>> m_geometriesByName;
    std::unique_ptr<MaterialManager> m_materialManager;
    std::unique_ptr<TextureManager> m_textureManager;
	std::unique_ptr<ShadowManager> m_shadowManager;
	std::unique_ptr<ShadowViewModel> m_shadowViewModel;
    std::unique_ptr<SceneViewModel> m_sceneViewModel;
    std::unique_ptr<LightingSystem> m_lightingSystem;
    std::unique_ptr<ParticleSystem> m_particleSystem;
    std::unique_ptr<ConstantBuffer> m_basicCB;
	PipelineStateManager* m_pipelineStateManager = nullptr;

	BasicConstants m_basicConstants;
    void RenderLayers(ID3D12GraphicsCommandList* commandList);
    bool GetGeometryVisibility(const std::string& name) const;
    GeometryEntry* GetGeometryEntry(const std::string& name);

	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> m_dsvHeap;
	D3D12_CPU_DESCRIPTOR_HANDLE m_dsvHandle;
	UINT m_dsvDescriptorSize;
	Microsoft::WRL::ComPtr<ID3D12Resource> m_depthStencilBuffer;
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> m_srvHeap;
	D3D12_CPU_DESCRIPTOR_HANDLE m_srvHandle;
	UINT m_srvDescriptorSize;
};
}
