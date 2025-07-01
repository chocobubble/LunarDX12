struct Light
{
	float3 lightStrength;
	float fallOffStart;
	float3 direction;
	float fallOffEnd;
	float3 position;
	float spotPower;
};

cbuffer BasicConstants : register(b0)
{
	float4x4 view;
	float4x4 projection;
	float3 eyePos;
	float4 ambientLight;
	Light lights[3];
}

cbuffer ObjectConstants : register(b1)
{
	float4x4 world;
	int textureIndex;	
}

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
	// pIn.pos = mul(pos, model);
	pIn.pos = mul(pos, world);
	pIn.posW = pIn.pos;
	pIn.pos = mul(pIn.pos, view);
	pIn.pos = mul(pIn.pos, projection);
    pIn.color = vIn.color;
	pIn.texCoord = vIn.texCoord;
	pIn.normal = vIn.normal;
	pIn.tangent = vIn.tangent;
    return pIn;
}