#include "IBLSystem_IBLBaker.h"
#include "Utils/Logger.h"

namespace Lunar
{

bool IBLSystemWithBaker::LoadFromIBLBaker(
    const std::string& iblSetName,
    ID3D12Device* device,
    ID3D12GraphicsCommandList* commandList,
    D3D12_CPU_DESCRIPTOR_HANDLE& srvHandle)
{
    LOG_FUNCTION_ENTRY();
    
    try {
        // 확장된 TextureManager 초기화
        if (!m_extendedTextureManager) {
            m_extendedTextureManager = std::make_unique<TextureManagerExtended>();
        }
        
        // IBLBaker 결과물 로딩
        LOG_DEBUG("Loading IBLBaker output for: ", iblSetName);
        auto bakerTextures = m_extendedTextureManager->LoadIBLBakerOutput(
            iblSetName, device, commandList, srvHandle);
        
        // IBL 세트 저장
        m_iblSets[iblSetName] = std::move(bakerTextures);
        m_currentIBLSet = iblSetName;
        
        // 기존 IBLTextures 구조체로 변환
        ConvertToIBLTextures(m_iblSets[iblSetName], device, commandList);
        
        // BRDF LUT 생성 (IBLBaker에서 제공하지 않음)
        if (!m_iblTextures.brdfLUT) {
            LOG_DEBUG("Generating BRDF LUT...");
            m_iblTextures.brdfLUT = GenerateBRDFLUT(device, commandList);
            
            // BRDF LUT SRV 생성
            D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
            srvDesc.Format = DXGI_FORMAT_R16G16_FLOAT;
            srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
            srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
            srvDesc.Texture2D.MipLevels = 1;
            
            device->CreateShaderResourceView(m_iblTextures.brdfLUT.Get(), &srvDesc, srvHandle);
            m_iblTextures.brdfSRV = srvHandle;
            srvHandle.ptr += device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
        }
        
        LOG_INFO("IBL loaded successfully from IBLBaker: ", iblSetName);
        return true;
        
    } catch (const std::exception& e) {
        LOG_ERROR("Failed to load IBL from IBLBaker: ", e.what());
        return false;
    }
}

bool IBLSystemWithBaker::LoadMultipleIBLSets(
    const std::vector<std::string>& iblSetNames,
    ID3D12Device* device,
    ID3D12GraphicsCommandList* commandList,
    D3D12_CPU_DESCRIPTOR_HANDLE& srvHandle)
{
    LOG_FUNCTION_ENTRY();
    
    bool allSucceeded = true;
    
    for (const auto& setName : iblSetNames) {
        LOG_DEBUG("Loading IBL set: ", setName);
        
        if (!LoadFromIBLBaker(setName, device, commandList, srvHandle)) {
            LOG_ERROR("Failed to load IBL set: ", setName);
            allSucceeded = false;
            continue;
        }
        
        LOG_DEBUG("Successfully loaded IBL set: ", setName);
    }
    
    // 첫 번째 성공한 세트를 현재 세트로 설정
    if (!m_iblSets.empty() && m_currentIBLSet.empty()) {
        m_currentIBLSet = m_iblSets.begin()->first;
        ConvertToIBLTextures(m_iblSets[m_currentIBLSet], device, commandList);
    }
    
    LOG_INFO("Loaded ", m_iblSets.size(), " IBL sets");
    return allSucceeded;
}

void IBLSystemWithBaker::SwitchToIBLSet(const std::string& iblSetName)
{
    LOG_FUNCTION_ENTRY();
    
    auto it = m_iblSets.find(iblSetName);
    if (it == m_iblSets.end()) {
        LOG_ERROR("IBL set not found: ", iblSetName);
        return;
    }
    
    if (m_currentIBLSet == iblSetName) {
        LOG_DEBUG("Already using IBL set: ", iblSetName);
        return;
    }
    
    LOG_DEBUG("Switching to IBL set: ", iblSetName);
    m_currentIBLSet = iblSetName;
    
    // 현재 IBLTextures 업데이트
    const auto& bakerTextures = it->second;
    m_iblTextures.environmentCubemap = bakerTextures.environmentMap;
    m_iblTextures.irradianceMap = bakerTextures.irradianceMap;
    m_iblTextures.environmentSRV = bakerTextures.environmentSRV;
    m_iblTextures.irradianceSRV = bakerTextures.irradianceSRV;
    
    // Prefilter Map 결합 (필요시)
    // m_iblTextures.prefilterMap = CombinePrefilterMips(...);
    
    LOG_INFO("Switched to IBL set: ", iblSetName);
}

std::vector<std::string> IBLSystemWithBaker::GetLoadedIBLSets() const
{
    std::vector<std::string> setNames;
    setNames.reserve(m_iblSets.size());
    
    for (const auto& pair : m_iblSets) {
        setNames.push_back(pair.first);
    }
    
    return setNames;
}

void IBLSystemWithBaker::BlendIBLSets(
    const std::string& fromSet,
    const std::string& toSet,
    float blendFactor)
{
    LOG_FUNCTION_ENTRY();
    
    // 고급 기능: 두 IBL 세트 간 블렌딩
    // 실제 구현은 복잡하므로 여기서는 간단한 전환만 수행
    
    if (blendFactor < 0.5f) {
        SwitchToIBLSet(fromSet);
    } else {
        SwitchToIBLSet(toSet);
    }
    
    LOG_DEBUG("Blended IBL from ", fromSet, " to ", toSet, " with factor ", blendFactor);
}

Microsoft::WRL::ComPtr<ID3D12Resource> IBLSystemWithBaker::CombinePrefilterMips(
    const std::vector<Microsoft::WRL::ComPtr<ID3D12Resource>>& mips,
    ID3D12Device* device,
    ID3D12GraphicsCommandList* commandList)
{
    // Prefilter mip들을 하나의 mip chain 텍스처로 결합
    // 복잡한 구현이므로 여기서는 첫 번째 mip만 반환
    
    if (mips.empty()) {
        LOG_ERROR("No prefilter mips to combine");
        return nullptr;
    }
    
    LOG_DEBUG("Using first prefilter mip as combined texture");
    return mips[0];
}

void IBLSystemWithBaker::ConvertToIBLTextures(
    const IBLBakerTextures& bakerTextures,
    ID3D12Device* device,
    ID3D12GraphicsCommandList* commandList)
{
    // IBLBakerTextures를 기존 IBLTextures 구조체로 변환
    m_iblTextures.environmentCubemap = bakerTextures.environmentMap;
    m_iblTextures.irradianceMap = bakerTextures.irradianceMap;
    m_iblTextures.environmentSRV = bakerTextures.environmentSRV;
    m_iblTextures.irradianceSRV = bakerTextures.irradianceSRV;
    
    // Prefilter Map 처리 (여러 mip을 하나로 결합)
    if (!bakerTextures.prefilterMips.empty()) {
        m_iblTextures.prefilterMap = CombinePrefilterMips(
            bakerTextures.prefilterMips, device, commandList);
        
        // 첫 번째 prefilter mip의 SRV 사용
        if (!bakerTextures.prefilterSRVs.empty()) {
            m_iblTextures.prefilterSRV = bakerTextures.prefilterSRVs[0];
        }
    }
    
    LOG_DEBUG("Converted IBLBaker textures to IBLTextures format");
}

} // namespace Lunar
