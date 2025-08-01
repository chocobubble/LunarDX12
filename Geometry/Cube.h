#pragma once
#include "Geometry.h"

namespace Lunar
{
class Cube : public Geometry
{
public:
    Cube() = default;
    ~Cube() = default;
    
    void CreateGeometry() override;
    
private:
    void CreateCubeVertices();
    void CreateCubeIndices();
};
}
