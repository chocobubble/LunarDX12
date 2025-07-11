#include "Common.hlsl"

struct VertexOut
{
	float4 pos : SV_POSITION;
	float4 color : COLOR;
	float2 texCoord : TEXCOORD;
	float3 normal : NORMAL;
	float3 posW : POSITION;
	float3 tangent : TANGENT;
};

struct HullOut
{
	float4 pos : SV_POSITION;
	float4 color : COLOR;
	float2 texCoord : TEXCOORD;
	float3 normal : NORMAL;
	float3 posW : POSITION;
	float3 tangent : TANGENT;
};

struct PatchConstOutput
{
	float edges[3] : SV_TessFactor;
	float inside : SV_InsideTessFactor;
};

PatchConstOutput PatchConstantFunc(InputPatch<VertexOut, 3> input, uint patchID : SV_PrimitiveID)
{
	float3 centerOfPatch = (input[0].posW + input[1].posW + input[2].posW) / 3.0;
	float distanceFromEye = length(centerOfPatch - eyePos);
	// REFACTOR: hardcoded
	float d0 = 2.0;
	float d1 = 10.0;
	float tess = 64.0f * saturate((d1 - distanceFromEye) / (d1 - d0)) + 1.0;
	
	PatchConstOutput output;
	output.edges[0] = tess;
	output.edges[1] = tess;
	output.edges[2] = tess;
	output.inside = tess;
	return output;
}

[domain("tri")]
[partitioning("integer")]
[outputtopology("triangle_cw")]
[outputcontrolpoints(3)]
[patchconstantfunc("PatchConstantFunc")]
[maxtessfactor(64)]
[mintessfactor(1)]
HullOut main(InputPatch<VertexOut, 3> input, uint i : SV_OutputControlPointID, uint patchID : SV_PrimitiveID)
{
	HullOut hullOut;
	hullOut.pos = input[i].pos;
	hullOut.color = input[i].color;
	hullOut.texCoord = input[i].texCoord;
	hullOut.normal = input[i].normal;
	hullOut.posW = input[i].posW;
	hullOut.tangent = input[i].tangent;
	return hullOut;
}