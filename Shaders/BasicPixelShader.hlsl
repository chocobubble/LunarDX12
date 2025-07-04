Texture2D wallTexture : register(t0);
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
	float3 posW : POSITION0;
	float3 eyePosW : POSITION1;
};

float CalculateAttenuation(float distanceFromLight, Light light)
{
	return saturate((light.fallOffEnd - distanceFromLight) / (light.fallOffEnd - light.fallOffStart));
}

float CalculateRoughnessFactor(float3 halfVector, float normalVector, float shininess)
{
	shininess *= 256.0f;
	return pow(max(dot(halfVector, normalVector), 0.0f), shininess) * (shininess + 8.0f) / 8.0f;
}

float3 SchlickFresnel(float3 R0, float3 lightVector, float3 halfVector)
{
	float cosTheta = saturate(dot(halfVector, lightVector));
	return R0 + (1 - R0) * pow((1.0 - cosTheta), 5);	
}

float3 BlinnPhong(float3 normal, float3 toEye, float3 lightVector, float3 lambertLightStrength)
{
	float3 hv = normalize(lightVector + toEye);
	float roughnessFactor = CalculateRoughnessFactor(hv, normal, shininess);
	float3 fresnelFactor = SchlickFresnel(fresnelR0, lightVector, hv);
	float3 t = roughnessFactor * fresnelFactor;
	t = t / (t + 1.0f);
	return (diffuseAlbedo.rgb + t) * lambertLightStrength;
	// return (diffuseAlbedo.rgb + 1.0 * fresnelFactor) * lambertLightStrength;
	// return lambertLightStrength;
}

float3 ComputeDirectionalLight(float3 lightVector, float3 normalVector, float3 toEye, float3 lightStrength)
{
	float3 lambertLightStrength = lightStrength * max(dot(lightVector, normalVector), 0.0f);
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
	// float4 posW = float4(pIn.posW, 1.0);
	// float4 shadowCoord = mul(posW, shadowTransform);
 //    float depth = shadowCoord.z;
	// float shadowDepth = shadowTexture.Sample(g_sampler, shadowCoord.xy).r;
	// float shadow = depth > shadowDepth ? 0.5 : 0.0;
	// float4 wallColor = wallTexture.Sample(g_sampler, pIn.texCoord);
	// wallColor.xyz -= shadow;
	// return wallColor;

	pIn.normal = normalize(pIn.normal);
	// float3 temp = float3(-3.5, 0.5, -3.5);
	float3 toEye = normalize(eyePos.xyz - pIn.posW);
	// float3 toEye = normalize((-3.5, 0.5, -3.5, 0.0) - pIn.posW);
	// float3 toEye = normalize(temp - pIn.posW);
	float3 finalColor = float3(0, 0, 0);
	// finalColor += ComputeDirectionalLight(normalize(-(lights[0].direction)), pIn.normal, toEye, lights[0].strength);
	// finalColor += ComputePointLight(lights[1], pIn.posW, pIn.normal, toEye);
	finalColor += ComputeSpotLight(lights[2], pIn.posW, pIn.normal, toEye);
	return float4(finalColor, 1);

	
	// return float4(1, 1, 1, 1) * dot(-lights[0].direction, pIn.normal); -> 정상!! 그렇다면 문제는 toEye?
}
