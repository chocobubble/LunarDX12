TextureCube<float4> skyCube : register(t11);
SamplerState g_sampler : register(s0);

struct PixelIn
{
	float4 pos : SV_POSITION;
	float3 texCoord : TEXCOORD;
};

float4 main(PixelIn pIn) : SV_TARGET
{
	return skyCube.Sample(g_sampler, pIn.texCoord);
}
