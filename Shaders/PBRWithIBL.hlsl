#include "PBR.hlsl"

// IBL Textures
TextureCube environmentMap : register(t9);
TextureCube irradianceMap : register(t10);
TextureCube prefilterMap : register(t11);
Texture2D brdfLUT : register(t12);

// IBL Constants
cbuffer IBLConstants : register(b3)
{
    float iblIntensity;
    float maxReflectionLOD;
    float2 padding;
};

// Sample irradiance map for diffuse IBL
float3 SampleIrradiance(float3 normal)
{
    return irradianceMap.Sample(g_sampler, normal).rgb * iblIntensity;
}

// Sample prefiltered environment map for specular IBL
float3 SamplePrefilter(float3 reflectDir, float roughness)
{
    float lod = roughness * maxReflectionLOD;
    return prefilterMap.SampleLevel(g_sampler, reflectDir, lod).rgb * iblIntensity;
}

// Sample BRDF integration map
float2 SampleBRDF(float nDotV, float roughness)
{
    return brdfLUT.Sample(g_sampler, float2(nDotV, roughness)).rg;
}

// Calculate IBL contribution
float3 CalculateIBL(float3 normal, float3 viewDir, Material material)
{
    float nDotV = max(dot(normal, viewDir), 0.0);
    
    // Diffuse IBL
    float3 irradiance = SampleIrradiance(normal);
    float3 diffuse = irradiance * material.albedo;
    
    // Specular IBL
    float3 reflectDir = reflect(-viewDir, normal);
    float3 prefilteredColor = SamplePrefilter(reflectDir, material.roughness);
    float2 brdf = SampleBRDF(nDotV, material.roughness);
    float3 specular = prefilteredColor * (material.F0 * brdf.x + brdf.y);
    
    // Combine diffuse and specular
    float3 kS = SchlickFresnel(material.F0, nDotV);
    float3 kD = (1.0 - kS) * (1.0 - material.metallic);
    
    return kD * diffuse + specular;
}

// Enhanced Cook-Torrance with IBL
float3 CookTorranceWithIBL(float3 normal, float3 nLightDir, float3 nToEye, Material material)
{
    // Direct lighting (existing Cook-Torrance)
    float3 directLighting = CookTorrance(normal, nLightDir, nToEye, material);
    
    // Indirect lighting (IBL)
    float3 indirectLighting = CalculateIBL(normal, nToEye, material);
    
    // Apply ambient occlusion to indirect lighting
    indirectLighting *= material.ao;
    
    return directLighting + indirectLighting;
}

// Simplified IBL-only function for testing
float3 IBLOnly(float3 normal, float3 viewDir, Material material)
{
    return CalculateIBL(normal, viewDir, material) * material.ao;
}

// Environment map background sampling
float3 SampleEnvironment(float3 direction)
{
    return environmentMap.Sample(g_sampler, direction).rgb;
}

// Tone mapping for HDR to LDR conversion (Uncharted 2)
float3 Uncharted2Tonemap(float3 x)
{
    float A = 0.15; // Shoulder Strength
    float B = 0.50; // Linear Strength  
    float C = 0.10; // Linear Angle
    float D = 0.20; // Toe Strength
    float E = 0.02; // Toe Numerator
    float F = 0.30; // Toe Denominator
    
    return ((x * (A * x + C * B) + D * E) / (x * (A * x + B) + D * F)) - E / F;
}

float3 Uncharted2ToneMapping(float3 hdrColor)
{
    float exposureBias = 2.0;
    float3 curr = Uncharted2Tonemap(exposureBias * hdrColor);
    
    float W = 11.2; // Linear White Point Value
    float3 whiteScale = 1.0 / Uncharted2Tonemap(W);
    
    return curr * whiteScale;
}

// Debug visualization modes
float3 DebugVisualization(float3 normal, float3 viewDir, Material material, uint debugMode)
{
    switch (debugMode)
    {
        case 0: // Normal rendering
            return CookTorranceWithIBL(normal, float3(0, 1, 0), viewDir, material);
            
        case 1: // IBL only
            return IBLOnly(normal, viewDir, material);
            
        case 2: // Irradiance only
            return SampleIrradiance(normal);
            
        case 3: // Prefilter only
            float3 reflectDir = reflect(-viewDir, normal);
            return SamplePrefilter(reflectDir, material.roughness);
            
        case 4: // Environment map
            return SampleEnvironment(reflect(-viewDir, normal));
            
        case 5: // BRDF visualization
            float nDotV = max(dot(normal, viewDir), 0.0);
            float2 brdf = SampleBRDF(nDotV, material.roughness);
            return float3(brdf.x, brdf.y, 0.0);
            
        default:
            return float3(1, 0, 1); // Magenta for invalid mode
    }
}
