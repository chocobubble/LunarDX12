cbuffer BasicConstants : register(b0)
{
	float4x4 view;
	float4x4 projection;
	float3 eyePos;
	float4 ambientLight;
}

cbuffer ObjectConstants : register(b1)
{
	float4x4 world;
	int textureIndex;	
}

struct VertexIn
{
	float3 pos : POSITION;
	float4 color : COLOR;
	float2 texCoord : TEXCOORD;
	float3 normal : NORMAL;
};

struct PixelIn
{
	float4 pos : SV_POSITION;
	float3 texCoord : TEXCOORD;
};

PixelIn main(VertexIn vIn)
{
	PixelIn pIn;
	pIn.pos = float4(vIn.pos, 1.0);
	pIn.texCoord = vIn.pos;
	return pIn;
}