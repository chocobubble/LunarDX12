struct Light
{
	float3 strength;
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
	float4 eyePos;
	float4 ambientLight;
	Light lights[3];
	float4x4 shadowTransform;
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
    float4 color : COLOR;
	float2 texCoord : TEXCOORD;
	float3 normal : NORMAL;
};

struct PixelIn
{
    float4 pos : SV_POSITION;
    float4 color : COLOR;
	float2 texCoord : TEXCOORD;
	float3 normal : NORMAL;
	float3 posW : POSITION0;
	float3 eyePosW : POSITION1;
};

PixelIn main(VertexIn vIn)
{
    PixelIn pIn;
	float4 pos = float4(vIn.pos, 1.0f);
	pIn.pos = mul(pos, world);
	// pIn.pos = pos;
	pIn.posW = mul(pos, world);
	pIn.pos = mul(pIn.pos, view);
	pIn.pos = mul(pIn.pos, projection);
    pIn.color = vIn.color;
	pIn.texCoord = vIn.texCoord;
	pIn.normal = normalize(mul(vIn.normal, (float3x3)(worldInvTranspose)));
	pIn.eyePosW = mul(eyePos, world);
    return pIn;
}