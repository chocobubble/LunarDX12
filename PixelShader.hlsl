Texture2D g_texture : register(t0);
SamplerState g_sampler : register(s0);

struct PixelIn
{
	float4 pos : SV_POSITION;
	float4 color : COLOR;
	float2 texCoord : TEXCOORD;
};

float4 main(PixelIn pIn) : SV_TARGET
{
	return g_texture.Sample(g_sampler, pIn.texCoord);
}