#pragma once
#include <vector>
#include <string>
#include <memory>

namespace Lunar
{

struct HDRImage
{
    int width;
    int height;
    int channels;
    std::vector<float> data; // RGB(A) float data
    
    HDRImage() : width(0), height(0), channels(0) {}
    
    // Get pixel at (x, y) - returns RGB as float3
    void GetPixel(int x, int y, float* rgb) const
    {
        if (x < 0 || x >= width || y < 0 || y >= height) return;
        
        int index = (y * width + x) * channels;
        rgb[0] = data[index];     // R
        rgb[1] = data[index + 1]; // G  
        rgb[2] = data[index + 2]; // B
    }
    
    // Set pixel at (x, y)
    void SetPixel(int x, int y, const float* rgb)
    {
        if (x < 0 || x >= width || y < 0 || y >= height) return;
        
        int index = (y * width + x) * channels;
        data[index] = rgb[0];     // R
        data[index + 1] = rgb[1]; // G
        data[index + 2] = rgb[2]; // B
    }
};

class HDRLoader
{
public:
    // Load HDR file (Radiance .hdr format)
    static std::unique_ptr<HDRImage> LoadHDR(const std::string& filename);
    
    // Convert equirectangular HDR to cubemap faces
    static std::vector<std::unique_ptr<HDRImage>> EquirectangularToCubemap(
        const HDRImage& equirectangular, 
        int cubemapSize = 512
    );
    
    // Generate diffuse irradiance map from environment cubemap
    static std::vector<std::unique_ptr<HDRImage>> GenerateIrradianceMap(
        const std::vector<std::unique_ptr<HDRImage>>& environmentCubemap,
        int irradianceSize = 64
    );
    
    // Generate prefiltered environment map for specular IBL
    static std::vector<std::vector<std::unique_ptr<HDRImage>>> GeneratePrefilterMap(
        const std::vector<std::unique_ptr<HDRImage>>& environmentCubemap,
        int prefilterSize = 256,
        int mipLevels = 5
    );

private:
    // Helper functions for cubemap conversion
    static void EquirectangularToDirection(float u, float v, float* direction);
    static void DirectionToEquirectangular(const float* direction, float& u, float& v);
    static void GetCubemapDirection(int face, float u, float v, float* direction);
    
    // Sampling functions for IBL generation
    static void SampleHemisphere(float u1, float u2, float* direction);
    static void SampleGGX(float u1, float u2, float roughness, const float* normal, float* direction);
    static float DistributionGGX(const float* normal, const float* halfway, float roughness);
};

} // namespace Lunar
