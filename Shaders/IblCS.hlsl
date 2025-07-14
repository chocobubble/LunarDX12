TextureCube<float4> environmentMap : register(t0, space0);
RWTexture2DArray<float4> irradianceMap : register(u0, space0);
RWTexture2DArray<float4> prefilteredMap : register(u1, space0);
SamplerState linearSampler : register(s0);

cbuffer RootConstants : register(b0)
{
    float roughness;
    uint mipLevel;
    uint2 padding;
};

static const float PI = 3.14159265359;
static const uint SAMPLE_COUNT = 1024;

// Convert cubemap face and UV coordinates to world direction vector
float3 GetCubemapDirection(uint face, float2 uv)
{
    // Convert UV from [0,1] to [-1,1]
    float2 coords = uv * 2.0 - 1.0;
    
    switch (face)
    {
        case 0: return normalize(float3(1.0, -coords.y, -coords.x));  // +X
        case 1: return normalize(float3(-1.0, -coords.y, coords.x));  // -X
        case 2: return normalize(float3(coords.x, 1.0, coords.y));    // +Y
        case 3: return normalize(float3(coords.x, -1.0, -coords.y));  // -Y
        case 4: return normalize(float3(coords.x, -coords.y, 1.0));   // +Z
        case 5: return normalize(float3(-coords.x, -coords.y, -1.0)); // -Z
        default: return float3(0, 0, 1);
    }
}

// Generate tangent space basis vectors from normal
void GetTangentSpace(float3 normal, out float3 tangent, out float3 bitangent)
{
    // Choose up vector that's not parallel to normal
    float3 up = abs(normal.y) < 0.999 ? float3(0, 1, 0) : float3(1, 0, 0);
    tangent = normalize(cross(up, normal));
    bitangent = cross(normal, tangent);
}

// Cosine-weighted hemisphere sampling
float3 CosineSampleHemisphere(float2 xi)
{
    float cosTheta = sqrt(xi.x);
    float sinTheta = sqrt(1.0 - xi.x);
    float phi = 2.0 * PI * xi.y;
    
    return float3(
        sinTheta * cos(phi),
        cosTheta,
        sinTheta * sin(phi)
    );
}

// Simple pseudo-random number generator
float2 GetRandomSample(uint index, uint2 pixelCoord)
{
    // Simple hash-based random
    uint seed = index + pixelCoord.x * 1973 + pixelCoord.y * 9277;
    
    // Linear congruential generator
    seed = seed * 1664525u + 1013904223u;
    float r1 = float(seed & 0x00FFFFFFu) / float(0x01000000u);
    
    seed = seed * 1664525u + 1013904223u;
    float r2 = float(seed & 0x00FFFFFFu) / float(0x01000000u);
    
    return float2(r1, r2);
}

// GGX/Trowbridge-Reitz Normal Distribution Function
float DistributionGGX(float3 N, float3 H, float roughness)
{
    float a = roughness * roughness;
    float a2 = a * a;
    float NdotH = max(dot(N, H), 0.0);
    float NdotH2 = NdotH * NdotH;
    
    float num = a2;
    float denom = (NdotH2 * (a2 - 1.0) + 1.0);
    denom = PI * denom * denom;
    
    return num / denom;
}

// Importance sample GGX distribution
float3 ImportanceSampleGGX(float2 xi, float3 N, float roughness)
{
    float a = roughness * roughness;
    
    float phi = 2.0 * PI * xi.x;
    float cosTheta = sqrt((1.0 - xi.y) / (1.0 + (a * a - 1.0) * xi.y));
    float sinTheta = sqrt(1.0 - cosTheta * cosTheta);
    
    float3 H;
    H.x = cos(phi) * sinTheta;
    H.y = sin(phi) * sinTheta;
    H.z = cosTheta;
    
    float3 up = abs(N.z) < 0.999 ? float3(0.0, 0.0, 1.0) : float3(1.0, 0.0, 0.0);
    float3 tangent = normalize(cross(up, N));
    float3 bitangent = cross(N, tangent);
    
    float3 sampleVec = tangent * H.x + bitangent * H.y + N * H.z;
    return normalize(sampleVec);
}

[numthreads(8, 8, 1)]
void debug(uint3 id : SV_DispatchThreadID)
{
    uint width, height, elements;
    irradianceMap.GetDimensions(width, height, elements);
    
    if (id.x >= width || id.y >= height)
        return;
    
    float4 debugColors[6] = {
        float4(1, 0, 0, 1), // +X = Red
        float4(0, 1, 0, 1), // -X = Green  
        float4(0, 0, 1, 1), // +Y = Blue
        float4(1, 1, 0, 1), // -Y = Yellow
        float4(1, 0, 1, 1), // +Z = Magenta
        float4(0, 1, 1, 1)  // -Z = Cyan
    };
    
    for (uint faceIndex = 0; faceIndex < 6; ++faceIndex)
    {
        uint3 outputCoord = uint3(id.x, id.y, faceIndex);
        irradianceMap[outputCoord] = debugColors[faceIndex];
    }
}

[numthreads(8, 8, 1)]
void irradiance(uint3 id : SV_DispatchThreadID)
{
    uint width, height, elements;
    irradianceMap.GetDimensions(width, height, elements);
    
    // Bounds check
    if (id.x >= width || id.y >= height || id.z >= 6)
        return;
    
    // Convert pixel coordinates to UV [0,1]
    float2 uv = (float2(id.xy) + 0.5) / float2(width, height);
    
    // Get world direction for this pixel on the cubemap face
    float3 normal = GetCubemapDirection(id.z, uv);
    
    // Generate tangent space basis
    float3 tangent, bitangent;
    GetTangentSpace(normal, tangent, bitangent);
    
    // Monte Carlo integration for irradiance
    float3 irradiance = float3(0, 0, 0);
    
    for (uint i = 0; i < SAMPLE_COUNT; ++i)
    {
        // Generate random sample
        float2 xi = GetRandomSample(i, id.xy);
        
        // Cosine-weighted hemisphere sample in local space
        float3 localSample = CosineSampleHemisphere(xi);
        
        // Transform sample to world space
        float3 worldSample = localSample.x * tangent + 
                            localSample.y * normal + 
                            localSample.z * bitangent;
        
        // Sample environment map
        float3 color = environmentMap.SampleLevel(linearSampler, worldSample, 0).rgb;
        
        // Accumulate irradiance
        irradiance += color;
    }
    
    // Average the samples and apply Lambert BRDF factor (π)
    irradiance = irradiance * PI / float(SAMPLE_COUNT);
    
    // Write result to irradiance map
    irradianceMap[id] = float4(irradiance, 1.0);
}

// Alternative entry point for debugging - fills each face with different colors
[numthreads(8, 8, 1)]
void main(uint3 id : SV_DispatchThreadID)
{
}

[numthreads(8, 8, 1)]
void prefiltered(uint3 id : SV_DispatchThreadID)
{
    uint width, height, elements;
    prefilteredMap.GetDimensions(width, height, elements);
    
    if (id.x >= width || id.y >= height || id.z >= 6)
        return;
    
    float2 uv = (float2(id.xy) + 0.5) / float2(width, height);
    
    float3 N = GetCubemapDirection(id.z, uv);
    
    float3 R = N;
    float3 V = R;
    
    const uint SAMPLE_COUNT_PREFILTERED = 1024;
    float totalWeight = 0.0;
    float3 prefilteredColor = float3(0.0, 0.0, 0.0);
    
    for (uint i = 0; i < SAMPLE_COUNT_PREFILTERED; ++i)
    {
        float2 xi = GetRandomSample(i, id.xy);
        
        float3 H = ImportanceSampleGGX(xi, N, roughness);
        
        float3 L = normalize(2.0 * dot(V, H) * H - V);
        
        float NdotL = max(dot(N, L), 0.0);
        if (NdotL > 0.0)
        {
            float D = DistributionGGX(N, H, roughness);
            float NdotH = max(dot(N, H), 0.0);
            float HdotV = max(dot(H, V), 0.0);
            float pdf = D * NdotH / (4.0 * HdotV) + 0.0001;
            
            float resolution = 512.0; 
            float saTexel = 4.0 * PI / (6.0 * resolution * resolution);
            float saSample = 1.0 / (float(SAMPLE_COUNT_PREFILTERED) * pdf + 0.0001);
            
            float mipLevel = roughness == 0.0 ? 0.0 : 0.5 * log2(saSample / saTexel);
            
            float3 color = environmentMap.SampleLevel(linearSampler, L, mipLevel).rgb;
            prefilteredColor += color * NdotL;
            totalWeight += NdotL;
        }
    }
    
    prefilteredColor = prefilteredColor / totalWeight;
    
    prefilteredMap[id] = float4(prefilteredColor, 1.0);
}
