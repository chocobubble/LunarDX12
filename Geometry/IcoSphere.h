#pragma once
#include <unordered_map>

#include "Geometry.h"

namespace Lunar
{
class IcoSphere : public Geometry
{
public:
    IcoSphere() = default;
    ~IcoSphere() = default;
    
    void     CreateGeometry() override;
    uint16_t GetMiddlePoint(UINT p1, UINT p2);
    void     Subdivide();
    void     CalculateNormals();
    void     CalculateTexCoords();
    void     CalculateColors();
    void     SetSubDivisionLevel(int subdivisionLevel) { m_subdivisionLevel = subdivisionLevel; }
    
private:
	// struct for pair hashing
	struct PairHash {
		size_t operator()(const std::pair<uint16_t,uint16_t>& p) const noexcept {
			return (size_t(p.first) << 16) | p.second;
		}
	};
    std::unordered_map<std::pair<UINT, UINT>, UINT, PairHash> m_middlePointCache;
    int                                                       m_subdivisionLevel = 2;
	
};
} // namespace Luanr
