#pragma once
// TextureManager에 IBLBaker 지원 추가

#include "TextureManager.h"
#include <vector>

namespace Lunar
{

// IBLBaker 결과물을 위한 구조체
struct IBLBakerTextures
{
    Microsoft::WRL::ComPtr<ID3D12Resource> environmentMap;
    Microsoft::WRL::ComPtr<ID3D12Resource> irradianceMap;
    std::vector<Microsoft::WRL::ComPtr<ID3D12Resource>> prefilterMips; // 5개 mip 레벨
    
    D3D12_CPU_DESCRIPTOR_HANDLE environmentSRV;
    D3D12_CPU_DESCRIPTOR_HANDLE irradianceSRV;
    std::vector<D3D12_CPU_DESCRIPTOR_HANDLE> prefilterSRVs;
};

// TextureManager 확장 클래스
class TextureManagerExtended : public TextureManager
{
public:
    // IBLBaker 결과물 로딩
    IBLBakerTextures LoadIBLBakerOutput(
        const std::string& iblSetName,
        ID3D12Device* device,
        ID3D12GraphicsCommandList* commandList,
        D3D12_CPU_DESCRIPTOR_HANDLE& srvHandle
    );
    
    // 단일 HDR 큐브맵 로딩 (IBLBaker 6면 파일 형식)
    Microsoft::WRL::ComPtr<ID3D12Resource> LoadHDRCubemapFromFaces(
        const std::string& baseName,
        ID3D12Device* device,
        ID3D12GraphicsCommandList* commandList
    );
    
    // Prefilter mip들을 하나의 텍스처로 결합
    Microsoft::WRL::ComPtr<ID3D12Resource> CombinePrefilterMips(
        const std::vector<Microsoft::WRL::ComPtr<ID3D12Resource>>& mips,
        ID3D12Device* device,
        ID3D12GraphicsCommandList* commandList
    );

private:
    // HDR 큐브맵 면 로딩 헬퍼
    struct HDRFaceData {
        int width, height;
        std::vector<float> data;
    };
    
    HDRFaceData LoadHDRFace(const std::string& filename);
    
    // 6개 면에서 큐브맵 리소스 생성
    Microsoft::WRL::ComPtr<ID3D12Resource> CreateHDRCubemapResource(
        const std::vector<HDRFaceData>& faces,
        ID3D12Device* device,
        ID3D12GraphicsCommandList* commandList
    );
};

} // namespace Lunar
