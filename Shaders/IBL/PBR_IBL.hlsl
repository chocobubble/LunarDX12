#include "../Common.hlsl"
#include "../PBR.hlsl"

TextureCube irradianceMap : register(t9);
TextureCube prefilteredMap : register(t10);
Texture2D brdfLUT : register(t11);
SamplerState envSampler : register(s1);

cbuffer IBLConstants : register(b3)
{
    float envIntensity;
    float maxReflectionLod;
    float exposure;
    float gamma;
};

float3 FresnelSchlickRoughness(float cosTheta, float3 F0, float roughness)
{
    return F0 + (max(float3(1.0 - roughness), F0) - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

float3 CalculateIBL(float3 albedo, float3 normal, float3 viewDir, 
                   float metallic, float roughness, float ao)
{
    float3 F0 = lerp(float3(0.04), albedo, metallic);
    float3 F = FresnelSchlickRoughness(max(dot(normal, viewDir), 0.0), F0, roughness);
    
    float3 kS = F;
    float3 kD = 1.0 - kS;
    kD *= 1.0 - metallic;
    
    float3 irradiance = irradianceMap.Sample(envSampler, normal).rgb;
    float3 diffuse = irradiance * albedo;
    
    float3 R = reflect(-viewDir, normal);
    float3 prefilteredColor = prefilteredMap.SampleLevel(envSampler, R, roughness * maxReflectionLod).rgb;
    
    float2 brdf = brdfLUT.Sample(envSampler, float2(max(dot(normal, viewDir), 0.0), roughness)).rg;
    float3 specular = prefilteredColor * (F * brdf.x + brdf.y);
    
    float3 ambient = (kD * diffuse + specular) * ao * envIntensity;
    return ambient;
}

float4 PBRPixelShaderWithIBL(VertexOut input) : SV_Target
{
    Material mat = GetMaterial(input.texCoord);
    
    float3 N = GetNormal(input);
    float3 V = normalize(eyePos - input.posW);
    
    float3 color = float3(0.0);
    
    for (int i = 0; i < lightCount; ++i)
    {
        color += CalculateDirectLighting(lights[i], mat, N, V, input.posW);
    }
    
    color += CalculateIBL(mat.albedo, N, V, mat.metallic, mat.roughness, mat.ao);
    
    return float4(color, 1.0);
}
