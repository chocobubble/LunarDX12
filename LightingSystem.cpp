#include "LightingSystem.h"

#include "ConstantBuffers.h"
#include "LunarConstants.h"
#include "Utils.h"

using namespace DirectX;
using namespace std;
using namespace Lunar::LunarConstants;

namespace Lunar
{
void LightingSystem::Initialize(ID3D12Device* device, UINT maxLights)
{
    m_maxLights = maxLights;
	for (auto& lightInfo : LIGHT_INFO)
	{
		LightData lightData;
		lightData.Direction = XMFLOAT3(lightInfo.direction);
		lightData.Strength = XMFLOAT3(lightInfo.strength);
		lightData.Position = XMFLOAT3(lightInfo.position);
		lightData.FalloffEnd = lightInfo.fallOffEnd;
		lightData.FalloffStart = lightInfo.fallOffStart;
		lightData.SpotPower = lightInfo.spotPower;
		AddLight(lightInfo.name, lightData, lightInfo.lightType);	
	}
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
