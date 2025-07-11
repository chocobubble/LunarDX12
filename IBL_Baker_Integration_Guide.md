# IBL Baker를 이용한 LunarDX12 통합 가이드

## 1. 권장 워크플로우

### A. cmftStudio 사용 (추천)

#### 설치
1. [cmftStudio 다운로드](https://github.com/dariomanesku/cmftStudio)
2. 실행 파일 압축 해제
3. PATH 환경변수에 추가 (선택사항)

#### 사용법
```bash
# 1. Equirectangular HDR → Cubemap 변환
cmft --input "Assets/HDR/environment.hdr" \
     --filter radiance \
     --srcFaceSize 1024 \
     --dstFaceSize 512 \
     --outputNum 1 \
     --output0 "Assets/IBL/environment_cubemap"

# 2. Irradiance Map 생성
cmft --input "Assets/HDR/environment.hdr" \
     --filter irradiance \
     --srcFaceSize 1024 \
     --dstFaceSize 64 \
     --outputNum 1 \
     --output0 "Assets/IBL/irradiance"

# 3. Radiance Map 생성 (여러 roughness 레벨)
cmft --input "Assets/HDR/environment.hdr" \
     --filter radiance \
     --srcFaceSize 1024 \
     --dstFaceSize 256 \
     --generateMipChain \
     --numMips 5 \
     --outputNum 1 \
     --output0 "Assets/IBL/radiance"
```

### B. 배치 스크립트 자동화

```batch
@echo off
REM IBL_Baker.bat - 자동 IBL 생성 스크립트

set CMFT_PATH="Tools/cmftStudio/cmft.exe"
set INPUT_HDR="Assets/HDR/%1.hdr"
set OUTPUT_DIR="Assets/IBL/%1"

echo Generating IBL textures for %1...

REM 출력 디렉토리 생성
mkdir "%OUTPUT_DIR%" 2>nul

REM Environment Cubemap
%CMFT_PATH% --input %INPUT_HDR% ^
            --filter radiance ^
            --srcFaceSize 1024 ^
            --dstFaceSize 512 ^
            --outputNum 1 ^
            --output0 "%OUTPUT_DIR%/environment"

REM Irradiance Map  
%CMFT_PATH% --input %INPUT_HDR% ^
            --filter irradiance ^
            --srcFaceSize 1024 ^
            --dstFaceSize 64 ^
            --outputNum 1 ^
            --output0 "%OUTPUT_DIR%/irradiance"

REM Prefiltered Environment Map
%CMFT_PATH% --input %INPUT_HDR% ^
            --filter radiance ^
            --srcFaceSize 1024 ^
            --dstFaceSize 256 ^
            --generateMipChain ^
            --numMips 5 ^
            --outputNum 1 ^
            --output0 "%OUTPUT_DIR%/prefilter"

echo IBL generation complete for %1!
pause
```

## 2. TextureManager 수정

기존 HDRLoader 대신 Baker 결과물 로딩:

```cpp
// TextureManager.cpp 수정
ComPtr<ID3D12Resource> TextureManager::LoadIBLTextures(
    const std::string& iblSetName,
    ID3D12Device* device,
    ID3D12GraphicsCommandList* commandList)
{
    std::string basePath = "Assets/IBL/" + iblSetName + "/";
    
    // Environment Cubemap 로딩
    std::string envPath = basePath + "environment.dds";
    auto environmentMap = LoadCubemapDDS(envPath, device, commandList);
    
    // Irradiance Map 로딩  
    std::string irrPath = basePath + "irradiance.dds";
    auto irradianceMap = LoadCubemapDDS(irrPath, device, commandList);
    
    // Prefiltered Map 로딩
    std::string prefilterPath = basePath + "prefilter.dds";
    auto prefilterMap = LoadCubemapDDS(prefilterPath, device, commandList);
    
    return { environmentMap, irradianceMap, prefilterMap };
}
```

## 3. IBLSystem 단순화

Baker 사용시 전처리 코드 제거:

```cpp
// IBLSystem.cpp 단순화
bool IBLSystem::LoadPrebakedIBL(
    const std::string& iblSetName,
    ID3D12Device* device,
    ID3D12GraphicsCommandList* commandList,
    D3D12_CPU_DESCRIPTOR_HANDLE& srvHandle)
{
    // Baker로 생성된 텍스처들 로딩
    std::string basePath = "Assets/IBL/" + iblSetName + "/";
    
    // 1. Environment Map
    m_iblTextures.environmentCubemap = LoadDDSCubemap(
        basePath + "environment.dds", device, commandList);
    
    // 2. Irradiance Map
    m_iblTextures.irradianceMap = LoadDDSCubemap(
        basePath + "irradiance.dds", device, commandList);
    
    // 3. Prefilter Map (Mip Chain 포함)
    m_iblTextures.prefilterMap = LoadDDSCubemap(
        basePath + "prefilter.dds", device, commandList);
    
    // 4. BRDF LUT (별도 생성 또는 미리 구운 것 사용)
    m_iblTextures.brdfLUT = LoadBRDFLUT(device, commandList);
    
    // SRV 생성
    CreateAllSRVs(device, srvHandle);
    
    return true;
}
```

## 4. 에셋 파이프라인 구성

### 디렉토리 구조
```
Assets/
├── HDR/                    # 원본 HDR 파일들
│   ├── studio.hdr
│   ├── outdoor.hdr
│   └── interior.hdr
├── IBL/                    # Baker 결과물
│   ├── studio/
│   │   ├── environment.dds
│   │   ├── irradiance.dds
│   │   └── prefilter.dds
│   ├── outdoor/
│   └── interior/
└── Tools/
    └── cmftStudio/
        └── cmft.exe
```

### 빌드 시스템 통합 (CMake/MSBuild)
```cmake
# CMakeLists.txt에 추가
function(generate_ibl HDR_NAME)
    add_custom_command(
        OUTPUT ${CMAKE_SOURCE_DIR}/Assets/IBL/${HDR_NAME}/environment.dds
        COMMAND ${CMAKE_SOURCE_DIR}/Tools/IBL_Baker.bat ${HDR_NAME}
        DEPENDS ${CMAKE_SOURCE_DIR}/Assets/HDR/${HDR_NAME}.hdr
        COMMENT "Generating IBL for ${HDR_NAME}"
    )
endfunction()

# IBL 생성 타겟들
generate_ibl(studio)
generate_ibl(outdoor)
generate_ibl(interior)
```

## 5. 런타임 IBL 전환

```cpp
// IBL 세트 동적 전환
class IBLManager {
private:
    std::map<std::string, IBLTextures> m_iblSets;
    std::string m_currentSet;

public:
    void LoadIBLSet(const std::string& name) {
        if (m_iblSets.find(name) == m_iblSets.end()) {
            m_iblSets[name] = LoadPrebakedIBL(name, ...);
        }
        m_currentSet = name;
    }
    
    void TransitionToIBL(const std::string& name, float duration) {
        // 부드러운 전환 구현
        StartIBLTransition(m_currentSet, name, duration);
    }
    
    const IBLTextures& GetCurrentIBL() const {
        return m_iblSets.at(m_currentSet);
    }
};
```

## 6. 장점과 단점 비교

### IBL Baker 사용시 장점
✅ **검증된 품질**: 전문 도구의 고품질 결과  
✅ **개발 시간 단축**: 복잡한 전처리 코드 불필요  
✅ **메모리 효율**: 최적화된 압축 포맷 (DDS)  
✅ **아티스트 친화적**: GUI 도구로 쉬운 조정  
✅ **표준 호환**: 다른 엔진과 호환 가능한 결과물  

### 단점
❌ **런타임 유연성 부족**: 실시간 IBL 수정 불가  
❌ **에셋 크기**: 미리 구운 텍스처들의 용량  
❌ **빌드 복잡성**: 에셋 파이프라인 구성 필요  
❌ **도구 의존성**: 외부 도구에 대한 의존성  

## 7. 권장 하이브리드 접근

```cpp
// 최적의 접근: Baker + 런타임 조합
class HybridIBLSystem {
public:
    // 주요 환경들은 Baker 사용
    void LoadPrebakedIBL(const std::string& name);
    
    // 동적 환경은 런타임 생성
    void GenerateRuntimeIBL(const HDRImage& hdr);
    
    // 두 IBL 간 블렌딩
    void BlendIBL(const std::string& from, const std::string& to, float t);
};
```

## 결론

**IBL Baker 사용을 강력히 추천**합니다:

1. **개발 초기**: Baker로 빠른 프로토타이핑
2. **프로덕션**: 주요 환경들은 Baker, 특수 케이스만 런타임
3. **최적화**: Baker 결과물이 더 효율적이고 안정적

이전에 구현한 IBL 시스템의 **이론적 이해**를 바탕으로 **실용적인 Baker 도구**를 활용하는 것이 가장 효율적인 접근입니다!
