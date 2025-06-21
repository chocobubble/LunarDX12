#pragma once
#include "Geometry.h"

namespace Lunar
{
class Plane : public Geometry
{
public:
    Plane(float width = 1.0f, float height = 1.0f, int widthSegments = 1, int heightSegments = 1);
    ~Plane() = default;
    
    void CreateGeometry() override;
    
    void SetDimensions(float width, float height);
    float GetWidth() const { return m_width; }
    float GetHeight() const { return m_height; }
    
private:
    float m_width;
    float m_height;
    int m_widthSegments;
    int m_heightSegments;
    
    void CreatePlaneVertices();
    void CreatePlaneIndices();
};
}
