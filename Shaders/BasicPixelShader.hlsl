#include "Common.hlsl"
#include "PBR.hlsl"

Texture2D wallTexture : register(t0);
Texture2D normalTexture : register(t4);
Texture2D shadowTexture : register(t0, space1);

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

float CalculateRoughnessFactor(float3 halfVector, float3 normalVector, float roughness)
{
    roughness *= 256.0f;
	return pow(saturate(dot(halfVector, normalVector)), roughness) * (roughness + 8.0f) / 8.0f;
}

float3 SchlickFresnelBP(float3 R0, float nDotH)
{
	return R0 + (1 - R0) * pow((1 - nDotH), 5);
}

float3 BlinnPhong(float3 normal, float3 nToEye, float3 nLightVector, Material material)
{
	float3 hv = normalize(nLightVector + nToEye);
	float roughnessFactor = CalculateRoughnessFactor(hv, normal, material.roughness);
    float nDotH = max(dot(normal, hv), 0.0f);
	float3 fresnelFactor = SchlickFresnelBP(F0, nDotH);
	float3 specAlbedo = fresnelFactor * roughnessFactor;
	specAlbedo = specAlbedo / (specAlbedo + 1.0f);
	return (material.albedo.rgb + specAlbedo);
}

float3 ComputeLight(Light light, float3 pos, float3 normalVector, float3 toEye, Material material)
{
    float3 lightVector = -light.direction;
    float attenuation = 1.0f;
    if (light.fallOffEnd > 0.0f) // point or spot light
    {
        lightVector = light.position - pos; 
        float distanceFromLight = length(lightVector);
        if (distanceFromLight > light.fallOffEnd)
            return float3(0.0f, 0.0f, 0.0f);
        attenuation = CalculateAttenuation(distanceFromLight, light);
    }
    float3 nLightVector = normalize(lightVector);
    float ndotl = max(dot(nLightVector, normalVector), 0.0f);
    float3 lambertLightStrength = light.strength * ndotl * attenuation;

    if (light.spotPower > 0.0f) // spot light
    {
        float spotFactor = pow(max(dot(-nLightVector, light.direction), 0.0f), light.spotPower);
        lambertLightStrength *= spotFactor;
    }

    static const uint PBR_ENABLED = 0x08;
    if (debugFlags & PBR_ENABLED)
    {
        return CookTorrance(normalVector, nLightVector, toEye, material) * lambertLightStrength;
    }
    else
    {
        return BlinnPhong(normalVector, toEye, nLightVector, material) * lambertLightStrength;
    }
}

float4 main(PixelIn pIn) : SV_TARGET
{
    Material material = GetMaterial(pIn.texCoord);

	float3 diffuseColor = material.albedo;

	pIn.normal = normalize(pIn.normal);
    float3 normalWS = pIn.normal;
	uint normalMapMask = 0x02;
	if (normalMapMask & debugFlags) 
    {
        float3 normalSample = normalTexture.Sample(g_sampler, pIn.texCoord).rgb;
        float3x3 TBN = GetTBN(pIn.normal, pIn.tangent);
        normalWS = NormalTSToWS(normalSample, TBN);
    }

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
	
	finalColor += ComputeLight(lights[0], pIn.posW, normalWS, nToEye, material) * shadowFactor;
	finalColor += ComputeLight(lights[1], pIn.posW, normalWS, nToEye, material);
	finalColor += ComputeLight(lights[2], pIn.posW, normalWS, nToEye, material);

    finalColor *= material.ao;

	return float4(finalColor, 1.0);
}
