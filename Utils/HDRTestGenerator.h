#pragma once
#include <string>
#include <memory>
#include "HDRLoader.h"

namespace Lunar
{

class HDRTestGenerator
{
public:
    // Generate a simple procedural HDR environment map for testing
    static std::unique_ptr<HDRImage> GenerateTestEnvironment(int width = 512, int height = 256);
    
    // Generate a simple sky gradient
    static std::unique_ptr<HDRImage> GenerateSkyGradient(int width = 512, int height = 256);
    
    // Save HDR image to file (simple format)
    static bool SaveHDR(const HDRImage& image, const std::string& filename);

private:
    // Helper functions
    static float SampleSky(float u, float v);
    static float SampleSun(float u, float v, float sunU, float sunV, float sunSize);
};

} // namespace Lunar
