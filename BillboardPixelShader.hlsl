Texture2D g_textureWall : register(t0);
Texture2D g_textureTree1 : register(t1);
Texture2D g_textureTree2 : register(t2);
SamplerState g_sampler : register(s0);

struct PixelInput
{
	float4 posH : SV_POSITION;
	float2 uv : TEXCOORD;
	uint primID : SV_PrimitiveID;
};

float4 main(PixelInput pIn) : SV_TARGET
{
	return pIn.primID % 2 == 0 ?
		g_textureTree2.Sample(g_sampler, pIn.uv) :
		g_textureTree1.Sample(g_sampler, pIn.uv);
}


