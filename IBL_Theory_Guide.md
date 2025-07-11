# IBL (Image-Based Lighting) 원리 완전 가이드

## 목차
1. [IBL의 물리학적 기반](#1-ibl의-물리학적-기반)
2. [IBL의 두 가지 구성 요소](#2-ibl의-두-가지-구성-요소)
3. [IBL 전처리 (Pre-computation)](#3-ibl-전처리-pre-computation)
4. [실시간 IBL 계산](#4-실시간-ibl-계산)
5. [수학적 세부사항](#5-수학적-세부사항)
6. [IBL의 물리적 정확성](#6-ibl의-물리적-정확성)
7. [성능 최적화](#7-성능-최적화)
8. [실제 구현에서의 고려사항](#8-실제-구현에서의-고려사항)
9. [IBL의 한계와 해결책](#9-ibl의-한계와-해결책)

---

## 1. IBL의 물리학적 기반

### 렌더링 방정식 (Rendering Equation)
IBL의 기초가 되는 렌더링 방정식:

```
Lo(p,ωo) = Le(p,ωo) + ∫Ω fr(p,ωi,ωo) Li(p,ωi) (n·ωi) dωi
```

**변수 설명:**
- `Lo`: 출력 방사휘도 (눈으로 보는 빛의 양)
- `Le`: 자체 발광 (물체가 스스로 내는 빛)
- `fr`: BRDF (양방향 반사 분포 함수)
- `Li`: 입사 방사휘도 (들어오는 빛의 양)
- `Ω`: 반구 (hemisphere) - 표면 위의 모든 방향
- `n`: 표면 법선 벡터
- `ωi`: 입사 방향
- `ωo`: 출사 방향 (관찰자 방향)

### IBL의 핵심 아이디어
전통적인 점광원이나 방향광 대신 **환경맵의 모든 방향에서 오는 빛**을 계산:

```
Lo(p,ωo) = ∫Ω fr(p,ωi,ωo) Lenvironment(ωi) (n·ωi) dωi
```

여기서 `Lenvironment(ωi)`는 환경맵에서 방향 `ωi`로부터 오는 빛의 양입니다.

---

## 2. IBL의 두 가지 구성 요소

### A. Diffuse IBL (Lambert 반사)

**완벽한 확산 반사 (Lambertian BRDF):**
```hlsl
fr_diffuse = albedo / π
```

**Diffuse IBL 적분:**
```hlsl
Ldiffuse = (albedo/π) ∫Ω Lenvironment(ωi) (n·ωi) dωi
```

**핵심 특징:**
- 이 적분은 **법선 방향에만 의존**
- View direction과 무관하므로 **미리 계산 가능**
- 결과: **Irradiance Map** (조도맵)

### B. Specular IBL (Cook-Torrance 반사)

**Cook-Torrance BRDF:**
```hlsl
fr_specular = (D·F·G) / (4·(n·l)·(n·v))
```

여기서:
- `D`: Distribution (미세면 분포 함수)
- `F`: Fresnel (프레넬 반사율)
- `G`: Geometry (기하학적 감쇠 함수)

**Specular IBL 적분:**
```hlsl
Lspecular = ∫Ω fr_specular(ωi,ωo) Lenvironment(ωi) (n·ωi) dωi
```

**문제점:**
- **roughness와 view direction에 의존**
- 실시간 계산 불가능 (너무 복잡)
- 해결책: **Split Sum Approximation** 필요

---

## 3. IBL 전처리 (Pre-computation)

### Split Sum Approximation
Epic Games에서 개발한 근사 기법으로 복잡한 적분을 두 부분으로 분리:

```
∫Ω fr(l,v) Li(l) (n·l) dl ≈ 
(∫Ω Li(l) D(h) (n·l) dl) · (∫Ω fr(l,v) (n·l) dl / ∫Ω D(h) (n·l) dl)
```

이를 **두 개의 독립적인 텍스처**로 분리:

### 1) Prefiltered Environment Map

**개념:** 각 roughness 레벨에 대해 환경맵을 미리 필터링

```hlsl
// 의사코드: Prefilter Map 생성
for (int mip = 0; mip < maxMipLevels; ++mip) {
    float roughness = mip / (maxMipLevels - 1);
    
    for (각 큐브맵 픽셀) {
        float3 N = GetCubemapDirection(face, u, v);
        float3 V = N; // 가정: View direction = Normal
        
        float3 color = 0;
        for (int sample = 0; sample < sampleCount; ++sample) {
            // 중요도 샘플링으로 방향 생성
            float3 H = ImportanceSampleGGX(u1, u2, roughness, N);
            float3 L = 2 * dot(V, H) * H - V;
            
            if (dot(N, L) > 0) {
                color += SampleEnvironment(L) * dot(N, L);
            }
        }
        prefilterMap[mip][pixel] = color / sampleCount;
    }
}
```

**결과:** 
- Mip 0: 거울 반사 (roughness = 0)
- Mip 4: 완전 확산 (roughness = 1)

### 2) BRDF Integration Map (2D LUT)

**개념:** (NdotV, roughness) → (scale, bias) 매핑 테이블

```hlsl
// 의사코드: BRDF LUT 생성
float2 IntegrateBRDF(float NdotV, float roughness) {
    float3 V = float3(sqrt(1 - NdotV*NdotV), 0, NdotV);
    float3 N = float3(0, 0, 1);
    
    float A = 0, B = 0;
    for (int i = 0; i < sampleCount; ++i) {
        float2 Xi = Hammersley(i, sampleCount);
        float3 H = ImportanceSampleGGX(Xi, roughness, N);
        float3 L = 2 * dot(V, H) * H - V;
        
        float NdotL = saturate(L.z);
        float NdotH = saturate(H.z);
        float VdotH = saturate(dot(V, H));
        
        if (NdotL > 0) {
            float G = GeometrySmith(N, V, L, roughness);
            float G_Vis = G * VdotH / (NdotH * NdotV);
            float Fc = pow(1 - VdotH, 5);
            
            A += (1 - Fc) * G_Vis;
            B += Fc * G_Vis;
        }
    }
    return float2(A, B) / sampleCount;
}
```

**결과:** 512×512 2D 텍스처 (RG 포맷)

---

## 4. 실시간 IBL 계산

### 최종 셰이더 구현

```hlsl
float3 CalculateIBL(float3 N, float3 V, float3 albedo, float metallic, float roughness) {
    float NdotV = max(dot(N, V), 0.0);
    
    // Fresnel 계산 (Schlick 근사)
    float3 F0 = lerp(float3(0.04, 0.04, 0.04), albedo, metallic);
    float3 F = F0 + (1.0 - F0) * pow(1.0 - NdotV, 5.0);
    
    // === Diffuse IBL ===
    float3 kS = F;                    // 반사된 에너지
    float3 kD = (1.0 - kS) * (1.0 - metallic); // 확산된 에너지
    float3 irradiance = SampleIrradiance(N);
    float3 diffuse = kD * albedo * irradiance;
    
    // === Specular IBL ===
    float3 R = reflect(-V, N);
    float3 prefilteredColor = SamplePrefilter(R, roughness);
    float2 brdf = SampleBRDF(NdotV, roughness);
    float3 specular = prefilteredColor * (F0 * brdf.x + brdf.y);
    
    return diffuse + specular;
}

// 텍스처 샘플링 함수들
float3 SampleIrradiance(float3 N) {
    return irradianceMap.Sample(sampler, N).rgb;
}

float3 SamplePrefilter(float3 R, float roughness) {
    float lod = roughness * maxReflectionLOD;
    return prefilterMap.SampleLevel(sampler, R, lod).rgb;
}

float2 SampleBRDF(float NdotV, float roughness) {
    return brdfLUT.Sample(sampler, float2(NdotV, roughness)).rg;
}
```

---

## 5. 수학적 세부사항

### Importance Sampling (중요도 샘플링)

**GGX 분포에 따른 중요도 샘플링:**
```hlsl
float3 ImportanceSampleGGX(float2 Xi, float roughness, float3 N) {
    float a = roughness * roughness;
    
    // 구면 좌표계에서 방향 생성
    float phi = 2.0 * PI * Xi.x;
    float cosTheta = sqrt((1.0 - Xi.y) / (1.0 + (a*a - 1.0) * Xi.y));
    float sinTheta = sqrt(1.0 - cosTheta*cosTheta);
    
    // 접선 공간에서 벡터 생성
    float3 H;
    H.x = cos(phi) * sinTheta;
    H.y = sin(phi) * sinTheta;
    H.z = cosTheta;
    
    // 월드 공간으로 변환
    float3 up = abs(N.z) < 0.999 ? float3(0,0,1) : float3(1,0,0);
    float3 tangent = normalize(cross(up, N));
    float3 bitangent = cross(N, tangent);
    
    return tangent * H.x + bitangent * H.y + N * H.z;
}
```

### Hammersley 시퀀스 (저불일치 시퀀스)

**균등하게 분포된 샘플 생성:**
```hlsl
float2 Hammersley(uint i, uint N) {
    uint bits = i;
    bits = (bits << 16u) | (bits >> 16u);
    bits = ((bits & 0x55555555u) << 1u) | ((bits & 0xAAAAAAAAu) >> 1u);
    bits = ((bits & 0x33333333u) << 2u) | ((bits & 0xCCCCCCCCu) >> 2u);
    bits = ((bits & 0x0F0F0F0Fu) << 4u) | ((bits & 0xF0F0F0F0u) >> 4u);
    bits = ((bits & 0x00FF00FFu) << 8u) | ((bits & 0xFF00FF00u) >> 8u);
    float rdi = float(bits) * 2.3283064365386963e-10;
    return float2(float(i) / float(N), rdi);
}
```

### GGX Distribution Function

```hlsl
float DistributionGGX(float3 N, float3 H, float roughness) {
    float a = roughness * roughness;
    float a2 = a * a;
    float NdotH = max(dot(N, H), 0.0);
    float NdotH2 = NdotH * NdotH;
    
    float num = a2;
    float denom = (NdotH2 * (a2 - 1.0) + 1.0);
    denom = PI * denom * denom;
    
    return num / denom;
}
```

### Geometry Function (Smith)

```hlsl
float GeometrySchlickGGX(float NdotV, float roughness) {
    float r = (roughness + 1.0);
    float k = (r * r) / 8.0;
    
    float num = NdotV;
    float denom = NdotV * (1.0 - k) + k;
    
    return num / denom;
}

float GeometrySmith(float3 N, float3 V, float3 L, float roughness) {
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    float ggx2 = GeometrySchlickGGX(NdotV, roughness);
    float ggx1 = GeometrySchlickGGX(NdotL, roughness);
    
    return ggx1 * ggx2;
}
```

---

## 6. IBL의 물리적 정확성

### 에너지 보존 (Energy Conservation)

```hlsl
// Fresnel이 에너지 보존을 보장
float3 kS = F;           // 반사된 에너지 비율
float3 kD = 1.0 - kS;    // 확산된 에너지 비율
// kS + kD = 1.0 (에너지 보존 법칙)

// 금속은 확산 반사가 없음
kD *= (1.0 - metallic);
```

**물리적 의미:**
- 들어온 빛의 에너지 = 반사된 에너지 + 확산된 에너지
- 금속은 자유 전자로 인해 확산 반사 없음
- 유전체는 반사와 확산 모두 존재

### 상호성 (Reciprocity)

BRDF는 입사각과 반사각을 바꿔도 동일한 값:
```
fr(ωi, ωo) = fr(ωo, ωi)
```

### 헬름홀츠 상호성 (Helmholtz Reciprocity)

빛의 경로를 역방향으로 추적해도 동일한 결과.

---

## 7. 성능 최적화

### Mipmap 체인 활용

```hlsl
// Roughness에 따른 LOD 계산
float lod = roughness * maxReflectionLOD;
float3 prefilteredColor = prefilterMap.SampleLevel(sampler, R, lod);
```

**원리:**
- Roughness 0 → Mip 0 (선명한 반사)
- Roughness 1 → Mip 4 (흐린 반사)

### 메모리 최적화

| 텍스처 | 크기 | 포맷 | 용도 |
|--------|------|------|------|
| Environment Map | 512×512×6 | R16G16B16A16F | 원본 환경맵 |
| Irradiance Map | 64×64×6 | R16G16B16A16F | Diffuse IBL |
| Prefilter Map | 256×256×6×5 | R16G16B16A16F | Specular IBL |
| BRDF LUT | 512×512 | R16G16F | BRDF 적분 |

### GPU 메모리 사용량 계산

```
Environment: 512×512×6×8 = 12.6 MB
Irradiance: 64×64×6×8 = 0.2 MB  
Prefilter: 256×256×6×5×8 = 20.5 MB
BRDF LUT: 512×512×4 = 1.0 MB
총합: ~34.3 MB
```

---

## 8. 실제 구현에서의 고려사항

### 감마 보정

```hlsl
// HDR 환경맵은 선형 공간에 저장
// 최종 출력 시 감마 보정 필요
float3 finalColor = pow(hdrColor, 1.0/2.2);
```

### 톤매핑과의 연계

```hlsl
// 렌더링 파이프라인
float3 hdrResult = CalculateIBL(...);
float3 toneMappedResult = ToneMapping(hdrResult);
float3 finalResult = GammaCorrection(toneMappedResult);
```

### 노출 조정 (Exposure)

```hlsl
// IBL 강도 조정
float3 adjustedIBL = CalculateIBL(...) * exposure;
```

### 환경맵 회전

```hlsl
// 환경맵 회전 매트릭스 적용
float3 rotatedDirection = mul(rotationMatrix, reflectionVector);
float3 envColor = SampleEnvironment(rotatedDirection);
```

---

## 9. IBL의 한계와 해결책

### 한계점

1. **정적 조명**
   - 환경맵이 고정되어 동적 조명 변화 반영 불가
   - 시간대 변화, 날씨 변화 등 표현 어려움

2. **거리 정보 부족**
   - 환경맵은 무한 거리 가정
   - 가까운 객체의 반사 부정확

3. **자기 반사 부족**
   - 객체 간 상호 반사 없음
   - 실내 환경에서 부자연스러움

4. **메모리 사용량**
   - 고품질 IBL은 상당한 메모리 필요
   - 모바일 환경에서 제약

### 해결책

#### 1. Reflection Probes
```hlsl
// 여러 위치의 환경맵 블렌딩
float3 probe1 = SampleProbe(worldPos, probe1Pos);
float3 probe2 = SampleProbe(worldPos, probe2Pos);
float3 blended = lerp(probe1, probe2, blendWeight);
```

#### 2. Parallax Correction
```hlsl
// 박스 투영으로 거리 보정
float3 CorrectParallax(float3 reflectDir, float3 worldPos, float3 probePos, float3 boxMin, float3 boxMax) {
    float3 firstPlaneIntersect = (boxMax - worldPos) / reflectDir;
    float3 secondPlaneIntersect = (boxMin - worldPos) / reflectDir;
    float3 furthestPlane = max(firstPlaneIntersect, secondPlaneIntersect);
    float distance = min(min(furthestPlane.x, furthestPlane.y), furthestPlane.z);
    float3 intersectPos = worldPos + reflectDir * distance;
    return intersectPos - probePos;
}
```

#### 3. Screen Space Reflections (SSR)
```hlsl
// IBL과 SSR 블렌딩
float3 ssrColor = ScreenSpaceReflection(screenPos, reflectDir);
float3 iblColor = CalculateIBL(N, V, material);
float3 finalColor = lerp(iblColor, ssrColor, ssrWeight);
```

#### 4. 동적 환경맵 업데이트
```cpp
// 실시간 환경맵 캡처 (성능 비용 높음)
void UpdateEnvironmentMap() {
    for (int face = 0; face < 6; ++face) {
        RenderToCubemapFace(face, probePosition);
    }
    GenerateIrradianceMap();
    GeneratePrefilterMap();
}
```

---

## 결론

IBL은 **물리 기반 렌더링의 핵심 기술**로서:

### 장점
- **사실적인 조명**: 실제 환경의 복잡한 조명 재현
- **효율성**: 수천 개의 광원을 하나의 환경맵으로 대체
- **일관성**: 물리 법칙을 따르는 정확한 조명

### 핵심 개념
- **Split Sum Approximation**: 복잡한 적분을 두 부분으로 분리
- **Importance Sampling**: 효율적인 몬테카를로 적분
- **에너지 보존**: 물리적으로 정확한 조명 모델

### 실용적 가치
- **AAA 게임**: 대부분의 현대 게임에서 표준 기술
- **영화 VFX**: 실사와 CG의 자연스러운 합성
- **제품 시각화**: 사실적인 재질 표현

IBL은 단순한 기술이 아닌 **광학과 수학의 정교한 결합**으로, 컴퓨터 그래픽스에서 사실적인 조명을 구현하는 가장 효과적인 방법 중 하나입니다.
