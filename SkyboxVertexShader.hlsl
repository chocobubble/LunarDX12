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
	pIn.pos = float4(vIn.pos, 1.0);
	pIn.texCoord = vIn.texCoord;
	return pIn;
}