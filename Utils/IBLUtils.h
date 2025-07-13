#pragma once 
#include <DirectXMath.h>
#include <intsafe.h>
#include <vector>

namespace Lunar
{
class IBLUtils
{
public:
static std::vector<std::vector<float>> GenerateIrradianceMap(const std::vector<std::vector<float>>& cubemapData, int cubemapSize, int irradianceMapSize, int channels = 3);
static DirectX::XMVECTOR GetCubemapDirection(int faceIndex, float u, float v);
static DirectX::XMVECTOR SampleCubemap(const std::vector<std::vector<float>>& cubemapData, UINT size, const DirectX::XMVECTOR& direction, int channels = 3);
static DirectX::XMVECTOR SampleEquirectangular(const float* imageData, UINT width, UINT height, const DirectX::XMVECTOR& direction, int channels = 3);
};
} // namespace Lunar