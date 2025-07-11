#pragma once
#include <vector>
#include <string>
#include <memory>

namespace Lunar
{

// 단순화된 HDR 이미지 구조체
struct HDRImage
{
    int width;
    int height;
    int channels;
    std::vector<float> data; // RGB float data (stb_image 호환)
    
    HDRImage() : width(0), height(0), channels(0) {}
    
    // stb_image 데이터로부터 생성
    static std::unique_ptr<HDRImage> FromSTB(float* stbData, int w, int h, int c)
    {
        auto image = std::make_unique<HDRImage>();
        image->width = w;
        image->height = h;
        image->channels = c;
        
        size_t dataSize = w * h * c;
        image->data.resize(dataSize);
        std::memcpy(image->data.data(), stbData, dataSize * sizeof(float));
        
        return image;
    }
};

class HDRLoader
{
public:
    // stb_image 기반 HDR 로딩
    static std::unique_ptr<HDRImage> LoadHDR(const std::string& filename)
    {
        int width, height, channels;
        float* data = stbi_loadf(filename.c_str(), &width, &height, &channels, 3);
        
        if (!data) return nullptr;
        
        auto image = HDRImage::FromSTB(data, width, height, 3);
        stbi_image_free(data);
        
        return image;
    }
    
    // HDR 파일 체크
    static bool IsHDR(const std::string& filename)
    {
        return stbi_is_hdr(filename.c_str()) != 0;
    }
    
    // 기존 변환 함수들은 그대로 유지
    static std::vector<std::unique_ptr<HDRImage>> EquirectangularToCubemap(
        const HDRImage& equirectangular, 
        int cubemapSize = 512
    );
    
    static std::vector<std::unique_ptr<HDRImage>> GenerateIrradianceMap(
        const std::vector<std::unique_ptr<HDRImage>>& environmentCubemap,
        int irradianceSize = 64
    );

private:
    // 기존 헬퍼 함수들 유지
    static void GetCubemapDirection(int face, float u, float v, float* direction);
    static void SampleHemisphere(float u1, float u2, float* direction);
};

} // namespace Lunar
