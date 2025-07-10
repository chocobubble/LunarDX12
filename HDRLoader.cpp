#include "IBLManager.h"
#include "Logger.h"
#include <fstream>
#include <sstream>
#include <cstring>

namespace Lunar
{

bool HDRLoader::LoadHDR(const std::string& filename, HDRImage& image)
{
    std::string extension = filename.substr(filename.find_last_of('.'));
    std::transform(extension.begin(), extension.end(), extension.begin(), ::tolower);
    
    if (extension == ".hdr")
    {
        return LoadRadianceHDR(filename, image);
    }
    else if (extension == ".exr")
    {
        return LoadEXR(filename, image);
    }
    
    LOG_ERROR("Unsupported HDR format: %s", extension.c_str());
    return false;
}

bool HDRLoader::LoadRadianceHDR(const std::string& filename, HDRImage& image)
{
    FILE* file = nullptr;
    fopen_s(&file, filename.c_str(), "rb");
    if (!file)
    {
        LOG_ERROR("Failed to open HDR file: %s", filename.c_str());
        return false;
    }
    
    int width, height;
    if (!ReadHeader(file, width, height))
    {
        fclose(file);
        return false;
    }
    
    image.width = width;
    image.height = height;
    image.channels = 3;
    image.pixels.resize(width * height * 3);
    
    if (!ReadPixels(file, image))
    {
        fclose(file);
        return false;
    }
    
    fclose(file);
    LOG_INFO("Loaded HDR image: %s (%dx%d)", filename.c_str(), width, height);
    return true;
}

bool HDRLoader::ReadHeader(FILE* file, int& width, int& height)
{
    char line[256];
    
    if (!fgets(line, sizeof(line), file) || strncmp(line, "#?RADIANCE", 10) != 0)
    {
        LOG_ERROR("Invalid HDR header");
        return false;
    }
    
    while (fgets(line, sizeof(line), file))
    {
        if (line[0] == '\n') break;
    }
    
    if (!fgets(line, sizeof(line), file))
    {
        LOG_ERROR("Failed to read HDR dimensions");
        return false;
    }
    
    if (sscanf_s(line, "-Y %d +X %d", &height, &width) != 2)
    {
        LOG_ERROR("Failed to parse HDR dimensions");
        return false;
    }
    
    return true;
}

bool HDRLoader::ReadPixels(FILE* file, HDRImage& image)
{
    std::vector<unsigned char> scanline(image.width * 4);
    
    for (int y = 0; y < image.height; ++y)
    {
        if (fread(scanline.data(), 4, image.width, file) != image.width)
        {
            LOG_ERROR("Failed to read HDR scanline %d", y);
            return false;
        }
        
        float* output = image.GetPixel(0, y);
        ConvertScanline(output, scanline.data(), image.width);
    }
    
    return true;
}

void HDRLoader::ConvertScanline(float* output, unsigned char* input, int width)
{
    for (int x = 0; x < width; ++x)
    {
        unsigned char* rgbe = &input[x * 4];
        float value = ConvertRGBE(rgbe);
        
        output[x * 3 + 0] = rgbe[0] * value;
        output[x * 3 + 1] = rgbe[1] * value;
        output[x * 3 + 2] = rgbe[2] * value;
    }
}

float HDRLoader::ConvertRGBE(unsigned char rgbe[4])
{
    if (rgbe[3] == 0)
        return 0.0f;
    
    return ldexp(1.0f, rgbe[3] - 128);
}

bool HDRLoader::LoadEXR(const std::string& filename, HDRImage& image)
{
    LOG_WARNING("EXR loading not implemented yet");
    return false;
}

} // namespace Lunar
