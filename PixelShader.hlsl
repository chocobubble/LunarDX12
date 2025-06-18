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
	float3 lightStrength;
	float fallOffStart;
	float3 direction;
	float fallOffEnd;
	float3 position;
	float spotPower;
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

float CalculateAttenuation(float distanceFromLight)
{
	return saturate((fallOffEnd - distanceFromLight) / (fallOffEnd - fallOffStart));
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

float3 ComputePointLight(float3 lightVector, float distanceFromLight, float3 normalVector, float3 lightStrength, float3 toEye)
{
	if (distanceFromLight > fallOffEnd)
		return 0.0f;
	float ndotl = max(dot(lightVector, normalVector), 0.0f);
	float3 lambertLightStrength = lightStrength * ndotl;
	float attenuation = CalculateAttenuation(distanceFromLight);
	lambertLightStrength *= attenuation;
	return BlinnPhong(normalVector, toEye, lightVector, lambertLightStrength);
}

float3 ComputeSpotLight(float3 lightVector, float3 distanceFormLight, float3 normalVector, float3 lightStrength, float3 lightDirection, float3 toEye)
{
	float ndotl = max(dot(lightVector, normalVector), 0.0f);
	float3 lambertLightStrength = lightStrength * ndotl;

	float attenuation = CalculateAttenuation(distanceFormLight);
	lambertLightStrength *= attenuation;

	float spotFactor = pow(max(dot(-lightVector, lightDirection), 0.0f), spotPower);
	lambertLightStrength *= spotFactor;

	return BlinnPhong(normalVector, toEye, lightVector, lambertLightStrength);
}

float4 main(PixelIn pIn) : SV_TARGET
{
	// return g_texture.Sample(g_sampler, pIn.texCoord) * diffuseAlbedo;
	// return float4(g_texture.Sample(g_sampler, pIn.texCoord).rgb * BlinnPhong(pIn.normal, pIn.pos, fresnelR0, diffuseAlbedo), 1.0);
	// float3 tmp = g_texture.Sample(g_sampler, pIn.texCoord).rgb;

	// FIXME : Lighting system looks bad. 
	float3 tmp = (0.0, 0.0, 0.0); 
	float3 lightVector = normalize(-direction);
	float3 toEye = normalize(eyePos - pIn.posW);
	tmp += ComputeDirectionalLight(lightVector, pIn.normal, toEye, lightStrength);

	float3 pointLightPos = {-5, 5, 5};
	lightVector = pointLightPos - pIn.posW;
	tmp += ComputePointLight(normalize(lightVector), length(lightVector), pIn.normal, lightStrength, toEye);

	float3 spotLightPos = {-5, -5, 1};
	float3 spotLightDir = {1, 1, -1};
	float3 spotLightVector = normalize(spotLightPos - pIn.posW);
	tmp += ComputeSpotLight(spotLightVector, length(spotLightPos - pIn.posW), pIn.normal, lightStrength, spotLightDir, toEye);

	float4 ambientLight = {0.1, 0.1, 0.1, 0.0};
	float4 ambient = ambientLight * diffuseAlbedo;
	return float4(tmp, 1.0) + ambient;
}