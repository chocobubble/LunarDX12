Texture2D g_texture : register(t0);
SamplerState g_sampler : register(s0);
cbuffer BasicConstants : register(b0)
{
	float4x4 model;
	float4x4 view;
	float4x4 projection;
	float3 eyePos;
}
cbuffer Light : register(b1)
{
	float3 strength;
	float fallOffStart;
	float3 direction;
	float fallOffEnd;
	float3 position;
	float spotPower;
}

struct PixelIn
{
	float4 pos : SV_POSITION;
	float4 color : COLOR;
	float2 texCoord : TEXCOORD;
	float3 normal : NORMAL;
};

float3 BlinnPhong(float3 color, float3 normal, float3 pos)
{
	float shininess = 32.0;
	float3 ambientLight = float3(0.1, 0.1, 0.1);
	float3 lv = normalize(-direction);
	float3 lightStrength = strength * dot(normal, lv);
	float3 ev = normalize(eyePos - pos);
	float3 hv = normalize(lv + ev);
	float3 ambient = ambientLight * color;
	float3 diffuse = lightStrength * saturate(dot(lv, normal)) * color;
	float3 specular = lightStrength * pow(saturate(dot(hv, normal)), shininess) * color;
	return ambient + diffuse + specular;
}

float4 main(PixelIn pIn) : SV_TARGET
{
	// return g_texture.Sample(g_sampler, pIn.texCoord);
	return float4(BlinnPhong(g_texture.Sample(g_sampler, pIn.texCoord).rgb, pIn.normal, pIn.pos), 1.0);
}