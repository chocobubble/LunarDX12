#pragma once
#include <vector>
#include <string>
#include <unordered_map>
#include <memory>
#include <d3d12.h>
#include <DirectXMath.h>

// #include "ConstantBuffers.h"

namespace Lunar
{
	namespace LunarConstants {
		enum class LightType;
	}

	struct BasicConstants;
struct LightData
{
    DirectX::XMFLOAT3 Strength = {1.0f, 1.0f, 1.0f};
    float FalloffStart = 1.0f;
    DirectX::XMFLOAT3 Direction = {0.0f, -1.0f, 0.0f};
    float FalloffEnd = 10.0f;
    DirectX::XMFLOAT3 Position = {0.0f, 0.0f, 0.0f};
    float SpotPower = 64.0f;
};

class LightingSystem
{
public:
    LightingSystem() = default;
    ~LightingSystem() = default;

    void Initialize(ID3D12Device* device, UINT maxLights = 16);

    void SetLightPosition(const std::string& name, const DirectX::XMFLOAT3& position);
    void SetLightDirection(const std::string& name, const DirectX::XMFLOAT3& direction);
    void SetLightColor(const std::string& name, const DirectX::XMFLOAT3& color);
    void SetLightRange(const std::string& name, float range);
    void SetLightEnabled(const std::string& name, bool enabled);

    const LightData* GetLight(const std::string& name) const;
    uint32_t GetLightIndex(const std::string& name) const;

    bool UpdateLightData(BasicConstants& basicConstants);

    void SetAmbientLight(const DirectX::XMFLOAT4& ambientLight);
    DirectX::XMFLOAT4 GetAmbientLight() const { return m_ambientLight; }

    std::vector<int> GetLightIndices() const;

private:
    struct LightEntry
    {
	    LunarConstants::LightType lightType;
        LightData data;
        std::string name;
        uint32_t index;
        bool enabled = true;
    };

    std::vector<std::unique_ptr<LightEntry>> m_lights;
    std::unordered_map<std::string, uint32_t> m_lightNameToIndex;

    // for now, set 1 directional, 1 point, 1 spot
    UINT m_maxLights = 3;
    bool m_needsUpdate = true;
    DirectX::XMFLOAT4 m_ambientLight = {0.1f, 0.1f, 0.1f, 1.0f};

    LunarConstants::LightType GetLightType(const std::string& name) const;
    void                      AddLight(const std::string& name, const LightData& lightData, LunarConstants::LightType type);
    LightData*                GetLight(const std::string& name);
};
}
