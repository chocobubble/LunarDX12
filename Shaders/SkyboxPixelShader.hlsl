// HDR Cubemap 텍스처 (3채널 RGB)
TextureCube<float3> skyCube : register(t9);
SamplerState g_sampler : register(s0);

struct PixelIn
{
	float4 pos : SV_POSITION;
	float3 texCoord : TEXCOORD;
};

// ACES tone mapping (더 자연스러운 결과)
float3 ACESToneMapping(float3 hdrColor)
{
    float a = 2.51;
    float b = 0.03;
    float c = 2.43;
    float d = 0.59;
    float e = 0.14;
    return saturate((hdrColor * (a * hdrColor + b)) / (hdrColor * (c * hdrColor + d) + e));
}

float4 main(PixelIn pIn) : SV_TARGET
{
	// 큐브맵 직접 샘플링 (3D 방향 벡터 사용)
	float3 hdrColor = skyCube.Sample(g_sampler, normalize(pIn.texCoord));
	
	// Tone mapping으로 HDR → LDR 변환
	float3 ldrColor = ACESToneMapping(hdrColor);
	
	// Gamma correction (Linear → sRGB)
	ldrColor = pow(ldrColor, 1.0/2.2);
	
	return float4(ldrColor, 1.0);
}
