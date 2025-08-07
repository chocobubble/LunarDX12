struct PixelIn
{
    float4 posH : SV_POSITION;
    float4 color : COLOR;
    float2 texCoord : TEXCOORD0;
    float3 worldPos : TEXCOORD1;
    float size : TEXCOORD2;
    float rotation : TEXCOORD3;
    uint blendMode : TEXCOORD4;
};

cbuffer ParticleConstants : register(b1)
{
    float4x4 viewProjection;
    float3 cameraPosition;
    float deltaTime;
    float2 screenSize;
    float softParticleDistance;
    float padding;
};

// 텍스처들
Texture2D particleTexture : register(t0);
Texture2D depthTexture : register(t1);  // 깊이 소프트닝용
SamplerState particleSampler : register(s0);
SamplerState depthSampler : register(s1);

float4 main(PixelIn pIn) : SV_TARGET 
{
    // 회전 적용된 텍스처 좌표 계산
    float2 centeredCoord = pIn.texCoord - 0.5;
    float cosRot = cos(pIn.rotation);
    float sinRot = sin(pIn.rotation);
    float2 rotatedCoord;
    rotatedCoord.x = centeredCoord.x * cosRot - centeredCoord.y * sinRot;
    rotatedCoord.y = centeredCoord.x * sinRot + centeredCoord.y * cosRot;
    rotatedCoord += 0.5;
    
    // 텍스처 샘플링
    float4 texColor = particleTexture.Sample(particleSampler, rotatedCoord);
    
    // 기본 색상과 텍스처 합성
    float4 finalColor = pIn.color * texColor;
    
    // 깊이 소프트닝 (Depth Softening)
    float2 screenPos = pIn.posH.xy / screenSize;
    float sceneDepth = depthTexture.Sample(depthSampler, screenPos).r;
    float particleDepth = pIn.posH.z;
    
    float depthDiff = sceneDepth - particleDepth;
    float softFactor = saturate(depthDiff / softParticleDistance);
    finalColor.a *= softFactor;
    
    // 블렌딩 모드별 처리
    switch(pIn.blendMode)
    {
        case 0: // Alpha Blending
            // 중심에서 가장자리로 갈수록 투명해지는 효과
            {
                float2 center = pIn.texCoord - 0.5;
                float distance = length(center);
                float fadeOut = 1.0 - smoothstep(0.3, 0.5, distance);
                finalColor.a *= fadeOut;
            }
            break;
            
        case 1: // Additive Blending
            // 중심이 가장 밝고 가장자리로 갈수록 어두워짐
            {
                float2 center = pIn.texCoord - 0.5;
                float distance = length(center);
                float intensity = 1.0 - smoothstep(0.0, 0.5, distance);
                finalColor.rgb *= intensity;
                finalColor.a = 1.0; // Additive는 알파를 1.0으로 고정
            }
            break;
            
        case 2: // Multiplicative Blending
            // 그림자 효과 - 중심이 어둡고 가장자리가 밝음
            {
                float2 center = pIn.texCoord - 0.5;
                float distance = length(center);
                float darkness = smoothstep(0.0, 0.5, distance);
                finalColor.rgb = lerp(finalColor.rgb, float3(1.0, 1.0, 1.0), darkness);
                finalColor.a = 1.0;
            }
            break;
            
        case 3: // Screen Blending
            // 부드러운 빛 효과
            {
                float2 center = pIn.texCoord - 0.5;
                float distance = length(center);
                float glow = 1.0 - smoothstep(0.0, 0.5, distance);
                finalColor.rgb *= glow * 0.8; // 약간 어둡게 해서 자연스럽게
                finalColor.a = glow;
            }
            break;
            
        case 4: // Subtractive Blending
            // 특수 효과 - 색상 반전
            {
                finalColor.rgb = 1.0 - finalColor.rgb;
                finalColor.a = pIn.color.a;
            }
            break;
    }
    
    // 거리에 따른 페이드 아웃 (LOD)
    float distanceToCamera = length(pIn.worldPos - cameraPosition);
    float distanceFade = 1.0 - smoothstep(50.0, 100.0, distanceToCamera);
    finalColor.a *= distanceFade;
    
    // 알파 테스트 (성능 최적화)
    if (finalColor.a < 0.01)
        discard;
    
    return finalColor;
}
