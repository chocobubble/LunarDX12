Texture2D g_textureWall : register(t0);
SamplerState g_sampler : register(s0);

struct PixelIn
{
	float4 pos : SV_POSITION;
	float2 texCoord : TEXCOORD;
};

float4 main(PixelIn pIn) : SV_TARGET
{
	return g_textureWall.Sample(g_sampler, pIn.texCoord);
}
