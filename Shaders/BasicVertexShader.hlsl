#include "Common.hlsl"

SamplerState g_sampler : register(s0);

Texture2D heightTexture : register(t6);

struct VertexIn
{
    float3 pos : POSITION;
    float4 color : COLOR;
	float2 texCoord : TEXCOORD;
	float3 normal : NORMAL;
	float3 tangent : TANGENT;
};

struct PixelIn
{
    float4 pos : SV_POSITION;
    float4 color : COLOR;
	float2 texCoord : TEXCOORD;
	float3 normal : NORMAL;
	float3 posW : POSITION;
	float3 tangent : TANGENT;
};

PixelIn main(VertexIn vIn)
{
    PixelIn pIn;
	
	float4 pos = float4(vIn.pos, 1.0f);
	float4 posW = mul(pos, world);
	
    pIn.normal = normalize(mul(vIn.normal, (float3x3)worldInvTranspose));
	pIn.tangent = mul(vIn.tangent, (float3x3)world);
	
    float heightScale = 0.2; // for now, hardcoded
    uint heightMapEnabledMask = 1 << 8;
    if ((debugFlags & heightMapEnabledMask) != 0)
    {
        float height = heightTexture.SampleLevel(g_sampler, vIn.texCoord, 0).r;
    	height = height * 2.0 - 1.0;
        posW.xyz += pIn.normal * height * heightScale;
    }
	
	pIn.posW = posW.xyz;
    float4 posWV = mul(posW, view); 
    pIn.pos = mul(posWV, projection);
	
    pIn.color = vIn.color;
	pIn.texCoord = vIn.texCoord;
	
    return pIn;
}