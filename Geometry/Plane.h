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
    
    float GetWidth() const { return m_width; }
    float GetHeight() const { return m_height; }

	DirectX::XMFLOAT4 GetPlaneEquation();
    
private:
    float m_width;
    float m_height;
    int m_widthSegments;
    int m_heightSegments;
	DirectX::XMFLOAT4 m_planeEquation;
    
    void CreatePlaneVertices();
    void CreatePlaneIndices();
	void CalculatePlaneEquation();
};
}
