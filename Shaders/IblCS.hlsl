// IBL Compute Shader for Irradiance Map Generation
// Converts environment cubemap to irradiance map using Monte Carlo integration

TextureCube<float4> environmentMap : register(t0);
RWTexture2DArray<float4> irradianceMap : register(u0);
SamplerState linearSampler : register(s0);

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
