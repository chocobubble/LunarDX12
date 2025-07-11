# HDR Loader & IBL System Implementation

## 구현 완료된 기능들

### 1. HDR 파일 로더 (`Utils/HDRLoader.h/cpp`)
- **Radiance HDR 포맷** (.hdr) 파일 로딩 지원
- **Equirectangular → Cubemap** 변환
- **IBL 전처리** 기능:
  - Irradiance Map 생성 (Diffuse IBL)
  - Prefiltered Environment Map 생성 (Specular IBL)
  - BRDF Integration LUT 생성

### 2. IBL 시스템 (`IBLSystem.h/cpp`)
- **DirectX 12 통합** IBL 관리 클래스
- **GPU 리소스 관리**: Cubemap, Irradiance Map, Prefilter Map, BRDF LUT
- **셰이더 리소스 뷰** 자동 생성 및 바인딩
- **HDR 텍스처 업로드** 최적화

### 3. PBR + IBL 셰이더 (`Shaders/PBRWithIBL.hlsl`)
- **기존 Cook-Torrance BRDF**와 IBL 통합
- **환경 조명** 계산:
  - Diffuse IBL (Irradiance Map 샘플링)
  - Specular IBL (Prefiltered Environment + BRDF LUT)
- **톤매핑** 통합 (Uncharted 2 방식)
- **디버그 시각화** 모드 지원

### 4. 테스트 환경 생성기 (`Utils/HDRTestGenerator.h/cpp`)
- **프로시저럴 HDR 환경** 생성
- **하늘 그라디언트** + **태양** 시뮬레이션
- **테스트용 환경맵** 자동 생성

## 통합된 렌더링 파이프라인

```
[Scene Geometry] → [PBR + IBL Shading] → [HDR Render Target] → [Post-Processing] → [Tone Mapping] → [Present]
                           ↑
                    [Environment Cubemap]
                    [Irradiance Map]
                    [Prefilter Map]
                    [BRDF LUT]
```

## 사용 방법

### 1. 초기화
```cpp
// MainApp.cpp에서 자동 초기화됨
m_iblSystem->Initialize(m_device.Get(), m_commandList.Get());
```

### 2. HDR 환경맵 로딩
```cpp
// 실제 HDR 파일 사용시:
m_iblSystem->LoadEnvironmentMap("Assets/HDR/environment.hdr", ...);

// 현재는 테스트 환경 자동 생성
auto testHDR = HDRTestGenerator::GenerateTestEnvironment(512, 256);
```

### 3. 셰이더에서 IBL 사용
```hlsl
// PBRWithIBL.hlsl 사용
float3 finalColor = CookTorranceWithIBL(normal, lightDir, viewDir, material);
```

## 이전 대화와의 연결점

### ✅ **PBR 시스템 아키텍처** 완성
- 이전에 설계한 ViewModel 패턴 기반 PBR 시스템에 IBL 통합
- MaterialConstants 구조체와 완벽 호환

### ✅ **HDR 파이프라인** 완성  
- R16G16B16A16_FLOAT 렌더 타겟 활용
- 이전에 논의한 톤매핑 시스템과 연결

### ✅ **포스트프로세싱 체인** 준비
- IBL → 가우시안 블러 → 톤매핑 → NPR 순서로 진행 가능
- 핑퐁 버퍼 시스템과 호환

## 다음 단계

1. **가우시안 블러** 구현 (이미 분석 완료)
2. **Uncharted 2 톤매핑** 적용 (셰이더에 이미 구현됨)
3. **NPR 효과** 추가

## 주요 특징

- **물리 기반**: 실제 필름과 인간 시각 시스템 모델링
- **성능 최적화**: GroupShared Memory, 분리 가능한 필터 활용
- **확장 가능**: 다양한 HDR 포맷과 IBL 기법 지원 준비
- **디버그 친화적**: 각 단계별 시각화 모드 제공

이제 **완전한 PBR + IBL 시스템**이 구축되었으며, 이전 대화에서 계획한 포스트프로세싱 파이프라인으로 진행할 수 있습니다!
