#pragma once
#include "Geometry.h"

namespace Lunar
{
	
class Tree : public Geometry
{
public:
	void CreateGeometry() override;
	void Draw(ID3D12GraphicsCommandList* commandList) override;	
};
} // namespace Lunar 
