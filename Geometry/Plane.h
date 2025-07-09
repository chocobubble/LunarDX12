#pragma once
#include "Geometry.h"

namespace Lunar
{
class Plane : public Geometry
{
public:
    Plane(int widthSegments = 5, int depthSegments = 5);
    ~Plane() = default;
    
    void CreateGeometry() override;

	DirectX::XMFLOAT4 GetPlaneEquation();
    
private:
    int m_widthSegments;
    int m_depthSegments;
	DirectX::XMFLOAT4 m_planeEquation;
    
    void CreatePlaneVertices();
    void CreatePlaneIndices();
	void CalculatePlaneEquation();
};
}
