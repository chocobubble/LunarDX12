#include "Common.hlsl"

struct GeometryIn
{
	float4 posW : SV_POSITION;
	float3 normalW : NORMAL;
};

struct PixelIn
{
	float4 pos : SV_POSITION;
	float3 color : COLOR;
};

[maxvertexcount(2)]
void main(point GeometryIn gIn[1], inout LineStream<PixelIn> stream)
{
	PixelIn pIn;
	float normalLen = 0.1;
	
	// start point
	float4 normalStart = mul(mul(gIn[0].posW, view), projection);
	pIn.pos = normalStart;
	pIn.color = abs(gIn[0].normalW); 
	stream.Append(pIn);

	// end point
	float4 normalWorldEnd = gIn[0].posW + float4(gIn[0].normalW * normalLen, 0.0);
	float4 normalEnd = mul(mul(normalWorldEnd, view), projection);
	pIn.pos = normalEnd;
	stream.Append(pIn);
}
