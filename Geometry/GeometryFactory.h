#pragma once
#include <memory>
#include <string>
#include "Geometry.h"

namespace Lunar
{
enum class GeometryType : int8_t
{
    Cube,
    Sphere,
	Plane
};

class GeometryFactory
{
public:
    static std::unique_ptr<Geometry> CreateCube();
    static std::unique_ptr<Geometry> CreatePlane(float width = 1.0f, float height = 1.0f, int widthSegments = 1, int heightSegments = 1);
    static std::unique_ptr<Geometry> CreateSphere();
    
    static std::unique_ptr<Geometry> CreateGeometry(GeometryType type);
    
    static std::unique_ptr<Geometry> CreateGeometry(const std::string& typeName);
    
    static std::string GeometryTypeToString(GeometryType type);
    static GeometryType StringToGeometryType(const std::string& typeName);
};
}
