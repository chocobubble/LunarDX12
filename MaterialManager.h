#pragma once

#include "ConstantBuffers.h"

namespace Lunar 
{
class MaterialManager
{
private:
    struct MaterialEntry {
        MaterialConstants material;
        std::make_shared<ConstantBuffer> constantBuffer; 
    };
public:
    MaterialManager();
    ~MaterialManager() = default;

    void Initialize();

    void CreateMaterials();
    void UpdateMaterial(const std::string& name, const MaterialConstants& materialData); 
    void BindConstantBuffer(const std::string& name, ID3D12GraphicsCommandList* commandList, UINT rootParameterIndex);
    const MaterialEntry& GetMaterial(const std::string& name) const;

private:
    std::unordered_map<std::string, MaterialEntry> m_materialMap;
}
} // namespace Lunar