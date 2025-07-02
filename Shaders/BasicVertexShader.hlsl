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
	float4 posW = mul(pos, world);
	pIn.posW = posW.xyz; 
	
    float4 posWV = mul(posW, view); 
    float4 posWVP = mul(posWV, projection);
    pIn.pos = posWVP;
	
    pIn.color = vIn.color;
	pIn.texCoord = vIn.texCoord;
	
    // Assumes nonuniform scaling
	pIn.normal = mul(vIn.normal, (float3x3)world);
	pIn.tangent = mul(vIn.tangent, (float3x3)world);
	
    return pIn;
}