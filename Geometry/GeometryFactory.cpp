#include "GeometryFactory.h"
#include "Cube.h"
#include "IcoSphere.h"
#include "Plane.h"
#include <algorithm>
#include <cctype>

#include "Tree.h"

namespace Lunar
{
std::unique_ptr<Geometry> GeometryFactory::CreateCube()
{
    return std::make_unique<Cube>();
}

std::unique_ptr<Geometry> GeometryFactory::CreatePlane(float width, float height, int widthSegments, int heightSegments)
{
    return std::make_unique<Plane>(width, height, widthSegments, heightSegments);
}

std::unique_ptr<Geometry> GeometryFactory::CreateSphere()
{
    return std::make_unique<IcoSphere>();
}

std::unique_ptr<Geometry> GeometryFactory::CreateTree()
{
	return std::make_unique<Tree>();
}

std::unique_ptr<Geometry> GeometryFactory::CreateGeometry(GeometryType type)
{
    switch (type)
    {
        case GeometryType::Cube:
            return CreateCube();
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

std::unique_ptr<Geometry> GeometryFactory::CloneGeometry(const Geometry* original)
{
	if (auto cube = dynamic_cast<const Cube*>(original))
	{
		return CreateCube();
	}
	else if (auto sphere = dynamic_cast<const IcoSphere*>(original))
	{
		return CreateSphere();
	}
	else if (auto plane = dynamic_cast<const Plane*>(original))
	{
		return CreatePlane(plane->GetWidth(), plane->GetHeight());
	}
	return nullptr;
}

std::string GeometryFactory::GeometryTypeToString(GeometryType type)
{
    switch (type)
    {
        case GeometryType::Cube:
            return "Cube";
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

    if (lowerTypeName == "cube")
        return GeometryType::Cube;
    else if (lowerTypeName == "sphere")
        return GeometryType::Sphere;
    else if (lowerTypeName == "plane")
        return GeometryType::Plane;
    else if (lowerTypeName == "tree")
        return GeometryType::Tree;
    else
        return GeometryType::Cube; 
}
}
