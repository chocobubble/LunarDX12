#include "HDRLoader.h"
#include <fstream>
#include <sstream>
#include <cmath>
#include <algorithm>
#include <random>

namespace Lunar
{

std::unique_ptr<HDRImage> HDRLoader::LoadHDR(const std::string& filename)
{
    std::ifstream file(filename, std::ios::binary);
    if (!file.is_open()) {
        return nullptr;
    }
    
    auto image = std::make_unique<HDRImage>();
    std::string line;
    
    // Read header
    bool foundFormat = false;
    while (std::getline(file, line)) {
        if (line.find("#?RADIANCE") != std::string::npos || 
            line.find("#?RGBE") != std::string::npos) {
            foundFormat = true;
        }
        if (line.empty()) break; // End of header
    }
    
    if (!foundFormat) return nullptr;
    
    // Read resolution line (e.g., "-Y 512 +X 1024")
    std::getline(file, line);
    std::istringstream iss(line);
    std::string token;
    
    iss >> token; // -Y
    iss >> image->height;
    iss >> token; // +X  
    iss >> image->width;
    
    image->channels = 3; // RGB
    image->data.resize(image->width * image->height * image->channels);
    
    // Read RGBE data
    std::vector<unsigned char> rgbe(image->width * 4); // RGBE format
    
    for (int y = 0; y < image->height; ++y) {
        file.read(reinterpret_cast<char*>(rgbe.data()), image->width * 4);
        
        for (int x = 0; x < image->width; ++x) {
            int rgbeIndex = x * 4;
            int floatIndex = (y * image->width + x) * 3;
            
            unsigned char r = rgbe[rgbeIndex];
            unsigned char g = rgbe[rgbeIndex + 1]; 
            unsigned char b = rgbe[rgbeIndex + 2];
            unsigned char e = rgbe[rgbeIndex + 3];
            
            if (e == 0) {
                // Zero exponent
                image->data[floatIndex] = 0.0f;
                image->data[floatIndex + 1] = 0.0f;
                image->data[floatIndex + 2] = 0.0f;
            } else {
                // Convert RGBE to float
                float scale = ldexp(1.0f, e - 128 - 8);
                image->data[floatIndex] = r * scale;
                image->data[floatIndex + 1] = g * scale;
                image->data[floatIndex + 2] = b * scale;
            }
        }
    }
    
    return image;
}

std::vector<std::unique_ptr<HDRImage>> HDRLoader::EquirectangularToCubemap(
    const HDRImage& equirectangular, int cubemapSize)
{
    std::vector<std::unique_ptr<HDRImage>> cubemap(6);
    
    // Create 6 faces
    for (int face = 0; face < 6; ++face) {
        cubemap[face] = std::make_unique<HDRImage>();
        cubemap[face]->width = cubemapSize;
        cubemap[face]->height = cubemapSize;
        cubemap[face]->channels = 3;
        cubemap[face]->data.resize(cubemapSize * cubemapSize * 3);
        
        for (int y = 0; y < cubemapSize; ++y) {
            for (int x = 0; x < cubemapSize; ++x) {
                // Convert cubemap coordinates to direction
                float u = (x + 0.5f) / cubemapSize * 2.0f - 1.0f;
                float v = (y + 0.5f) / cubemapSize * 2.0f - 1.0f;
                
                float direction[3];
                GetCubemapDirection(face, u, v, direction);
                
                // Convert direction to equirectangular coordinates
                float equiU, equiV;
                DirectionToEquirectangular(direction, equiU, equiV);
                
                // Sample from equirectangular image
                int equiX = static_cast<int>(equiU * equirectangular.width);
                int equiY = static_cast<int>(equiV * equirectangular.height);
                
                equiX = std::clamp(equiX, 0, equirectangular.width - 1);
                equiY = std::clamp(equiY, 0, equirectangular.height - 1);
                
                float rgb[3];
                equirectangular.GetPixel(equiX, equiY, rgb);
                cubemap[face]->SetPixel(x, y, rgb);
            }
        }
    }
    
    return cubemap;
}

std::vector<std::unique_ptr<HDRImage>> HDRLoader::GenerateIrradianceMap(
    const std::vector<std::unique_ptr<HDRImage>>& environmentCubemap,
    int irradianceSize)
{
    std::vector<std::unique_ptr<HDRImage>> irradianceMap(6);
    
    const int sampleCount = 1024; // Number of samples for Monte Carlo integration
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<float> dis(0.0f, 1.0f);
    
    for (int face = 0; face < 6; ++face) {
        irradianceMap[face] = std::make_unique<HDRImage>();
        irradianceMap[face]->width = irradianceSize;
        irradianceMap[face]->height = irradianceSize;
        irradianceMap[face]->channels = 3;
        irradianceMap[face]->data.resize(irradianceSize * irradianceSize * 3);
        
        for (int y = 0; y < irradianceSize; ++y) {
            for (int x = 0; x < irradianceSize; ++x) {
                float u = (x + 0.5f) / irradianceSize * 2.0f - 1.0f;
                float v = (y + 0.5f) / irradianceSize * 2.0f - 1.0f;
                
                float normal[3];
                GetCubemapDirection(face, u, v, normal);
                
                float irradiance[3] = {0.0f, 0.0f, 0.0f};
                
                // Monte Carlo integration over hemisphere
                for (int i = 0; i < sampleCount; ++i) {
                    float u1 = dis(gen);
                    float u2 = dis(gen);
                    
                    float sampleDir[3];
                    SampleHemisphere(u1, u2, sampleDir);
                    
                    // Transform sample direction to world space (align with normal)
                    // Simplified: assume normal is already in correct space
                    float dot = std::max(0.0f, sampleDir[0] * normal[0] + 
                                              sampleDir[1] * normal[1] + 
                                              sampleDir[2] * normal[2]);
                    
                    if (dot > 0.0f) {
                        // Sample environment map
                        // This is simplified - in practice you'd need proper sampling
                        float rgb[3] = {1.0f, 1.0f, 1.0f}; // Placeholder
                        
                        irradiance[0] += rgb[0] * dot;
                        irradiance[1] += rgb[1] * dot;
                        irradiance[2] += rgb[2] * dot;
                    }
                }
                
                // Average and apply pi factor
                float scale = 3.14159f / sampleCount;
                irradiance[0] *= scale;
                irradiance[1] *= scale;
                irradiance[2] *= scale;
                
                irradianceMap[face]->SetPixel(x, y, irradiance);
            }
        }
    }
    
    return irradianceMap;
}

void HDRLoader::GetCubemapDirection(int face, float u, float v, float* direction)
{
    switch (face) {
        case 0: // +X
            direction[0] = 1.0f;
            direction[1] = -v;
            direction[2] = -u;
            break;
        case 1: // -X
            direction[0] = -1.0f;
            direction[1] = -v;
            direction[2] = u;
            break;
        case 2: // +Y
            direction[0] = u;
            direction[1] = 1.0f;
            direction[2] = v;
            break;
        case 3: // -Y
            direction[0] = u;
            direction[1] = -1.0f;
            direction[2] = -v;
            break;
        case 4: // +Z
            direction[0] = u;
            direction[1] = -v;
            direction[2] = 1.0f;
            break;
        case 5: // -Z
            direction[0] = -u;
            direction[1] = -v;
            direction[2] = -1.0f;
            break;
    }
    
    // Normalize
    float length = sqrt(direction[0] * direction[0] + 
                       direction[1] * direction[1] + 
                       direction[2] * direction[2]);
    direction[0] /= length;
    direction[1] /= length;
    direction[2] /= length;
}

void HDRLoader::DirectionToEquirectangular(const float* direction, float& u, float& v)
{
    float theta = atan2(direction[2], direction[0]);
    float phi = asin(direction[1]);
    
    u = (theta + 3.14159f) / (2.0f * 3.14159f);
    v = (phi + 3.14159f / 2.0f) / 3.14159f;
}

void HDRLoader::SampleHemisphere(float u1, float u2, float* direction)
{
    float cosTheta = sqrt(1.0f - u1);
    float sinTheta = sqrt(u1);
    float phi = 2.0f * 3.14159f * u2;
    
    direction[0] = sinTheta * cos(phi);
    direction[1] = cosTheta;
    direction[2] = sinTheta * sin(phi);
}

// Placeholder implementations for prefilter map generation
std::vector<std::vector<std::unique_ptr<HDRImage>>> HDRLoader::GeneratePrefilterMap(
    const std::vector<std::unique_ptr<HDRImage>>& environmentCubemap,
    int prefilterSize, int mipLevels)
{
    // Simplified implementation - in practice this would be much more complex
    std::vector<std::vector<std::unique_ptr<HDRImage>>> prefilterMap(mipLevels);
    
    for (int mip = 0; mip < mipLevels; ++mip) {
        int mipSize = prefilterSize >> mip;
        prefilterMap[mip].resize(6);
        
        for (int face = 0; face < 6; ++face) {
            prefilterMap[mip][face] = std::make_unique<HDRImage>();
            prefilterMap[mip][face]->width = mipSize;
            prefilterMap[mip][face]->height = mipSize;
            prefilterMap[mip][face]->channels = 3;
            prefilterMap[mip][face]->data.resize(mipSize * mipSize * 3, 1.0f);
        }
    }
    
    return prefilterMap;
}

void HDRLoader::SampleGGX(float u1, float u2, float roughness, const float* normal, float* direction)
{
    // Placeholder implementation
    direction[0] = normal[0];
    direction[1] = normal[1];
    direction[2] = normal[2];
}

float HDRLoader::DistributionGGX(const float* normal, const float* halfway, float roughness)
{
    // Placeholder implementation
    return 1.0f;
}

} // namespace Lunar
