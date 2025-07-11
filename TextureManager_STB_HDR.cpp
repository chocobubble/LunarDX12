// TextureManager.cpp에 추가할 stb_image 기반 HDR 지원

#include <stb_image.h>

namespace Lunar
{

ComPtr<ID3D12Resource> TextureManager::LoadTexture(
    const LunarConstants::TextureInfo& textureInfo, 
    ID3D12Device* device, 
    ID3D12GraphicsCommandList* commandList, 
    const std::string& filename, 
    Microsoft::WRL::ComPtr<ID3D12Resource>& uploadBuffer)
{
    // 기존 코드...
    
    // HDR 파일 체크 및 로딩
    if (stbi_is_hdr(filename.c_str()) || textureInfo.fileType == LunarConstants::FileType::HDR)
    {
        return LoadHDRTexture(filename, device, commandList, uploadBuffer);
    }
    
    // 기존 LDR 로딩 로직...
}

ComPtr<ID3D12Resource> TextureManager::LoadHDRTexture(
    const std::string& filename,
    ID3D12Device* device,
    ID3D12GraphicsCommandList* commandList,
    Microsoft::WRL::ComPtr<ID3D12Resource>& uploadBuffer)
{
    int width, height, channels;
    
    // stb_image로 HDR 로딩 (float 배열로 반환)
    float* hdrData = stbi_loadf(filename.c_str(), &width, &height, &channels, 3); // RGB 강제
    if (!hdrData) {
        LOG_ERROR("Failed to load HDR texture: ", filename);
        throw std::runtime_error("Failed to load HDR texture: " + filename);
    }
    
    LOG_DEBUG("HDR texture loaded: ", filename, " (", width, "x", height, ", ", channels, " channels)");
    
    // HDR 텍스처 디스크립터
    D3D12_RESOURCE_DESC textureDesc = {};
    textureDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    textureDesc.Width = width;
    textureDesc.Height = height;
    textureDesc.DepthOrArraySize = 1;
    textureDesc.MipLevels = 1;
    textureDesc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT; // HDR 포맷
    textureDesc.SampleDesc.Count = 1;
    textureDesc.SampleDesc.Quality = 0;
    textureDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
    textureDesc.Flags = D3D12_RESOURCE_FLAG_NONE;
    
    // Float RGB를 Half-Float RGBA로 변환
    std::vector<uint16_t> halfFloatData(width * height * 4);
    for (int i = 0; i < width * height; ++i) {
        halfFloatData[i * 4] = FloatToHalf(hdrData[i * 3]);     // R
        halfFloatData[i * 4 + 1] = FloatToHalf(hdrData[i * 3 + 1]); // G
        halfFloatData[i * 4 + 2] = FloatToHalf(hdrData[i * 3 + 2]); // B
        halfFloatData[i * 4 + 3] = FloatToHalf(1.0f);               // A
    }
    
    UINT64 rowSizeInBytes = width * 8; // R16G16B16A16 = 8 bytes per pixel
    
    ComPtr<ID3D12Resource> texture = CreateTextureResource(
        device, 
        commandList, 
        textureDesc, 
        reinterpret_cast<const uint8_t*>(halfFloatData.data()),
        rowSizeInBytes,
        uploadBuffer
    );
    
    stbi_image_free(hdrData); // stb_image 메모리 해제
    return texture;
}

// Equirectangular HDR을 Cubemap으로 변환 (stb_image 기반)
ComPtr<ID3D12Resource> TextureManager::CreateHDRCubemap(
    const std::string& hdrFilename,
    ID3D12Device* device,
    ID3D12GraphicsCommandList* commandList,
    int cubemapSize)
{
    // 1. HDR 이미지 로드
    int width, height, channels;
    float* hdrData = stbi_loadf(hdrFilename.c_str(), &width, &height, &channels, 3);
    if (!hdrData) {
        LOG_ERROR("Failed to load HDR for cubemap: ", hdrFilename);
        return nullptr;
    }
    
    // 2. Equirectangular → Cubemap 변환
    std::vector<std::vector<float>> cubeFaces(6);
    for (int face = 0; face < 6; ++face) {
        cubeFaces[face].resize(cubemapSize * cubemapSize * 3);
        
        for (int y = 0; y < cubemapSize; ++y) {
            for (int x = 0; x < cubemapSize; ++x) {
                // Cubemap UV → Direction
                float u = (x + 0.5f) / cubemapSize * 2.0f - 1.0f;
                float v = (y + 0.5f) / cubemapSize * 2.0f - 1.0f;
                
                float direction[3];
                GetCubemapDirection(face, u, v, direction);
                
                // Direction → Equirectangular UV
                float theta = atan2(direction[2], direction[0]);
                float phi = asin(direction[1]);
                
                float equiU = (theta + 3.14159f) / (2.0f * 3.14159f);
                float equiV = (phi + 3.14159f / 2.0f) / 3.14159f;
                
                // Sample from equirectangular
                int equiX = static_cast<int>(equiU * width) % width;
                int equiY = static_cast<int>(equiV * height) % height;
                
                int srcIndex = (equiY * width + equiX) * 3;
                int dstIndex = (y * cubemapSize + x) * 3;
                
                cubeFaces[face][dstIndex] = hdrData[srcIndex];         // R
                cubeFaces[face][dstIndex + 1] = hdrData[srcIndex + 1]; // G
                cubeFaces[face][dstIndex + 2] = hdrData[srcIndex + 2]; // B
            }
        }
    }
    
    stbi_image_free(hdrData);
    
    // 3. Cubemap 리소스 생성
    D3D12_RESOURCE_DESC textureDesc = {};
    textureDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    textureDesc.Width = cubemapSize;
    textureDesc.Height = cubemapSize;
    textureDesc.DepthOrArraySize = 6; // 6 faces
    textureDesc.MipLevels = 1;
    textureDesc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
    textureDesc.SampleDesc.Count = 1;
    textureDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
    textureDesc.Flags = D3D12_RESOURCE_FLAG_NONE;
    
    // Float → Half-Float 변환 및 업로드
    std::vector<uint16_t> allFacesData(cubemapSize * cubemapSize * 4 * 6);
    
    for (int face = 0; face < 6; ++face) {
        for (int i = 0; i < cubemapSize * cubemapSize; ++i) {
            int srcIndex = i * 3;
            int dstIndex = (face * cubemapSize * cubemapSize + i) * 4;
            
            allFacesData[dstIndex] = FloatToHalf(cubeFaces[face][srcIndex]);     // R
            allFacesData[dstIndex + 1] = FloatToHalf(cubeFaces[face][srcIndex + 1]); // G
            allFacesData[dstIndex + 2] = FloatToHalf(cubeFaces[face][srcIndex + 2]); // B
            allFacesData[dstIndex + 3] = FloatToHalf(1.0f);                          // A
        }
    }
    
    // CreateCubemapResource 호출 (기존 함수 활용)
    std::vector<uint8_t*> facePointers(6);
    for (int face = 0; face < 6; ++face) {
        facePointers[face] = reinterpret_cast<uint8_t*>(
            &allFacesData[face * cubemapSize * cubemapSize * 4]
        );
    }
    
    UINT64 rowSizeInBytes = cubemapSize * 8; // R16G16B16A16 = 8 bytes per pixel
    
    return CreateCubemapResource(device, commandList, textureDesc, facePointers, rowSizeInBytes, uploadBuffer);
}

} // namespace Lunar
