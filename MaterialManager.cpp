#include <DirectXMath.h>
#include <map>

#include "MaterialManager.h"
#include "ConstantBuffers.h"
#include "LunarConstants.h"
#include "Logger.h"

using namespace std;
using namespace DirectX;

namespace Lunar
{

MaterialManager::MaterialManager()
{
}

void MaterialManager::Initialize(ID3D12Device* device)
{
    CreateMaterials(device);
}

void MaterialManager::CreateMaterials(ID3D12Device* device)
{
    auto createMaterial = [&](string name, XMFLOAT4 diffuse, XMFLOAT3 fresnelR0, float roughness) {
        MaterialConstants material = {
            diffuse,
            fresnelR0,
            roughness
        };
        m_materialMap[name] = {
            material,
            std::make_unique<ConstantBuffer>(device, sizeof(MaterialConstants))
        };
    };
	createMaterial("default", XMFLOAT4(0.9f, 0.9f, 0.9f, 1.0f), XMFLOAT3(0.01f, 0.01f, 0.01f), 0.125f);
}

void MaterialManager::UpdateMaterial(const std::string& name, const MaterialConstants& material)
{
    auto it = m_materialMap.find(name);
    if (it == m_materialMap.end())
    {
        LOG_ERROR("Material ", name, " not found");
        return;
    }
    MaterialEntry& entry = it->second;
    entry.material = material;
    entry.constantBuffer->CopyData(&(entry.material), sizeof(MaterialConstants));
} 

void MaterialManager::BindConstantBuffer(const std::string& name, ID3D12GraphicsCommandList* commandList)
{
    auto it = m_materialMap.find(name);
    if (it == m_materialMap.end())
    {
        LOG_ERROR("Material ", name, " not found");
        return;
    }
    const MaterialEntry& entry = it->second;
    commandList->SetGraphicsRootConstantBufferView(
        Lunar::LunarConstants::MATERIAL_CONSTANTS_ROOT_PARAMETER_INDEX, 
        entry.constantBuffer->GetResource()->GetGPUVirtualAddress()); 
} 

const MaterialConstants& MaterialManager::GetMaterial(const std::string& name) const
{
	auto it = m_materialMap.find(name);
	if (it == m_materialMap.end())
	{
		LOG_ERROR("Material ", name, " not found");
		return MaterialConstants();
	}
    return it->second.material;
}


std::vector<std::string> MaterialManager::GetMaterialNames() const
{
    std::vector<std::string> names;
    for (auto& it : m_materialMap)
    {
        names.push_back(it.first);
    }
    return names;
}

} // namespace Lunar