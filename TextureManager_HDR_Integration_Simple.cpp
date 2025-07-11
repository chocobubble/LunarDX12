// TextureManager.cpp에 HDR 지원 추가하는 방법

namespace Lunar
{

ComPtr<ID3D12Resource> TextureManager::LoadTexture(
    const LunarConstants::TextureInfo& textureInfo, 
    ID3D12Device* device, 
    ID3D12GraphicsCommandList* commandList, 
    const std::string& filename, 
    Microsoft::WRL::ComPtr<ID3D12Resource>& uploadBuffer)
{
    LOG_FUNCTION_ENTRY();

    D3D12_RESOURCE_DESC textureDesc = {};
    textureDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    textureDesc.DepthOrArraySize = 1;
    textureDesc.MipLevels = 1;
    textureDesc.SampleDesc.Count = 1;
    textureDesc.SampleDesc.Quality = 0;
    textureDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
    textureDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

    UINT64 rowSizeInBytes;
    
    // HDR 파일 체크 및 처리
    if (textureInfo.fileType == LunarConstants::FileType::HDR || stbi_is_hdr(filename.c_str()))
    {
        return LoadHDRTexture(textureInfo, device, commandList, filename, uploadBuffer);
    }
    
    // 기존 LDR 로딩 로직
    textureDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    
    if (textureInfo.dimensionType == LunarConstants::TextureDimension::CUBEMAP)
    {
        // 기존 Cubemap 로직...
    }
    
    uint8_t* data = nullptr;
    if (textureInfo.fileType == LunarConstants::FileType::DEFAULT) 
    {
        int width, height, channels;
        data = stbi_load(filename.c_str(), &width, &height, &channels, 4);
        // 기존 로직...
    }
    // 기존 코드 계속...
}

// 새로 추가할 HDR 로딩 함수
ComPtr<ID3D12Resource> TextureManager::LoadHDRTexture(
    const LunarConstants::TextureInfo& textureInfo,
    ID3D12Device* device, 
    ID3D12GraphicsCommandList* commandList, 
    const std::string& filename, 
    Microsoft::WRL::ComPtr<ID3D12Resource>& uploadBuffer)
{
    LOG_FUNCTION_ENTRY();
    
    int width, height, channels;
    
    // stb_image로 HDR 로딩 (기존 패턴과 동일)
    float* hdrData = stbi_loadf(filename.c_str(), &width, &height, &channels, 3); // RGB
    if (!hdrData) {
        LOG_ERROR("Failed to load HDR texture: ", filename);
        throw std::runtime_error("Failed to load HDR texture: " + filename);
    }
    
    LOG_DEBUG("HDR texture loaded: ", filename, " (", width, "x", height, ", ", channels, " channels)");
    
    // HDR 텍스처 디스크립터 (기존 패턴과 유사)
    D3D12_RESOURCE_DESC textureDesc = {};
    textureDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    textureDesc.Width = static_cast<UINT>(width);
    textureDesc.Height = static_cast<UINT>(height);
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
    
    UINT64 rowSizeInBytes = UINT64(width) * 8; // R16G16B16A16 = 8 bytes per pixel
    
    // 기존 CreateTextureResource 함수 재사용
    ComPtr<ID3D12Resource> texture = CreateTextureResource(
        device, 
        commandList, 
        textureDesc, 
        reinterpret_cast<const uint8_t*>(halfFloatData.data()),
        rowSizeInBytes,
        uploadBuffer
    );
    
    stbi_image_free(hdrData); // 기존 패턴과 동일한 메모리 해제
    return texture;
}

// Float to Half-Float 변환 (DirectXMath 사용 권장)
uint16_t TextureManager::FloatToHalf(float value)
{
    // DirectXMath 사용하는 것이 더 정확함
    // #include <DirectXPackedVector.h>
    // return DirectX::PackedVector::XMConvertFloatToHalf(value);
    
    // 간단한 구현 (테스트용)
    union { float f; uint32_t i; } u;
    u.f = value;
    
    uint32_t sign = (u.i >> 31) & 0x1;
    uint32_t exp = (u.i >> 23) & 0xFF;
    uint32_t mantissa = u.i & 0x7FFFFF;
    
    if (exp == 0) return static_cast<uint16_t>(sign << 15);
    if (exp == 0xFF) return static_cast<uint16_t>((sign << 15) | 0x7C00 | (mantissa ? 0x200 : 0));
    
    int newExp = exp - 127 + 15;
    if (newExp >= 31) return static_cast<uint16_t>((sign << 15) | 0x7C00);
    if (newExp <= 0) return static_cast<uint16_t>(sign << 15);
    
    return static_cast<uint16_t>((sign << 15) | (newExp << 10) | (mantissa >> 13));
}

} // namespace Lunar
