#pragma once

#include <memory>
#include <string>
#include <unordered_map>

#include "ConstantBuffers.h"

namespace Lunar 
{
class MaterialManager
{
private:
    struct MaterialEntry {
        MaterialConstants               material;
        std::unique_ptr<ConstantBuffer> constantBuffer;
    };
public:
	
    MaterialManager();
    ~MaterialManager() = default;

    void Initialize(ID3D12Device* device);

    void CreateMaterials(ID3D12Device* device);
    void UpdateMaterial(const std::string& name, const MaterialConstants& materialData); 
    void BindConstantBuffer(const std::string& name, ID3D12GraphicsCommandList* commandList);
    const MaterialConstants& GetMaterial(const std::string& name) const;

private:
    std::unordered_map<std::string, MaterialEntry> m_materialMap;
};
} // namespace Lunar