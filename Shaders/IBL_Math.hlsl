// IBL_Math.hlsl - IBL 구현에 필요한 수학 함수들

#ifndef IBL_MATH_HLSL
#define IBL_MATH_HLSL

static const float PI = 3.14159265359;

// ============================================================================
// 기본 수학 함수들
// ============================================================================

// Van Der Corput 시퀀스 (Hammersley의 일부)
float RadicalInverse_VdC(uint bits) {
    bits = (bits << 16u) | (bits >> 16u);
    bits = ((bits & 0x55555555u) << 1u) | ((bits & 0xAAAAAAAAu) >> 1u);
    bits = ((bits & 0x33333333u) << 2u) | ((bits & 0xCCCCCCCCu) >> 2u);
    bits = ((bits & 0x0F0F0F0Fu) << 4u) | ((bits & 0xF0F0F0F0u) >> 4u);
    bits = ((bits & 0x00FF00FFu) << 8u) | ((bits & 0xFF00FF00u) >> 8u);
    return float(bits) * 2.3283064365386963e-10; // / 0x100000000
}

// Hammersley 저불일치 시퀀스
float2 Hammersley(uint i, uint N) {
    return float2(float(i) / float(N), RadicalInverse_VdC(i));
}

// ============================================================================
// BRDF 관련 함수들
// ============================================================================

// GGX/Trowbridge-Reitz 분포 함수
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

// Schlick-GGX 기하학적 감쇠 함수
float GeometrySchlickGGX(float NdotV, float roughness) {
    float r = (roughness + 1.0);
    float k = (r * r) / 8.0; // Direct lighting용
    
    float num = NdotV;
    float denom = NdotV * (1.0 - k) + k;
    
    return num / denom;
}

// IBL용 Schlick-GGX (k 값이 다름)
float GeometrySchlickGGX_IBL(float NdotV, float roughness) {
    float a = roughness;
    float k = (a * a) / 2.0; // IBL용
    
    float num = NdotV;
    float denom = NdotV * (1.0 - k) + k;
    
    return num / denom;
}

// Smith 기하학적 감쇠 함수
float GeometrySmith(float3 N, float3 V, float3 L, float roughness) {
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    float ggx2 = GeometrySchlickGGX(NdotV, roughness);
    float ggx1 = GeometrySchlickGGX(NdotL, roughness);
    
    return ggx1 * ggx2;
}

// IBL용 Smith 기하학적 감쇠 함수
float GeometrySmith_IBL(float3 N, float3 V, float3 L, float roughness) {
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    float ggx2 = GeometrySchlickGGX_IBL(NdotV, roughness);
    float ggx1 = GeometrySchlickGGX_IBL(NdotL, roughness);
    
    return ggx1 * ggx2;
}

// Schlick 프레넬 근사
float3 FresnelSchlick(float cosTheta, float3 F0) {
    return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

// Roughness를 고려한 프레넬 (IBL용)
float3 FresnelSchlickRoughness(float cosTheta, float3 F0, float roughness) {
    return F0 + (max(float3(1.0 - roughness, 1.0 - roughness, 1.0 - roughness), F0) - F0) * 
           pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

// ============================================================================
// 중요도 샘플링 함수들
// ============================================================================

// GGX 분포에 따른 중요도 샘플링
float3 ImportanceSampleGGX(float2 Xi, float3 N, float roughness) {
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
    
    // 접선 공간에서 월드 공간으로 변환
    float3 up = abs(N.z) < 0.999 ? float3(0.0, 0.0, 1.0) : float3(1.0, 0.0, 0.0);
    float3 tangent = normalize(cross(up, N));
    float3 bitangent = cross(N, tangent);
    
    float3 sampleVec = tangent * H.x + bitangent * H.y + N * H.z;
    return normalize(sampleVec);
}

// 균등 반구 샘플링
float3 UniformSampleHemisphere(float2 Xi) {
    float phi = Xi.y * 2.0 * PI;
    float cosTheta = Xi.x;
    float sinTheta = sqrt(1.0 - cosTheta * cosTheta);
    
    return float3(cos(phi) * sinTheta, sin(phi) * sinTheta, cosTheta);
}

// 코사인 가중 반구 샘플링 (Lambert 분포)
float3 CosineSampleHemisphere(float2 Xi) {
    float phi = Xi.y * 2.0 * PI;
    float cosTheta = sqrt(Xi.x);
    float sinTheta = sqrt(1.0 - Xi.x);
    
    return float3(cos(phi) * sinTheta, sin(phi) * sinTheta, cosTheta);
}

// ============================================================================
// 좌표계 변환 함수들
// ============================================================================

// 접선 공간 기저 벡터 생성
void GetTangentBasis(float3 N, out float3 T, out float3 B) {
    float3 up = abs(N.z) < 0.999 ? float3(0.0, 0.0, 1.0) : float3(1.0, 0.0, 0.0);
    T = normalize(cross(up, N));
    B = cross(N, T);
}

// 접선 공간에서 월드 공간으로 변환
float3 TangentToWorld(float3 vec, float3 N, float3 T, float3 B) {
    return vec.x * T + vec.y * B + vec.z * N;
}

// 월드 공간에서 접선 공간으로 변환
float3 WorldToTangent(float3 vec, float3 N, float3 T, float3 B) {
    return float3(dot(vec, T), dot(vec, B), dot(vec, N));
}

// ============================================================================
// 큐브맵 관련 함수들
// ============================================================================

// 큐브맵 면과 UV에서 방향 벡터 계산
float3 GetCubemapDirection(int face, float2 uv) {
    // UV를 [-1, 1] 범위로 변환
    float2 texCoord = uv * 2.0 - 1.0;
    
    switch (face) {
        case 0: // +X
            return normalize(float3(1.0, -texCoord.y, -texCoord.x));
        case 1: // -X
            return normalize(float3(-1.0, -texCoord.y, texCoord.x));
        case 2: // +Y
            return normalize(float3(texCoord.x, 1.0, texCoord.y));
        case 3: // -Y
            return normalize(float3(texCoord.x, -1.0, -texCoord.y));
        case 4: // +Z
            return normalize(float3(texCoord.x, -texCoord.y, 1.0));
        case 5: // -Z
            return normalize(float3(-texCoord.x, -texCoord.y, -1.0));
        default:
            return float3(0.0, 0.0, 1.0);
    }
}

// 방향 벡터에서 큐브맵 면과 UV 계산
void DirectionToCubemap(float3 dir, out int face, out float2 uv) {
    float3 absDir = abs(dir);
    float maxAxis = max(max(absDir.x, absDir.y), absDir.z);
    
    if (maxAxis == absDir.x) {
        if (dir.x > 0.0) {
            face = 0; // +X
            uv = float2(-dir.z, -dir.y) / dir.x;
        } else {
            face = 1; // -X
            uv = float2(dir.z, -dir.y) / (-dir.x);
        }
    } else if (maxAxis == absDir.y) {
        if (dir.y > 0.0) {
            face = 2; // +Y
            uv = float2(dir.x, dir.z) / dir.y;
        } else {
            face = 3; // -Y
            uv = float2(dir.x, -dir.z) / (-dir.y);
        }
    } else {
        if (dir.z > 0.0) {
            face = 4; // +Z
            uv = float2(dir.x, -dir.y) / dir.z;
        } else {
            face = 5; // -Z
            uv = float2(-dir.x, -dir.y) / (-dir.z);
        }
    }
    
    // [-1, 1]에서 [0, 1]로 변환
    uv = uv * 0.5 + 0.5;
}

// ============================================================================
// Equirectangular 변환 함수들
// ============================================================================

// 방향 벡터를 Equirectangular UV로 변환
float2 DirectionToEquirectangular(float3 dir) {
    float phi = atan2(dir.z, dir.x);
    float theta = acos(dir.y);
    
    float u = phi / (2.0 * PI) + 0.5;
    float v = theta / PI;
    
    return float2(u, v);
}

// Equirectangular UV를 방향 벡터로 변환
float3 EquirectangularToDirection(float2 uv) {
    float phi = (uv.x - 0.5) * 2.0 * PI;
    float theta = uv.y * PI;
    
    float sinTheta = sin(theta);
    float cosTheta = cos(theta);
    float sinPhi = sin(phi);
    float cosPhi = cos(phi);
    
    return float3(sinTheta * cosPhi, cosTheta, sinTheta * sinPhi);
}

// ============================================================================
// 유틸리티 함수들
// ============================================================================

// 안전한 정규화 (0 벡터 처리)
float3 SafeNormalize(float3 v) {
    float len = length(v);
    return len > 0.0 ? v / len : float3(0.0, 0.0, 1.0);
}

// 벡터의 길이 제곱
float LengthSquared(float3 v) {
    return dot(v, v);
}

// 선형 보간 (3D)
float3 Lerp3(float3 a, float3 b, float t) {
    return a + t * (b - a);
}

// 구면 선형 보간
float3 Slerp(float3 a, float3 b, float t) {
    float dot_product = dot(a, b);
    dot_product = clamp(dot_product, -1.0, 1.0);
    
    float theta = acos(dot_product) * t;
    float3 relative_vec = normalize(b - a * dot_product);
    
    return a * cos(theta) + relative_vec * sin(theta);
}

// 감마 보정
float3 LinearToGamma(float3 color) {
    return pow(color, 1.0 / 2.2);
}

float3 GammaToLinear(float3 color) {
    return pow(color, 2.2);
}

// Luminance 계산
float Luminance(float3 color) {
    return dot(color, float3(0.2126, 0.7152, 0.0722));
}

#endif // IBL_MATH_HLSL
