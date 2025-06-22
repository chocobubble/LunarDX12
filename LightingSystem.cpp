#include "LightingSystem.h"

#include "ConstantBuffers.h"
#include "LunarConstants.h"
#include "Utils.h"

using namespace DirectX;
using namespace std;

namespace Lunar
{
void LightingSystem::Initialize(ID3D12Device* device, UINT maxLights)
{
    m_maxLights = maxLights;

    LightData sunLight;
    sunLight.Strength = {1.2f, 1.0f, 0.8f};      
    sunLight.Direction = {0.3f, -0.8f, 0.5f};    
    sunLight.FalloffStart = 0.0f;                
    sunLight.FalloffEnd = 0.0f;                  
    sunLight.Position = {0.0f, 0.0f, 0.0f};      
    sunLight.SpotPower = 0.0f;                   

    AddLight("SunLight", sunLight, LightType::Directional);

    LightData roomLight;
    roomLight.Strength = {1.0f, 0.9f, 0.7f};     
    roomLight.Direction = {0.0f, 0.0f, 0.0f};    
    roomLight.FalloffStart = 2.0f;                
    roomLight.FalloffEnd = 8.0f;                 
    roomLight.Position = {0.0f, 3.0f, 0.0f};     
    roomLight.SpotPower = 0.0f;                  

    AddLight("RoomLight", roomLight, LightType::Point);

    LightData flashLight;
    flashLight.Strength = {1.0f, 1.0f, 0.9f};    
    flashLight.Direction = {0.0f, -0.5f, 1.0f};  
    flashLight.FalloffStart = 1.0f;              
    flashLight.FalloffEnd = 10.0f;               
    flashLight.Position = {0.0f, 1.5f, 0.0f};    
    flashLight.SpotPower = 16.0f;                

    AddLight("FlashLight", flashLight, LightType::Spot);
}

void LightingSystem::SetLightPosition(const std::string& name, const XMFLOAT3& position)
{
    auto* light = GetLight(name);
    if (light)
    {
        light->Position = position;
        m_needsUpdate = true;
    }
}

void LightingSystem::SetLightDirection(const std::string& name, const XMFLOAT3& direction)
{
    auto* light = GetLight(name);
    if (light)
    {
        light->Direction = direction;
        m_needsUpdate = true;
    }
}

void LightingSystem::SetLightColor(const std::string& name, const XMFLOAT3& color)
{
    auto* light = GetLight(name);
    if (light)
    {
        light->Strength = color;
        m_needsUpdate = true;
    }
}

void LightingSystem::SetLightRange(const std::string& name, float range)
{
    auto* light = GetLight(name);
    if (light)
    {
        light->FalloffStart = range * 0.1f;
        light->FalloffEnd = range;
        m_needsUpdate = true;
    }
}

void LightingSystem::SetLightEnabled(const std::string& name, bool enabled)
{
    auto* light = GetLight(name);
    if (light)
    {
        if (enabled)
        {
            LightType lightType = GetLightType(name);
            if (lightType == LightType::Point)
            {
                light->Strength = XMFLOAT3(1.0f, 0.9f, 0.7f);
            }
            else if (lightType == LightType::Spot)
            {
                light->Strength = XMFLOAT3(1.0f, 1.0f, 0.9f);
            }
            else if (lightType == LightType::Directional)
            {
                light->Strength = XMFLOAT3(1.2f, 1.0f, 0.8f);
            }
        }
        else
        {
            light->Strength = XMFLOAT3(0.0f, 0.0f, 0.0f);
        }
        m_needsUpdate = true;
    }
}

const LightData* LightingSystem::GetLight(const std::string& name) const
{
    auto it = m_lightNameToIndex.find(name);
    if (it != m_lightNameToIndex.end() && it->second < m_lights.size())
    {
        return &m_lights[it->second]->data;
    }
    return nullptr;
}
LightData* LightingSystem::GetLight(const std::string& name)
{
	auto it = m_lightNameToIndex.find(name);
	if (it != m_lightNameToIndex.end() && it->second < m_lights.size())
	{
		return &m_lights[it->second]->data;
	}
	return nullptr;
}

uint32_t LightingSystem::GetLightIndex(const std::string& name) const
{
    auto it = m_lightNameToIndex.find(name);
    return (it != m_lightNameToIndex.end()) ? it->second : 0;
}

bool LightingSystem::UpdateLightData(BasicConstants& basicConstants)
{
    if (!m_needsUpdate) return false;

	// FIXME
    for (size_t i = 0; i < m_lights.size(); ++i)
    {
        basicConstants.lights[i] = m_lights[i]->data;
    }

    basicConstants.ambientLight = m_ambientLight;

    m_needsUpdate = false;
    return true;
}

void LightingSystem::SetAmbientLight(const XMFLOAT4& ambientLight)
{
    m_ambientLight = ambientLight;
    m_needsUpdate = true;
}

vector<int> LightingSystem::GetLightIndices() const
{
    vector<int> indices;
    for (const auto& light : m_lights)
    {
        indices.push_back(light->index);
    }
    return indices;
}

LightType LightingSystem::GetLightType(const std::string& name) const
{
	uint32_t index = GetLightIndex(name);
	return m_lights[index]->lightType;
}

void LightingSystem::AddLight(const string& name, const LightData& data, LightType type)
{
    LightEntry lightEntry = {
        type,
        data,
        name,
        0,
        true
    };
    m_lightNameToIndex[name] = m_lights.size();
    m_lights.push_back(make_unique<LightEntry>(lightEntry));
}
}
