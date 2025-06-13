cbuffer BasicConstants : register(b0)
{
	float4x4 model;
	float4x4 view;
	float4x4 projection;
	float3 eyeWorld;
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
    pIn.color = vIn.color;
    return pIn;
}