#include "common.hlsl"

Texture2D wallTexture : register(t0);
Texture2D tileTexture : register(t3);
Texture2D normalTexture : register(t4);
Texture2D shadowTexture : register(t0, space1);
SamplerState g_sampler : register(s0);

cbuffer Material : register(b2)
{
	float4 diffuseAlbedo;
	float3 fresnelR0;
	float shininess;
}

struct PixelIn
{
	float4 pos : SV_POSITION;
	float4 color : COLOR;
	float2 texCoord : TEXCOORD;
	float3 normal : NORMAL;
	float3 posW : POSITION;
	float3 tangent : TANGENT;
};

float CalculateAttenuation(float distanceFromLight, Light light)
{
	return saturate((light.fallOffEnd - distanceFromLight) / (light.fallOffEnd - light.fallOffStart));
}

float CalculateRoughnessFactor(float3 halfVector, float3 normalVector, float shininess)
{
    shininess *= 256.0f;
	return pow(saturate(dot(halfVector, normalVector)), shininess) * (shininess + 8.0f) / 8.0f;
}

float3 SchlickFresnel(float3 R0, float3 normalVector, float3 halfVector)
{
	float cosTheta = max(dot(halfVector, normalVector), 0.0);
	return R0 + (1 - R0) * pow((1 - cosTheta), 5);	
}

float3 BlinnPhong(float3 normal, float3 nToEye, float3 nLightVector, float3 lambertLightStrength)
{
	float shininess = 32.0;
	float3 hv = normalize(nLightVector + nToEye);
	float roughnessFactor = CalculateRoughnessFactor(hv, normal, shininess);
	float3 fresnelFactor = SchlickFresnel(fresnelR0, nToEye, hv);
	float3 specAlbedo = fresnelFactor * roughnessFactor;
	specAlbedo = specAlbedo / (specAlbedo + 1.0f);
	return (diffuseAlbedo.rgb + specAlbedo) * lambertLightStrength;	
}

float3 ComputeDirectionalLight(Light light, float3 pos, float3 normalVector, float3 nToEye)
{
	float3 lightVector = -light.direction;
	float3 nLightVector = normalize(lightVector);
	float3 lambertLightStrength = light.strength * max(dot(nLightVector, normalVector), 0.0f);
	return BlinnPhong(normalVector, nToEye, nLightVector, lambertLightStrength);
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
	float4 diffuseColor = tileTexture.Sample(g_sampler, pIn.texCoord);
	float3 normalSample = normalTexture.Sample(g_sampler, pIn.texCoord).rgb;

	pIn.normal = normalize(pIn.normal);
	float3x3 TBN = GetTBN(pIn.normal, pIn.tangent);
	float3 normalWS = NormalTSToWS(normalSample, TBN);
	if (normalMapIndex > 0.5) normalWS = pIn.normal;
	float shadowFactor = 1.0;
	float4 posW = float4(pIn.posW, 1.0);
	float4 shadowCoord = mul(posW, shadowTransform);
	
	if (shadowCoord.x >= 0.0 && shadowCoord.x <= 1.0 && shadowCoord.y >= 0.0 && shadowCoord.y <= 1.0 && shadowCoord.z >= 0.0 && shadowCoord.z <= 1.0)
    {
		float currentDepth = shadowCoord.z;
		float shadowDepth = shadowTexture.Sample(g_sampler, shadowCoord.xy).r;
		
		shadowFactor = currentDepth > shadowDepth ? 0.3 : 1.0;
	}
	
	float3 toEye = eyePos - pIn.posW.xyz;
	float3 nToEye = normalize(toEye);
	float3 finalColor = ambientLight.rgb * diffuseColor.rgb;
	
	finalColor += ComputeDirectionalLight(lights[0], pIn.posW, normalWS, nToEye) * shadowFactor;
	finalColor += ComputePointLight(lights[1], pIn.posW, normalWS, toEye);
	finalColor += ComputeSpotLight(lights[2], pIn.posW, normalWS, toEye);

	return float4(finalColor, diffuseColor.a);
}
