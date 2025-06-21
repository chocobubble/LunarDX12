#pragma once
#include "Geometry.h"

namespace Lunar
{
class Icosphere : public Geometry
{
public:
    Icosphere(int subdivisionLevel = 2);
    ~Icosphere() = default;
    
    void CreateGeometry() override;

    float GetWidth() const { return m_width; }
    float GetHeight() const { return m_height; }
    
private:
    int m_subdivisionLevel = 2;
    std::unordered_map<pair<uint16_t, uint16_t>, uint16_t> m_cachedMiddlePoints;
    
    std::tuple<float, float> GetMiddlePoint(float x1, float y1, float x2, float y2) const;
};
}
