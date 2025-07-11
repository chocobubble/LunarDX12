#include "TextureManager_IBLBaker.h"
#include "Utils/Logger.h"
#include <stb_image.h>
#include <filesystem>

namespace Lunar
{

IBLBakerTextures TextureManagerExtended::LoadIBLBakerOutput(
    const std::string& iblSetName,
    ID3D12Device* device,
    ID3D12GraphicsCommandList* commandList,
    D3D12_CPU_DESCRIPTOR_HANDLE& srvHandle)
{
    LOG_FUNCTION_ENTRY();
    
    IBLBakerTextures result;
    std::string basePath = "Assets/IBL/" + iblSetName + "/";
    
    // 디렉토리 존재 확인
    if (!std::filesystem::exists(basePath)) {
        LOG_ERROR("IBL directory not found: ", basePath);
        throw std::runtime_error("IBL directory not found: " + basePath);
    }
    
    try {
        // 1. Environment Map 로딩
        LOG_DEBUG("Loading environment map...");
        result.environmentMap = LoadHDRCubemapFromFaces(
            basePath + "environment", device, commandList);
        
        // 2. Irradiance Map 로딩
        LOG_DEBUG("Loading irradiance map...");
        result.irradianceMap = LoadHDRCubemapFromFaces(
            basePath + "irradiance", device, commandList);
        
        // 3. Prefilter Maps 로딩 (5개 mip 레벨)
        LOG_DEBUG("Loading prefilter maps...");
        result.prefilterMips.resize(5);
        for (int mip = 0; mip < 5; ++mip) {
            std::string prefilterBase = basePath + "prefilter_" + std::to_string(mip);
            result.prefilterMips[mip] = LoadHDRCubemapFromFaces(
                prefilterBase, device, commandList);
        }
        
        // 4. SRV 생성
        CreateIBLShaderResourceViews(result, device, srvHandle);
        
        LOG_DEBUG("IBLBaker textures loaded successfully: ", iblSetName);
        
    } catch (const std::exception& e) {
        LOG_ERROR("Failed to load IBLBaker output: ", e.what());
        throw;
    }
    
    return result;
}

Microsoft::WRL::ComPtr<ID3D12Resource> TextureManagerExtended::LoadHDRCubemapFromFaces(
    const std::string& baseName,
    ID3D12Device* device,
    ID3D12GraphicsCommandList* commandList)
{
    // IBLBaker 파일명 규칙: basename_px.hdr, basename_nx.hdr, etc.
    std::vector<std::string> faceSuffixes = {
        "_px", "_nx", "_py", "_ny", "_pz", "_nz"  // +X, -X, +Y, -Y, +Z, -Z
    };
    
    std::vector<HDRFaceData> faces(6);
    
    // 각 면 로딩
    for (int i = 0; i < 6; ++i) {
        std::string faceFile = baseName + faceSuffixes[i] + ".hdr";
        faces[i] = LoadHDRFace(faceFile);
        
        if (faces[i].data.empty()) {
            LOG_ERROR("Failed to load cubemap face: ", faceFile);
            throw std::runtime_error("Failed to load cubemap face: " + faceFile);
        }
    }
    
    // 모든 면의 크기가 동일한지 확인
    int faceSize = faces[0].width;
    for (const auto& face : faces) {
        if (face.width != faceSize || face.height != faceSize) {
            LOG_ERROR("Cubemap faces have different sizes");
            throw std::runtime_error("Cubemap faces must have the same size");
        }
    }
    
    LOG_DEBUG("Loaded HDR cubemap faces: ", faceSize, "x", faceSize);
    
    return CreateHDRCubemapResource(faces, device, commandList);
}

TextureManagerExtended::HDRFaceData TextureManagerExtended::LoadHDRFace(const std::string& filename)
{
    HDRFaceData face;
    
    int channels;
    float* data = stbi_loadf(filename.c_str(), &face.width, &face.height, &channels, 3);
    
    if (!data) {
        LOG_ERROR("stbi_loadf failed for: ", filename);
        return face; // 빈 데이터 반환
    }
    
    // RGB 데이터를 벡터로 복사
    size_t dataSize = face.width * face.height * 3;
    face.data.assign(data, data + dataSize);
    
    stbi_image_free(data);
    
    LOG_DEBUG("Loaded HDR face: ", filename, " (", face.width, "x", face.height, ")");
    return face;
}

Microsoft::WRL::ComPtr<ID3D12Resource> TextureManagerExtended::CreateHDRCubemapResource(
    const std::vector<HDRFaceData>& faces,
    ID3D12Device* device,
    ID3D12GraphicsCommandList* commandList)
{
    if (faces.size() != 6) {
        throw std::runtime_error("Cubemap must have exactly 6 faces");
    }
    
    int faceSize = faces[0].width;
    
    // 큐브맵 리소스 디스크립터
    D3D12_RESOURCE_DESC textureDesc = {};
    textureDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    textureDesc.Width = faceSize;
    textureDesc.Height = faceSize;
    textureDesc.DepthOrArraySize = 6; // 6 faces
    textureDesc.MipLevels = 1;
    textureDesc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT; // HDR 포맷
    textureDesc.SampleDesc.Count = 1;
    textureDesc.SampleDesc.Quality = 0;
    textureDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
    textureDesc.Flags = D3D12_RESOURCE_FLAG_NONE;
    
    // Default heap에 텍스처 생성
    Microsoft::WRL::ComPtr<ID3D12Resource> cubemap;
    D3D12_HEAP_PROPERTIES heapProps = {};
    heapProps.Type = D3D12_HEAP_TYPE_DEFAULT;
    
    HRESULT hr = device->CreateCommittedResource(
        &heapProps,
        D3D12_HEAP_FLAG_NONE,
        &textureDesc,
        D3D12_RESOURCE_STATE_COPY_DEST,
        nullptr,
        IID_PPV_ARGS(&cubemap));
    
    if (FAILED(hr)) {
        throw std::runtime_error("Failed to create cubemap resource");
    }
    
    // Upload buffer 생성
    const UINT bytesPerPixel = 8; // R16G16B16A16_FLOAT = 8 bytes
    const UINT64 uploadBufferSize = faceSize * faceSize * bytesPerPixel * 6;
    
    Microsoft::WRL::ComPtr<ID3D12Resource> uploadBuffer;
    D3D12_HEAP_PROPERTIES uploadHeapProps = {};
    uploadHeapProps.Type = D3D12_HEAP_TYPE_UPLOAD;
    
    D3D12_RESOURCE_DESC uploadDesc = {};
    uploadDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    uploadDesc.Width = uploadBufferSize;
    uploadDesc.Height = 1;
    uploadDesc.DepthOrArraySize = 1;
    uploadDesc.MipLevels = 1;
    uploadDesc.Format = DXGI_FORMAT_UNKNOWN;
    uploadDesc.SampleDesc.Count = 1;
    uploadDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    
    hr = device->CreateCommittedResource(
        &uploadHeapProps,
        D3D12_HEAP_FLAG_NONE,
        &uploadDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(&uploadBuffer));
    
    if (FAILED(hr)) {
        throw std::runtime_error("Failed to create upload buffer");
    }
    
    // 데이터 업로드
    void* mappedData;
    uploadBuffer->Map(0, nullptr, &mappedData);
    
    for (int face = 0; face < 6; ++face) {
        const HDRFaceData& faceData = faces[face];
        
        // Float RGB를 Half-Float RGBA로 변환
        std::vector<uint16_t> halfFloatData(faceSize * faceSize * 4);
        
        for (int i = 0; i < faceSize * faceSize; ++i) {
            // RGB → RGBA 변환 및 Float → Half 변환
            halfFloatData[i * 4] = FloatToHalf(faceData.data[i * 3]);     // R
            halfFloatData[i * 4 + 1] = FloatToHalf(faceData.data[i * 3 + 1]); // G
            halfFloatData[i * 4 + 2] = FloatToHalf(faceData.data[i * 3 + 2]); // B
            halfFloatData[i * 4 + 3] = FloatToHalf(1.0f);                     // A
        }
        
        // 면별 데이터 복사
        size_t faceOffset = face * faceSize * faceSize * bytesPerPixel;
        memcpy(static_cast<char*>(mappedData) + faceOffset,
               halfFloatData.data(),
               faceSize * faceSize * bytesPerPixel);
    }
    
    uploadBuffer->Unmap(0, nullptr);
    
    // GPU로 복사
    for (int face = 0; face < 6; ++face) {
        D3D12_TEXTURE_COPY_LOCATION dst = {};
        dst.pResource = cubemap.Get();
        dst.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
        dst.SubresourceIndex = face;
        
        D3D12_TEXTURE_COPY_LOCATION src = {};
        src.pResource = uploadBuffer.Get();
        src.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
        src.PlacedFootprint.Offset = face * faceSize * faceSize * bytesPerPixel;
        src.PlacedFootprint.Footprint.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
        src.PlacedFootprint.Footprint.Width = faceSize;
        src.PlacedFootprint.Footprint.Height = faceSize;
        src.PlacedFootprint.Footprint.Depth = 1;
        src.PlacedFootprint.Footprint.RowPitch = faceSize * bytesPerPixel;
        
        commandList->CopyTextureRegion(&dst, 0, 0, 0, &src, nullptr);
    }
    
    // 리소스 상태 전환
    D3D12_RESOURCE_BARRIER barrier = {};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Transition.pResource = cubemap.Get();
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    
    commandList->ResourceBarrier(1, &barrier);
    
    return cubemap;
}

void TextureManagerExtended::CreateIBLShaderResourceViews(
    IBLBakerTextures& iblTextures,
    ID3D12Device* device,
    D3D12_CPU_DESCRIPTOR_HANDLE& srvHandle)
{
    UINT descriptorSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    
    // Environment Map SRV
    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE;
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.TextureCube.MipLevels = 1;
    
    device->CreateShaderResourceView(iblTextures.environmentMap.Get(), &srvDesc, srvHandle);
    iblTextures.environmentSRV = srvHandle;
    srvHandle.ptr += descriptorSize;
    
    // Irradiance Map SRV
    device->CreateShaderResourceView(iblTextures.irradianceMap.Get(), &srvDesc, srvHandle);
    iblTextures.irradianceSRV = srvHandle;
    srvHandle.ptr += descriptorSize;
    
    // Prefilter Maps SRVs
    iblTextures.prefilterSRVs.resize(5);
    for (int mip = 0; mip < 5; ++mip) {
        device->CreateShaderResourceView(iblTextures.prefilterMips[mip].Get(), &srvDesc, srvHandle);
        iblTextures.prefilterSRVs[mip] = srvHandle;
        srvHandle.ptr += descriptorSize;
    }
}

// Float to Half-Float 변환 (DirectXMath 사용 권장)
uint16_t TextureManagerExtended::FloatToHalf(float value)
{
    // 간단한 Float → Half 변환 구현
    // 실제 프로덕션에서는 DirectX::PackedVector::XMConvertFloatToHalf 사용 권장
    
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
