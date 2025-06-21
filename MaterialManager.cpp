#include <DirectXMath.h>
#include <map>

#include "MaterialManager.h"
#include "ConstantBuffers.h"
#include "LunarConstants.h"
#include "Logger.h"

using std;
using DirectX;

namespace Lunar
{

MaterialManager::MaterialManager()
{
    CreateMaterial();
}

void MaterialManager::Initialize()
{
}

void MaterialManager::CreateMaterial()
{
    MaterialEntry entry = {
        { { 0.2f, 0.6f, 0.2f, 1.0f }, // diffuse
        { 0.01f, 0.01f, 0.01f }, // FresnelR0
        0.125f }, // Roughness
        std::make_shared<ConstantBuffer>(sizeof(MaterialConstants))
    }
}

void MaterialManager::UpdateMaterial(const std::string& name, const MaterialConstants& material)
{
    auto it = m_materials.find(name);
    if (it == m_materials.end())
    {
        LOG_ERROR("Material ", name, " not found");
        return;
    }
    const MaterialEntry& entry = it->second;
    entry.material = material;
    entry.constantBuffer->CopyData(material, sizeof(MaterialConstants));
} 

void MaterialManager::BindConstantBuffer(const std::string& name, ID3D12GraphicsCommandList* commandList);
{
    auto it = m_materials.find(name);
    if (it == m_materials.end())
    {
        LOG_ERROR("Material ", name, " not found");
        return;
    }
    const MaterialEntry& entry = it->second;
    commandList->SetGraphicsRootConstantBufferView(
        Lunar::Constants::MATERIAL_CONSTANTS_ROOT_PARAMETER_INDEX, 
        entry.constantBuffer->GetGPUVirtualAddress()); 
} 

const MaterialEntry& MaterialManager::GetMaterial(const std::string& name) const
{
    if (m_materials.find(name) == m_materials.end())
    {
        LOG_ERROR("Material ", name, " not found");
        return m_materials["default"];
    }
    return m_materials[name];
}

} // namespace Lunar