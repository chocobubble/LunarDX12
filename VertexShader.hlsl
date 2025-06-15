cbuffer BasicConstants : register(b0)
{
	float4x4 model;
	float4x4 view;
	float4x4 projection;
	float3 eyePos;
}

struct VertexIn
{
    float3 pos : POSITION;
    float4 color : COLOR;
};

struct PixelIn
{
    float4 pos : SV_POSITION;
    float4 color : COLOR;
};

PixelIn main(VertexIn vIn)
{
    PixelIn pIn;
	float4 pos = float4(vIn.pos, 1.0f);
	pIn.pos = mul(pos, model);
	pIn.pos = mul(pIn.pos, view);
	pIn.pos = mul(pIn.pos, projection);
    pIn.color = vIn.color;
    return pIn;
}