#include "HDRTestGenerator.h"
#include <cmath>
#include <algorithm>

namespace Lunar
{

struct float3 {
    float x, y, z;
    float3() : x(0), y(0), z(0) {}
    float3(float x_, float y_, float z_) : x(x_), y(y_), z(z_) {}
};

std::unique_ptr<HDRImage> HDRTestGenerator::GenerateTestEnvironment(int width, int height)
{
    auto image = std::make_unique<HDRImage>();
    image->width = width;
    image->height = height;
    image->channels = 3;
    image->data.resize(width * height * 3);
    
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            float u = static_cast<float>(x) / width;
            float v = static_cast<float>(y) / height;
            
            // Convert to spherical coordinates
            float theta = u * 2.0f * 3.14159f; // Longitude
            float phi = v * 3.14159f;          // Latitude
            
            // Sky gradient
            float skyIntensity = SampleSky(u, v);
            
            // Add sun
            float sunIntensity = SampleSun(u, v, 0.75f, 0.3f, 0.05f);
            
            // Combine
            float totalIntensity = skyIntensity + sunIntensity;
            
            // Sky color (blue to white gradient)
            float3 skyColor(0.5f + 0.5f * (1.0f - v), 0.7f + 0.3f * (1.0f - v), 1.0f);
            float3 sunColor(1.0f, 0.9f, 0.8f);
            
            float3 finalColor;
            if (sunIntensity > 0.1f) {
                // Sun area
                finalColor.x = sunColor.x * totalIntensity;
                finalColor.y = sunColor.y * totalIntensity;
                finalColor.z = sunColor.z * totalIntensity;
            } else {
                // Sky area
                finalColor.x = skyColor.x * skyIntensity;
                finalColor.y = skyColor.y * skyIntensity;
                finalColor.z = skyColor.z * skyIntensity;
            }
            
            int index = (y * width + x) * 3;
            image->data[index] = finalColor.x;
            image->data[index + 1] = finalColor.y;
            image->data[index + 2] = finalColor.z;
        }
    }
    
    return image;
}

std::unique_ptr<HDRImage> HDRTestGenerator::GenerateSkyGradient(int width, int height)
{
    auto image = std::make_unique<HDRImage>();
    image->width = width;
    image->height = height;
    image->channels = 3;
    image->data.resize(width * height * 3);
    
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            float u = static_cast<float>(x) / width;
            float v = static_cast<float>(y) / height;
            
            // Simple sky gradient
            float intensity = 0.5f + 1.5f * (1.0f - v); // Brighter at horizon
            
            // Sky colors
            float r = 0.5f + 0.5f * (1.0f - v);  // More red at horizon
            float g = 0.7f + 0.3f * (1.0f - v);  // More green at horizon  
            float b = 1.0f;                      // Always blue
            
            int index = (y * width + x) * 3;
            image->data[index] = r * intensity;
            image->data[index + 1] = g * intensity;
            image->data[index + 2] = b * intensity;
        }
    }
    
    return image;
}

float HDRTestGenerator::SampleSky(float u, float v)
{
    // Simple sky intensity based on elevation
    return 0.5f + 1.0f * (1.0f - v); // Brighter near horizon
}

float HDRTestGenerator::SampleSun(float u, float v, float sunU, float sunV, float sunSize)
{
    float du = u - sunU;
    float dv = v - sunV;
    float distance = sqrt(du * du + dv * dv);
    
    if (distance < sunSize) {
        // Sun disk
        float falloff = 1.0f - (distance / sunSize);
        return 10.0f * falloff * falloff; // Bright sun
    }
    
    return 0.0f;
}

bool HDRTestGenerator::SaveHDR(const HDRImage& image, const std::string& filename)
{
    // This is a simplified implementation
    // In practice, you'd want to save in proper HDR format
    return true; // Placeholder
}

} // namespace Lunar
