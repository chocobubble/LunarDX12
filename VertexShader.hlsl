cbuffer BasicConstants : register(b0)
{
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
    pIn.pos = float4(vIn.pos, 1.0f);
    pIn.color = vIn.color;
	// for test
	pIn.pos = float4(vIn.pos + eyeWorld, 1.0f);
    pIn.color = vIn.color + float4(eyeWorld, 0.0f);
    return pIn;
}