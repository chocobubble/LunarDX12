// IBL_Precompute.hlsl - IBL 전처리용 컴퓨트 셰이더들

#include "IBL_Math.hlsl"

// ============================================================================
// 1. Equirectangular to Cubemap 변환
// ============================================================================

Texture2D<float4> equirectangularMap : register(t0);
RWTexture2DArray<float4> outputCubemap : register(u0);
SamplerState linearSampler : register(s0);

[numthreads(32, 32, 1)]
void EquirectangularToCubemapCS(uint3 id : SV_DispatchThreadID) {
    uint width, height, elements;
    outputCubemap.GetDimensions(width, height, elements);
    
    if (id.x >= width || id.y >= height || id.z >= 6) return;
    
    // 큐브맵 좌표를 방향 벡터로 변환
    float2 uv = (float2(id.xy) + 0.5) / float2(width, height);
    float3 direction = GetCubemapDirection(id.z, uv);
    
    // 방향 벡터를 Equirectangular UV로 변환
    float2 equiUV = DirectionToEquirectangular(direction);
    
    // 환경맵 샘플링
    float4 color = equirectangularMap.SampleLevel(linearSampler, equiUV, 0);
    
    outputCubemap[id] = color;
}

// ============================================================================
// 2. Irradiance Map 생성 (Diffuse IBL)
// ============================================================================

TextureCube<float4> environmentMap : register(t0);
RWTexture2DArray<float4> irradianceMap : register(u0);

cbuffer IrradianceConstants : register(b0) {
    uint sampleCount;
    float padding[3];
};

[numthreads(32, 32, 1)]
void GenerateIrradianceMapCS(uint3 id : SV_DispatchThreadID) {
    uint width, height, elements;
    irradianceMap.GetDimensions(width, height, elements);
    
    if (id.x >= width || id.y >= height || id.z >= 6) return;
    
    // 큐브맵 좌표를 법선 벡터로 변환
    float2 uv = (float2(id.xy) + 0.5) / float2(width, height);
    float3 N = GetCubemapDirection(id.z, uv);
    
    // 접선 공간 기저 벡터 생성
    float3 up = float3(0.0, 1.0, 0.0);
    float3 right = normalize(cross(up, N));
    up = normalize(cross(N, right));
    
    float3 irradiance = float3(0.0, 0.0, 0.0);
    float nrSamples = 0.0;
    
    // 반구에서 샘플링
    float sampleDelta = 0.025; // 샘플링 간격
    for (float phi = 0.0; phi < 2.0 * PI; phi += sampleDelta) {
        for (float theta = 0.0; theta < 0.5 * PI; theta += sampleDelta) {
            // 구면 좌표를 직교 좌표로 변환
            float3 tangentSample = float3(sin(theta) * cos(phi), sin(theta) * sin(phi), cos(theta));
            
            // 접선 공간에서 월드 공간으로 변환
            float3 sampleVec = tangentSample.x * right + tangentSample.y * up + tangentSample.z * N;
            
            // 환경맵 샘플링
            float3 envColor = environmentMap.SampleLevel(linearSampler, sampleVec, 0).rgb;
            irradiance += envColor * cos(theta) * sin(theta);
            nrSamples++;
        }
    }
    
    irradiance = PI * irradiance * (1.0 / nrSamples);
    irradianceMap[id] = float4(irradiance, 1.0);
}

// ============================================================================
// 3. Prefiltered Environment Map 생성 (Specular IBL)
// ============================================================================

RWTexture2DArray<float4> prefilterMap : register(u0);

cbuffer PrefilterConstants : register(b0) {
    float roughness;
    uint sampleCount;
    float2 padding;
};

[numthreads(32, 32, 1)]
void GeneratePrefilterMapCS(uint3 id : SV_DispatchThreadID) {
    uint width, height, elements;
    prefilterMap.GetDimensions(width, height, elements);
    
    if (id.x >= width || id.y >= height || id.z >= 6) return;
    
    // 큐브맵 좌표를 방향 벡터로 변환
    float2 uv = (float2(id.xy) + 0.5) / float2(width, height);
    float3 N = GetCubemapDirection(id.z, uv);
    
    // 가정: View direction = Normal (distant viewer)
    float3 R = N;
    float3 V = R;
    
    float3 prefilteredColor = float3(0.0, 0.0, 0.0);
    float totalWeight = 0.0;
    
    // 몬테카를로 적분
    for (uint i = 0; i < sampleCount; ++i) {
        // 저불일치 시퀀스로 샘플 생성
        float2 Xi = Hammersley(i, sampleCount);
        
        // GGX 중요도 샘플링으로 Half vector 생성
        float3 H = ImportanceSampleGGX(Xi, N, roughness);
        float3 L = normalize(2.0 * dot(V, H) * H - V);
        
        float NdotL = max(dot(N, L), 0.0);
        if (NdotL > 0.0) {
            // 환경맵에서 샘플링
            // 높은 roughness에서는 낮은 해상도 mip 사용
            float D = DistributionGGX(N, H, roughness);
            float NdotH = max(dot(N, H), 0.0);
            float HdotV = max(dot(H, V), 0.0);
            float pdf = D * NdotH / (4.0 * HdotV) + 0.0001;
            
            float resolution = float(width); // 큐브맵 해상도
            float saTexel = 4.0 * PI / (6.0 * resolution * resolution);
            float saSample = 1.0 / (float(sampleCount) * pdf + 0.0001);
            
            float mipLevel = roughness == 0.0 ? 0.0 : 0.5 * log2(saSample / saTexel);
            
            float3 envColor = environmentMap.SampleLevel(linearSampler, L, mipLevel).rgb;
            prefilteredColor += envColor * NdotL;
            totalWeight += NdotL;
        }
    }
    
    prefilteredColor = prefilteredColor / totalWeight;
    prefilterMap[id] = float4(prefilteredColor, 1.0);
}

// ============================================================================
// 4. BRDF Integration Map 생성 (2D LUT)
// ============================================================================

RWTexture2D<float2> brdfLUT : register(u0);

[numthreads(32, 32, 1)]
void GenerateBRDFLUTCS(uint3 id : SV_DispatchThreadID) {
    uint width, height;
    brdfLUT.GetDimensions(width, height);
    
    if (id.x >= width || id.y >= height) return;
    
    float NdotV = (float(id.x) + 0.5) / float(width);
    float roughness = (float(id.y) + 0.5) / float(height);
    
    // 접선 공간에서 계산 (N = (0,0,1))
    float3 V = float3(sqrt(1.0 - NdotV * NdotV), 0.0, NdotV);
    float3 N = float3(0.0, 0.0, 1.0);
    
    float A = 0.0;
    float B = 0.0;
    
    const uint SAMPLE_COUNT = 1024;
    for (uint i = 0; i < SAMPLE_COUNT; ++i) {
        float2 Xi = Hammersley(i, SAMPLE_COUNT);
        float3 H = ImportanceSampleGGX(Xi, N, roughness);
        float3 L = normalize(2.0 * dot(V, H) * H - V);
        
        float NdotL = max(L.z, 0.0);
        float NdotH = max(H.z, 0.0);
        float VdotH = max(dot(V, H), 0.0);
        
        if (NdotL > 0.0) {
            float G = GeometrySmith_IBL(N, V, L, roughness);
            float G_Vis = (G * VdotH) / (NdotH * NdotV);
            float Fc = pow(1.0 - VdotH, 5.0);
            
            A += (1.0 - Fc) * G_Vis;
            B += Fc * G_Vis;
        }
    }
    
    A /= float(SAMPLE_COUNT);
    B /= float(SAMPLE_COUNT);
    
    brdfLUT[id.xy] = float2(A, B);
}

// ============================================================================
// 5. 환경맵 회전 (선택사항)
// ============================================================================

cbuffer RotationConstants : register(b0) {
    float4x4 rotationMatrix;
};

[numthreads(32, 32, 1)]
void RotateEnvironmentMapCS(uint3 id : SV_DispatchThreadID) {
    uint width, height, elements;
    outputCubemap.GetDimensions(width, height, elements);
    
    if (id.x >= width || id.y >= height || id.z >= 6) return;
    
    // 큐브맵 좌표를 방향 벡터로 변환
    float2 uv = (float2(id.xy) + 0.5) / float2(width, height);
    float3 direction = GetCubemapDirection(id.z, uv);
    
    // 회전 적용
    float3 rotatedDirection = mul((float3x3)rotationMatrix, direction);
    
    // 원본 환경맵에서 샘플링
    float4 color = environmentMap.SampleLevel(linearSampler, rotatedDirection, 0);
    
    outputCubemap[id] = color;
}

// ============================================================================
// 6. 환경맵 다운샘플링 (Mip 생성)
// ============================================================================

TextureCube<float4> inputCubemap : register(t0);
RWTexture2DArray<float4> outputMip : register(u0);

cbuffer MipConstants : register(b0) {
    uint mipLevel;
    float3 padding;
};

[numthreads(32, 32, 1)]
void GenerateCubemapMipsCS(uint3 id : SV_DispatchThreadID) {
    uint width, height, elements;
    outputMip.GetDimensions(width, height, elements);
    
    if (id.x >= width || id.y >= height || id.z >= 6) return;
    
    // 현재 mip 레벨의 UV 계산
    float2 uv = (float2(id.xy) + 0.5) / float2(width, height);
    float3 direction = GetCubemapDirection(id.z, uv);
    
    // 4개 샘플의 평균 (박스 필터)
    float texelSize = 1.0 / float(width);
    float3 color = float3(0.0, 0.0, 0.0);
    
    // 2x2 박스 필터
    for (int x = -1; x <= 1; x += 2) {
        for (int y = -1; y <= 1; y += 2) {
            float2 offset = float2(x, y) * texelSize * 0.5;
            float2 sampleUV = uv + offset;
            float3 sampleDir = GetCubemapDirection(id.z, sampleUV);
            color += inputCubemap.SampleLevel(linearSampler, sampleDir, mipLevel - 1).rgb;
        }
    }
    
    color /= 4.0;
    outputMip[id] = float4(color, 1.0);
}
