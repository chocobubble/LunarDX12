#include "IBLUtils.h"

#include <random>

#include "Logger.h"

using namespace std;
using namespace DirectX;

namespace Lunar
{
vector<vector<float>> IBLUtils::GenerateIrradianceMap(const vector<vector<float>>& cubemapData, int cubemapSize, int irradianceMapSize, int channels)
{
	LOG_FUNCTION_ENTRY();
    vector<vector<float>> irradianceMap(6);

    const int sampleCount = 128; // Number of samples for Monte Carlo integration
    std::random_device rd;
    std::mt19937 generator(rd());
    std::uniform_real_distribution<float> distribution(0.0f, 1.0f);

    for (int face = 0; face < 6; ++face)
    {
        irradianceMap[face].resize(irradianceMapSize * irradianceMapSize * channels);

        for (int y = 0; y < irradianceMapSize; ++y)
        {
            for (int x = 0; x < irradianceMapSize; ++x)
            {
                float u = (x + 0.5f) / irradianceMapSize * 2.0f - 1.0f; // Convert to [-1, 1]
                float v = (y + 0.5f) / irradianceMapSize * 2.0f - 1.0f;

                XMVECTOR normal = GetCubemapDirection(face, u, v);

                XMVECTOR up = abs(XMVectorGetY(normal)) < 0.999f ?
                              XMVectorSet(0, 1, 0, 0) : XMVectorSet(1, 0, 0, 0); // To avoid division by zero
                XMVECTOR tangent = XMVector3Normalize(XMVector3Cross(up, normal));
                XMVECTOR bitangent = XMVector3Cross(normal, tangent);

                XMVECTOR irradiance = {0.0f, 0.0f, 0.0f};

                for (int i = 0; i < sampleCount; ++i)
                {
                    float xi1 = distribution(generator);
                    float xi2 = distribution(generator);

                    // Cosine-weighted hemisphere sampling
                    float cosTheta = sqrtf(xi1);
                    float sinTheta = sqrtf(1.0f - xi1);
                    float phi = 2.0f * XM_PI * xi2;

                    XMVECTOR localSample = {
                        sinTheta * cosf(phi),
                        cosTheta,
                        sinTheta * sinf(phi),
                        0.0f
                    };

                    XMVECTOR worldSample = XMVectorGetX(localSample) * tangent +
                                          XMVectorGetY(localSample) * normal +
                                          XMVectorGetZ(localSample) * bitangent;

                    irradiance = XMVectorAdd(irradiance, SampleCubemap(cubemapData, cubemapSize, worldSample, channels));
                }

                irradiance = XMVectorScale(irradiance, (XM_PI / sampleCount)); 

                int dstIndex = (y * irradianceMapSize + x) * channels;
                irradianceMap[face][dstIndex] = XMVectorGetX(irradiance);
                irradianceMap[face][dstIndex + 1] = XMVectorGetY(irradiance);
                irradianceMap[face][dstIndex + 2] = XMVectorGetZ(irradiance);
            }
        }
    }

	LOG_FUNCTION_EXIT();
    return irradianceMap;
}

XMVECTOR IBLUtils::GetCubemapDirection(int faceIndex, float u, float v)
{
    switch (faceIndex)
    {
	    case 0: // +X
    		return XMVector3Normalize({1.0f, -v, -u}); // +X
    	case 1: // -X
    		return XMVector3Normalize({-1.0f, -v, u}); // -X
    	case 2: // +Y
    		return XMVector3Normalize({u, 1.0f, v}); // +Y
    	case 3: // -Y
    		return XMVector3Normalize({u, -1.0f, -v}); // -Y
        case 4: // +Z
            return XMVector3Normalize({u, -v, 1.0f}); // +Z
        case 5: // -Z
            return XMVector3Normalize({-u, -v, -1.0f}); // -Z
        default:
            LOG_ERROR("Invalid cubemap face index: ", faceIndex);
            return {0.0f, 0.0f, 0.0f, 0.0f};
    }
}

XMVECTOR IBLUtils::SampleCubemap(const vector<vector<float>>& cubemapData, UINT size, const XMVECTOR& direction, int channels)
{
    float directionX = XMVectorGetX(direction);
    float directionY = XMVectorGetY(direction);
    float directionZ = XMVectorGetZ(direction);
    float absX = abs(directionX);
    float absY = abs(directionY);
    float absZ = abs(directionZ);

    int face;
    float u, v;
    // choose the face based on the largest component
    if (absX >= absY && absX >= absZ) {
        face = (directionX > 0) ? 0 : 1; // +X
        u = (-directionZ / absX + 1.0f) * 0.5f; // [-1, 1] to [0, 1]
        v = (-directionY / absX + 1.0f) * 0.5f;
        (directionX > 0) ? u = u : u = -u; // Flip u for -X face
    } else if (absY >= absX && absY >= absZ) {
        face = (directionY > 0) ? 2 : 3; // +Y
        u = (directionX / absY + 1.0f) * 0.5f;
        v = (directionZ / absY + 1.0f) * 0.5f;
        (directionY > 0) ? v = v : v = -v; 
    } else {
        face = (directionZ > 0) ? 4 : 5; // +Z
        u = (directionX / absZ + 1.0f) * 0.5f;
        v = (-directionY / absZ + 1.0f) * 0.5f;
        (directionZ > 0) ? u = u : u = -u; 
    }

    int x = static_cast<int>(u * (size - 1));
    int y = static_cast<int>(v * (size - 1));
    x = std::clamp(x, 0, static_cast<int>(size) - 1);
    y = std::clamp(y, 0, static_cast<int>(size) - 1);

    int index = (y * size + x) * channels;
    return XMVectorSet(cubemapData[face][index], cubemapData[face][index + 1], cubemapData[face][index + 2], 0.0f);
}

XMVECTOR IBLUtils::SampleEquirectangular(const float* imageData, UINT width, UINT height, const XMVECTOR& direction, int channels)
{
    float x = XMVectorGetX(direction); 
    float y = XMVectorGetY(direction); 
    float z = XMVectorGetZ(direction); 

    float theta = atan2f(z, x); // polar angle
    float phi = asinf(y); // azimuthal angle

    float u = (theta + phi) / (2.0f * XM_PI);
    float v = 0.5f - phi / XM_PI;

    int pixelX = static_cast<int>(u * (width - 1));
    int pixelY = static_cast<int>(v * (height - 1));

    pixelX = std::clamp(pixelX, 0, static_cast<int>(width) - 1);
    pixelY = std::clamp(pixelY, 0, static_cast<int>(height) - 1);

    int index = (pixelY * width + pixelX) * channels; 
    return XMVectorSet(imageData[index], imageData[index + 1], imageData[index + 2], 0.0f);
}


} // namespace Lunar