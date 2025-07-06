cbuffer BasicConstants : register(b0)
{
	float4x4 view;
	float4x4 projection;
	float3 eyePos;
}

cbuffer ObjectConstants : register(b1)
{
	float4x4 world;
	float4x4 worldInvTranspose;
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
	float2 texCoord : TEXCOORD;
};

PixelIn main(VertexIn vIn)
{
	PixelIn pIn;
	float4 pos = float4(vIn.pos, 1.0);
	float4 posV = mul(pos, view);
	float4 posVP = mul(posV, projection);
	pIn.pos = posVP;
	pIn.texCoord = vIn.texCoord;
	return pIn;
}
