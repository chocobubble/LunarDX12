#include "GeometryFactory.h"
#include "Cube.h"
#include "Sphere.h"
#include "Plane.h"
#include <algorithm>
#include <cctype>

namespace Lunar
{

std::unique_ptr<Geometry> GeometryFactory::CreatePlane(float width, float height, int widthSegments, int heightSegments)
{
    return std::make_unique<Plane>(width, height, widthSegments, heightSegments);
}

std::unique_ptr<Geometry> GeometryFactory::CreateGeometry(GeometryType type)
{
    switch (type)
    {
    case GeometryType::Sphere:
        return CreateSphere();
    case GeometryType::Plane:
        return CreatePlane();
    default:
        return CreateCube(); 
    }
}

std::unique_ptr<Geometry> GeometryFactory::CreateGeometry(const std::string& typeName)
{
    GeometryType type = StringToGeometryType(typeName);
    return CreateGeometry(type);
}

std::string GeometryFactory::GeometryTypeToString(GeometryType type)
{
    switch (type)
    {
    case GeometryType::Sphere:
        return "Sphere";
    case GeometryType::Plane:
        return "Plane";
    default:
        return "Unknown";
    }
}

GeometryType GeometryFactory::StringToGeometryType(const std::string& typeName)
{
    std::string lowerTypeName = typeName;
    std::transform(lowerTypeName.begin(), lowerTypeName.end(), lowerTypeName.begin(),
        [](unsigned char c) { return std::tolower(c); });

    if (lowerTypeName == "sphere")
        return GeometryType::Sphere;
    else if (lowerTypeName == "plane")
        return GeometryType::Plane;
    else
        return GeometryType::Cube; 
}
}
