# IBLBaker 오픈소스 통합 가이드

## 1. IBLBaker 설치

### GitHub에서 클론
```bash
git clone https://github.com/derydoca/IBLBaker.git
cd IBLBaker
git submodule update --init --recursive
```

### 빌드 (Windows)
```bash
# Visual Studio 2019/2022 필요
mkdir build
cd build
cmake .. -G "Visual Studio 16 2019" -A x64
cmake --build . --config Release
```

### 빌드 결과물
```
build/Release/
├── IBLBaker.exe        # 메인 실행파일
├── glfw3.dll
├── glew32.dll
└── 기타 의존성 DLL들
```

## 2. 기본 사용법

### 명령줄 인터페이스
```bash
# 기본 IBL 생성
IBLBaker.exe -inputFile "environment.hdr" -outputDir "output/"

# 상세 옵션
IBLBaker.exe ^
    -inputFile "Assets/HDR/studio.hdr" ^
    -outputDir "Assets/IBL/studio/" ^
    -irradianceSize 64 ^
    -prefilterSize 256 ^
    -environmentSize 512 ^
    -numSamples 1024
```

### 생성되는 파일들
```
Assets/IBL/studio/
├── environment.hdr         # Environment cubemap
├── irradiance.hdr         # Diffuse irradiance map  
├── prefilter_0.hdr        # Prefilter mip 0 (roughness 0.0)
├── prefilter_1.hdr        # Prefilter mip 1 (roughness 0.25)
├── prefilter_2.hdr        # Prefilter mip 2 (roughness 0.5)
├── prefilter_3.hdr        # Prefilter mip 3 (roughness 0.75)
└── prefilter_4.hdr        # Prefilter mip 4 (roughness 1.0)
```

## 3. LunarDX12 통합

### A. 배치 스크립트 생성
```batch
@echo off
REM GenerateIBL.bat - IBLBaker 자동화 스크립트

set IBLBAKER="Tools/IBLBaker/IBLBaker.exe"
set INPUT_HDR="Assets/HDR/%1.hdr"
set OUTPUT_DIR="Assets/IBL/%1"

echo Generating IBL for %1...

REM 출력 디렉토리 생성
mkdir "%OUTPUT_DIR%" 2>nul

REM IBL 생성
%IBLBAKER% -inputFile %INPUT_HDR% ^
           -outputDir "%OUTPUT_DIR%/" ^
           -irradianceSize 64 ^
           -prefilterSize 256 ^
           -environmentSize 512 ^
           -numSamples 1024 ^
           -outputFormat HDR

echo IBL generation complete!
pause
```

### B. TextureManager 수정

```cpp
// TextureManager.h에 추가
class TextureManager {
public:
    struct IBLTextures {
        ComPtr<ID3D12Resource> environmentMap;
        ComPtr<ID3D12Resource> irradianceMap;
        std::vector<ComPtr<ID3D12Resource>> prefilterMips; // 5개 mip 레벨
    };
    
    IBLTextures LoadIBLBakerOutput(
        const std::string& iblSetName,
        ID3D12Device* device,
        ID3D12GraphicsCommandList* commandList
    );

private:
    ComPtr<ID3D12Resource> LoadHDRCubemap(
        const std::string& filename,
        ID3D12Device* device,
        ID3D12GraphicsCommandList* commandList
    );
};
```

```cpp
// TextureManager.cpp 구현
TextureManager::IBLTextures TextureManager::LoadIBLBakerOutput(
    const std::string& iblSetName,
    ID3D12Device* device,
    ID3D12GraphicsCommandList* commandList)
{
    IBLTextures result;
    std::string basePath = "Assets/IBL/" + iblSetName + "/";
    
    // 1. Environment Map 로딩
    result.environmentMap = LoadHDRCubemap(
        basePath + "environment.hdr", device, commandList);
    
    // 2. Irradiance Map 로딩
    result.irradianceMap = LoadHDRCubemap(
        basePath + "irradiance.hdr", device, commandList);
    
    // 3. Prefilter Maps 로딩 (5개 mip 레벨)
    result.prefilterMips.resize(5);
    for (int mip = 0; mip < 5; ++mip) {
        std::string prefilterFile = basePath + "prefilter_" + std::to_string(mip) + ".hdr";
        result.prefilterMips[mip] = LoadHDRCubemap(prefilterFile, device, commandList);
    }
    
    return result;
}

ComPtr<ID3D12Resource> TextureManager::LoadHDRCubemap(
    const std::string& filename,
    ID3D12Device* device,
    ID3D12GraphicsCommandList* commandList)
{
    // IBLBaker는 6개 면을 별도 파일로 출력하므로
    // 파일명 패턴에 따라 로딩 필요
    
    std::vector<std::string> faceFiles = {
        filename + "_px.hdr", // +X
        filename + "_nx.hdr", // -X
        filename + "_py.hdr", // +Y
        filename + "_ny.hdr", // -Y
        filename + "_pz.hdr", // +Z
        filename + "_nz.hdr"  // -Z
    };
    
    // 각 면을 stb_image로 로딩
    std::vector<std::unique_ptr<HDRImage>> faces(6);
    for (int i = 0; i < 6; ++i) {
        int width, height, channels;
        float* data = stbi_loadf(faceFiles[i].c_str(), &width, &height, &channels, 3);
        if (!data) {
            LOG_ERROR("Failed to load cubemap face: ", faceFiles[i]);
            continue;
        }
        
        faces[i] = std::make_unique<HDRImage>();
        faces[i]->width = width;
        faces[i]->height = height;
        faces[i]->channels = 3;
        faces[i]->data.assign(data, data + width * height * 3);
        
        stbi_image_free(data);
    }
    
    // DirectX 12 큐브맵 리소스 생성
    return CreateCubemapFromHDRFaces(faces, device, commandList);
}
```

### C. IBLSystem 단순화

```cpp
// IBLSystem.cpp - IBLBaker 결과물 사용
bool IBLSystem::LoadFromIBLBaker(
    const std::string& iblSetName,
    ID3D12Device* device,
    ID3D12GraphicsCommandList* commandList,
    D3D12_CPU_DESCRIPTOR_HANDLE& srvHandle)
{
    // TextureManager를 통해 IBLBaker 결과물 로딩
    auto iblTextures = m_textureManager->LoadIBLBakerOutput(
        iblSetName, device, commandList);
    
    m_iblTextures.environmentCubemap = iblTextures.environmentMap;
    m_iblTextures.irradianceMap = iblTextures.irradianceMap;
    
    // Prefilter Map을 하나의 mip chain 텍스처로 결합
    m_iblTextures.prefilterMap = CombinePrefilterMips(
        iblTextures.prefilterMips, device, commandList);
    
    // BRDF LUT는 별도 생성 (IBLBaker에서 제공하지 않음)
    m_iblTextures.brdfLUT = GenerateBRDFLUT(device, commandList);
    
    // SRV 생성
    CreateShaderResourceViews(device, srvHandle);
    
    Logger::Log("IBL loaded from IBLBaker: " + iblSetName);
    return true;
}
```

## 4. 빌드 시스템 통합

### CMake 통합 (선택사항)
```cmake
# CMakeLists.txt에 추가
find_program(IBLBAKER_EXECUTABLE
    NAMES IBLBaker IBLBaker.exe
    PATHS ${CMAKE_SOURCE_DIR}/Tools/IBLBaker
    DOC "IBLBaker executable"
)

function(generate_ibl_baker HDR_NAME)
    set(INPUT_HDR ${CMAKE_SOURCE_DIR}/Assets/HDR/${HDR_NAME}.hdr)
    set(OUTPUT_DIR ${CMAKE_SOURCE_DIR}/Assets/IBL/${HDR_NAME})
    
    add_custom_command(
        OUTPUT ${OUTPUT_DIR}/environment.hdr
        COMMAND ${CMAKE_COMMAND} -E make_directory ${OUTPUT_DIR}
        COMMAND ${IBLBAKER_EXECUTABLE}
            -inputFile ${INPUT_HDR}
            -outputDir ${OUTPUT_DIR}/
            -irradianceSize 64
            -prefilterSize 256
            -environmentSize 512
            -numSamples 1024
        DEPENDS ${INPUT_HDR}
        COMMENT "Generating IBL using IBLBaker for ${HDR_NAME}"
    )
    
    add_custom_target(ibl_${HDR_NAME} 
        DEPENDS ${OUTPUT_DIR}/environment.hdr
    )
endfunction()

# IBL 생성 타겟들
generate_ibl_baker(studio)
generate_ibl_baker(outdoor)
generate_ibl_baker(interior)
```

## 5. 사용 예시

### MainApp.cpp에서 사용
```cpp
void MainApp::InitializeIBL()
{
    // IBLBaker로 생성된 IBL 로딩
    m_iblSystem->LoadFromIBLBaker("studio", m_device.Get(), m_commandList.Get(), srvHandle);
    
    // 런타임에 다른 IBL로 전환
    // m_iblSystem->LoadFromIBLBaker("outdoor", ...);
}
```

### 셰이더에서 사용 (기존과 동일)
```hlsl
// PBRWithIBL.hlsl - 변경 없음
float3 finalColor = CookTorranceWithIBL(normal, lightDir, viewDir, material);
```

## 6. IBLBaker 장점

### ✅ 오픈소스 장점
- **무료**: 라이선스 비용 없음
- **커스터마이징**: 소스코드 수정 가능
- **투명성**: 알고리즘 완전 공개
- **커뮤니티**: GitHub 이슈/PR 가능

### ✅ 기술적 장점
- **검증된 품질**: 표준 IBL 알고리즘 구현
- **효율성**: 최적화된 몬테카를로 샘플링
- **호환성**: 표준 HDR 포맷 지원
- **확장성**: 새로운 기능 추가 가능

## 7. 디렉토리 구조

```
LunarDX12/
├── Assets/
│   ├── HDR/                    # 원본 HDR 파일들
│   │   ├── studio.hdr
│   │   ├── outdoor.hdr
│   │   └── interior.hdr
│   └── IBL/                    # IBLBaker 결과물
│       ├── studio/
│       │   ├── environment.hdr
│       │   ├── irradiance.hdr
│       │   ├── prefilter_0.hdr
│       │   ├── prefilter_1.hdr
│       │   ├── prefilter_2.hdr
│       │   ├── prefilter_3.hdr
│       │   └── prefilter_4.hdr
│       ├── outdoor/
│       └── interior/
├── Tools/
│   └── IBLBaker/
│       ├── IBLBaker.exe
│       └── 의존성 DLL들
└── Scripts/
    └── GenerateIBL.bat
```

## 결론

IBLBaker 오픈소스는 **완벽한 선택**입니다:

1. **비용 효율적**: 완전 무료
2. **품질 보장**: 검증된 IBL 알고리즘
3. **통합 용이**: 기존 시스템과 쉬운 연동
4. **확장 가능**: 필요시 소스코드 수정

이전에 구현한 IBL 이론적 이해를 바탕으로 **실용적인 IBLBaker**를 활용하여 고품질 IBL 시스템을 완성할 수 있습니다!
