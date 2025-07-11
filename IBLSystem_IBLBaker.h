#pragma once
#include "IBLSystem.h"
#include "TextureManager_IBLBaker.h"

namespace Lunar
{

// IBLBaker 통합을 위한 IBLSystem 확장
class IBLSystemWithBaker : public IBLSystem
{
public:
    IBLSystemWithBaker() = default;
    ~IBLSystemWithBaker() = default;

    // IBLBaker 결과물 로딩
    bool LoadFromIBLBaker(
        const std::string& iblSetName,
        ID3D12Device* device,
        ID3D12GraphicsCommandList* commandList,
        D3D12_CPU_DESCRIPTOR_HANDLE& srvHandle
    );
    
    // 여러 IBL 세트 관리
    bool LoadMultipleIBLSets(
        const std::vector<std::string>& iblSetNames,
        ID3D12Device* device,
        ID3D12GraphicsCommandList* commandList,
        D3D12_CPU_DESCRIPTOR_HANDLE& srvHandle
    );
    
    // IBL 세트 전환
    void SwitchToIBLSet(const std::string& iblSetName);
    
    // 현재 활성 IBL 세트 이름
    const std::string& GetCurrentIBLSet() const { return m_currentIBLSet; }
    
    // 로딩된 IBL 세트 목록
    std::vector<std::string> GetLoadedIBLSets() const;
    
    // IBL 세트 간 블렌딩 (고급 기능)
    void BlendIBLSets(
        const std::string& fromSet,
        const std::string& toSet,
        float blendFactor
    );

private:
    // IBL 세트 저장소
    std::map<std::string, IBLBakerTextures> m_iblSets;
    std::string m_currentIBLSet;
    
    // 확장된 TextureManager
    std::unique_ptr<TextureManagerExtended> m_extendedTextureManager;
    
    // Prefilter mip들을 단일 텍스처로 결합
    Microsoft::WRL::ComPtr<ID3D12Resource> CombinePrefilterMips(
        const std::vector<Microsoft::WRL::ComPtr<ID3D12Resource>>& mips,
        ID3D12Device* device,
        ID3D12GraphicsCommandList* commandList
    );
    
    // IBLBaker 텍스처를 기존 IBLTextures로 변환
    void ConvertToIBLTextures(
        const IBLBakerTextures& bakerTextures,
        ID3D12Device* device,
        ID3D12GraphicsCommandList* commandList
    );
};

} // namespace Lunar
