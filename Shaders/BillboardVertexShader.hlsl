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
	float4x4 worldInvTranspose;
	int textureIndex;
}

struct VertexIn
{
	float3 pos : POSITION;
};

struct GeometryIn
{
	float4 posH : SV_POSITION;
	float2 size : TEXCOORD;
};

GeometryIn main(VertexIn vIn)
{
	GeometryIn gIn;
	float4 posW = float4(vIn.pos, 1.0f);
	float4 posWV = mul(posW, view);
	gIn.posH = mul(posWV, projection);
	gIn.size = float2(4.0f, 4.0f); // for now, hard coded
	return gIn;
}