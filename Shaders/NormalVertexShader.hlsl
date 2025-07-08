#include "Common.hlsl"

struct VertexIn
{
	float3 pos : POSITION;
	float4 color : COLOR;
	float2 texCoord : TEXCOORD;
	float3 normal : NORMAL;
	float3 tangent : TANGENT;
};

struct GeometryIn
{
	float4 posW : SV_POSITION;
	float3 normalW : NORMAL;
};

GeometryIn main(VertexIn vIn)
{
	GeometryIn gIn;
	gIn.posW = mul(float4(vIn.pos, 1.0), world);
	gIn.normalW = normalize(mul(vIn.normal, (float3x3)worldInvTranspose)); 
	return gIn;
}
