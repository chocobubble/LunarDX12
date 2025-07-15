TextureCube<float4> environmentMap : register(t0, space0);
RWTexture2DArray<float4> outputMap : register(u0, space0);
SamplerState linearSampler : register(s0);

cbuffer RootConstants : register(b0)
{
    float roughness;
    uint mipLevel;
    uint2 padding;
};

static const float PI = 3.14159265359;
static const uint SAMPLE_COUNT = 1024;

float3 GetCubemapDirection(uint face, float2 uv)
{
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

void GetTangentSpace(float3 normal, out float3 tangent, out float3 bitangent)
{
    float3 up = abs(normal.y) < 0.999 ? float3(0, 1, 0) : float3(1, 0, 0);
    tangent = normalize(cross(up, normal));
    bitangent = cross(normal, tangent);
}

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

float2 GetRandomSample(uint index, uint2 pixelCoord)
{
    uint seed = index + pixelCoord.x * 1973 + pixelCoord.y * 9277;
    
    seed = seed * 1664525u + 1013904223u;
    float r1 = float(seed & 0x00FFFFFFu) / float(0x01000000u);
    
    seed = seed * 1664525u + 1013904223u;
    float r2 = float(seed & 0x00FFFFFFu) / float(0x01000000u);
    
    return float2(r1, r2);
}

float DistributionGGX(float3 N, float3 H, float roughness)
{
    /*
     * GGX Normal Distribution Function:
     * 
     * D(H) = α² / (π * ((N·H)² * (α² - 1) + 1)²)
     * 
     * Where:
     * - α = roughness² (remapped roughness parameter)
     * - N = surface normal vector
     * - H = halfway vector between view and light
     * - N·H = cosine of angle between normal and halfway vector
     * 
     * This function describes the distribution of microfacet normals
     * for a given roughness value. Higher roughness = wider distribution.
     */
    
    float a = roughness * roughness;
    float a2 = a * a;
    float NdotH = max(dot(N, H), 0.0);
    float NdotH2 = NdotH * NdotH;
    
    float num = a2;
    float denom = (NdotH2 * (a2 - 1.0) + 1.0);
    denom = PI * denom * denom;
    
    return num / denom;
}

float3 ImportanceSampleGGX(float2 xi, float3 normal, float roughness)
{
    /*
     * GGX Importance Sampling using Inverse Transform Method:
     *
     * Spherical coordinates:
     * φ = 2π * ξ₁                                    (azimuth angle, uniform)
     * cos θ = √((1 - ξ₂) / (1 + (α² - 1) * ξ₂))    (polar angle, GGX distribution)
     * sin θ = √(1 - cos² θ)
     * 
     * Cartesian coordinates (tangent space):
     * H.x = sin θ * cos φ
     * H.y = cos θ           (Y-axis = normal direction in DirectX)
     * H.z = sin θ * sin φ
     * 
     * Transform to world space:
     * H_world = H.x * tangent + H.y * normal + H.z * bitangent
     * 
     * Where:
     * - ξ₁, ξ₂ = uniform random variables [0,1]
     * - α = roughness²
     * - θ = polar angle from normal (0° = along normal)
     * - φ = azimuth angle around normal
     * 
     * This generates halfway vectors (H) distributed according to GGX,
     * which are then used to calculate light directions for sampling.
     */

    float a = roughness * roughness;
    
    float phi = 2.0 * PI * xi.x;
    float cosTheta = sqrt((1.0 - xi.y) / (1.0 + (a * a - 1.0) * xi.y));
    float sinTheta = sqrt(1.0 - cosTheta * cosTheta);
    
    float3 halfVector = float3(0, 0, 0);
    halfVector.x = sinTheta * cos(phi);  
    halfVector.y = cosTheta;             
    halfVector.z = sinTheta * sin(phi);  
    
    float3 tangent, bitangent;
    GetTangentSpace(normal, tangent, bitangent);
    
    float3 sampleVec = halfVector.x * tangent + halfVector.y * normal + halfVector.z * bitangent;
    return normalize(sampleVec);
}

[numthreads(8, 8, 1)]
void debug(uint3 id : SV_DispatchThreadID)
{
    uint width, height, elements;
    outputMap.GetDimensions(width, height, elements);
    
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
        outputMap[outputCoord] = debugColors[faceIndex];
    }
}

[numthreads(8, 8, 1)]
void irradiance(uint3 id : SV_DispatchThreadID)
{
    uint width, height, elements;
    outputMap.GetDimensions(width, height, elements);
    
    if (id.x >= width || id.y >= height || id.z >= 6)
        return;
    
    float2 uv = (float2(id.xy) + 0.5) / float2(width, height);
    
    float3 normal = GetCubemapDirection(id.z, uv);
    
    float3 tangent, bitangent;
    GetTangentSpace(normal, tangent, bitangent);
    
    float3 irradiance = float3(0, 0, 0);
    
    for (uint i = 0; i < SAMPLE_COUNT; ++i)
    {
        float2 xi = GetRandomSample(i, id.xy);
        
        float3 localSample = CosineSampleHemisphere(xi);
        
        float3 worldSample = localSample.x * tangent + 
                            localSample.y * normal + 
                            localSample.z * bitangent;
        
        float3 color = environmentMap.SampleLevel(linearSampler, worldSample, 0).rgb;
        
        irradiance += color;
    }
    
    irradiance = irradiance * PI / float(SAMPLE_COUNT);
    
    outputMap[id] = float4(irradiance, 1.0);
}

[numthreads(8, 8, 1)]
void main(uint3 id : SV_DispatchThreadID)
{
}

[numthreads(8, 8, 1)]
void prefiltered(uint3 id : SV_DispatchThreadID)
{
    uint width, height, elements;
    outputMap.GetDimensions(width, height, elements);
    
    if (id.x >= width || id.y >= height || id.z >= 6)
        return;
    
    float2 uv = (float2(id.xy) + 0.5) / float2(width, height);
    
    float3 normal = GetCubemapDirection(id.z, uv);
    
    float3 reflection = normal;
    float3 toEye = reflection;
    
    const uint SAMPLE_COUNT_PREFILTERED = 1024;
    float totalWeight = 0.0;
    float3 prefilteredColor = float3(0.0, 0.0, 0.0);
    
    for (uint i = 0; i < SAMPLE_COUNT_PREFILTERED; ++i)
    {
        float2 xi = GetRandomSample(i, id.xy);
        
        float3 halfVector = ImportanceSampleGGX(xi, normal, roughness);
        
        float3 lightVector = normalize(2.0 * dot(toEye, halfVector) * halfVector - toEye);
        
        float NdotL = dot(normal, lightVector);
        if (NdotL > 0.0)
        {
            float3 color = environmentMap.SampleLevel(linearSampler, lightVector, 0).rgb;
            prefilteredColor += color * NdotL;
            totalWeight += NdotL;
        }
    }
    
    prefilteredColor = prefilteredColor / totalWeight;
    
    outputMap[id] = float4(prefilteredColor, 1.0);
}
    
float GeometrySchlickGGX(float NdotV, float roughness)
{
    /*
     * Schlick-GGX approximation for geometry function:
     * 
     * G₁(v) = (N·V) / ((N·V) * (1 - k) + k)
     * 
     */
    float a = roughness + 1.0;
    float k = (a * a) / 8.0;

	return NdotV / (NdotV * (1.0 - k) + k);
}

float GeometrySmith(float3 normal, float3 toEye, float3 lightVector, float roughness)
{
    /*
     * Smith's method combines masking and shadowing:
     * G(L,V,H) = G₁(L) * G₁(V)
     */
    float NdotV = max(dot(normal, toEye), 0.0);
    float NdotL = max(dot(normal, lightVector), 0.0);
    float g2 = GeometrySchlickGGX(NdotV, roughness);
    float g1 = GeometrySchlickGGX(NdotL, roughness);

    return g1 * g2;
}

float2 IntegrateBRDF(float NdotV, float roughness)
{
    /*
     * BRDF Integration for Split-Sum Approximation:
     * 
     * ∫ BRDF(l,v) * n·l dl ≈ F₀ * scale + bias
     * 
     * This function computes (scale, bias) for given NdotV and roughness
     * Red channel: scale factor for F₀
     * Green channel: bias term
     */
    float3 V;
    V.x = sqrt(1.0 - NdotV * NdotV);
    V.y = 0.0;
    V.z = NdotV;

    float A = 0.0;
    float B = 0.0; 

    float3 N = float3(0.0, 0.0, 1.0);
    
    const uint SAMPLE_COUNT_BRDF = 1024;
    for(uint i = 0; i < SAMPLE_COUNT_BRDF; ++i)
    {
        float2 Xi = GetRandomSample(i, uint2(NdotV * 1000, roughness * 1000));
        float3 H = ImportanceSampleGGX(Xi, N, roughness);
        float3 L = normalize(2.0 * dot(V, H) * H - V);

        float NdotL = max(L.z, 0.0);
        float NdotH = max(H.z, 0.0);
        float VdotH = max(dot(V, H), 0.0);

        if(NdotL > 0.0)
        {
            float G = GeometrySmith(N, V, L, roughness);
            float G_Vis = (G * VdotH) / (NdotH * NdotV);
            float Fc = pow(1.0 - VdotH, 5.0);

            A += (1.0 - Fc) * G_Vis;
            B += Fc * G_Vis;
        }
    }
    A /= float(SAMPLE_COUNT_BRDF);
    B /= float(SAMPLE_COUNT_BRDF);
    return float2(A, B);
}

[numthreads(8, 8, 1)]
void brdfLut(uint3 id : SV_DispatchThreadID)
{
    uint width, height, elements;
    outputMap.GetDimensions(width, height, elements);
    
    if (id.x >= width || id.y >= height)
        return;
    
    float2 uv = (float2(id.xy) + 0.5) / float2(width, height);
    
    float NdotV = uv.x;
    float roughness = uv.y;
    
    float2 integratedBRDF = IntegrateBRDF(NdotV, roughness);
    
    outputMap[uint3(id.x, id.y, 0)] = float4(integratedBRDF.x, integratedBRDF.y, 0.0, 1.0);
}

