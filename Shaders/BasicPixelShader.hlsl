Texture2D shadowTexture : register(t4);
SamplerState g_sampler : register(s0);

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
	float3 eyePos;
	float4 ambientLight;
	Light lights[3];
}

cbuffer ObjectConstants : register(b1)
{
	float4x4 world;
	int textureIndex;	
}

cbuffer Material : register(b2)
{
	float4 diffuseAlbedo;
	float3 fresnelR0;
	float roughness;
}

struct PixelIn
{
	float4 pos : SV_POSITION;
	float4 color : COLOR;
	float2 texCoord : TEXCOORD;
	float3 normal : NORMAL;
	float3 posW : POSITION;
};

float CalculateAttenuation(float distanceFromLight, Light light)
{
	return saturate((light.fallOffEnd - distanceFromLight) / (light.fallOffEnd - light.fallOffStart));
}

float CalculateRoughnessFactor(float3 halfVector, float normalVector, float Shininess)
{
	return pow(max(dot(halfVector, normalVector), 0.0f), Shininess) * (Shininess + 8.0f) / 8.0f;
}

float3 SchlickFresnel(float3 R0, float3 toEye, float3 halfVector)
{
	float cosTheta = max(dot(halfVector, toEye), 0.0);
	return R0 + (1 - R0) * pow((1 - cosTheta), 5);	
}

float3 BlinnPhong(float3 normal, float3 toEye, float lightVector, float lambertLightStrength)
{
	float shininess = 32.0;
	float3 hv = normalize(lightVector + toEye);
	float roughnessFactor = CalculateRoughnessFactor(hv, normal, shininess);
	float3 fresnelFactor = SchlickFresnel(fresnelR0, toEye, hv);
	return (diffuseAlbedo.rgb + roughnessFactor * fresnelFactor) * lambertLightStrength;	
}

float3 ComputeDirectionalLight(float3 lightVector, float3 normalVector, float3 toEye, float3 lightStrength)
{
	float lambertLightStrength = lightStrength * max(dot(lightVector, normalVector), 0.0f);
	return BlinnPhong(normalVector, toEye, lightVector, lambertLightStrength);
}

float3 ComputePointLight(Light light, float3 pos, float3 normalVector, float3 toEye)
{
	float3 lightVector = light.position - pos;
	float distanceFromLight = length(lightVector); 
	if (distanceFromLight > light.fallOffEnd)
		return 0.0f;
	float3 nLightVector = normalize(lightVector);
	float ndotl = max(dot(nLightVector, normalVector), 0.0f);
	float3 lambertLightStrength = light.strength * ndotl;
	float attenuation = CalculateAttenuation(distanceFromLight, light);
	lambertLightStrength *= attenuation;
	return BlinnPhong(normalVector, toEye, nLightVector, lambertLightStrength);
}

float3 ComputeSpotLight(Light light, float3 pos, float3 normalVector, float3 toEye)
{
	float3 lightVector = light.position - pos;
	float distanceFromLight = length(lightVector);
	if (distanceFromLight > light.fallOffEnd)
		return 0.0f;
	float3 nLightVector = normalize(lightVector);
	float ndotl = max(dot(nLightVector, normalVector), 0.0f);
	float3 lambertLightStrength = light.strength * ndotl;

	float attenuation = CalculateAttenuation(distanceFromLight, light);
	lambertLightStrength *= attenuation;

	float spotFactor = pow(max(dot(-nLightVector, light.direction), 0.0f), light.spotPower);
	lambertLightStrength *= spotFactor;

	return BlinnPhong(normalVector, toEye, nLightVector, lambertLightStrength);
}

float4 main(PixelIn pIn) : SV_TARGET
{
	float depth = shadowTexture.Sample(g_sampler, pIn.texCoord).r;

	if (depth == 1.0f) return float4(1.0, 0.0, 0.0, 1.0); 
	if (depth > 0.95f) return float4(1.0, 0.5, 0.0, 1.0);
	if (depth < 0.01f) return float4(0.0, 0.0, 1.0, 1.0);
	return float4(0, depth, 0, 1); 
	
	// FIXME : Lighting system looks bad.
	float3 tmp = (0.0, 0.0, 0.0);
	
	float3 toEye = normalize(eyePos - pIn.posW);
	tmp += ComputePointLight(lights[1], pIn.posW, pIn.normal, toEye);

	return pIn.color;
}
