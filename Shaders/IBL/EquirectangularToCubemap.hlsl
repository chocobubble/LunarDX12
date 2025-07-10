Texture2D<float4> equirectangularMap : register(t0);
RWTexture2DArray<float4> outputCubemap : register(u0);
SamplerState linearSampler : register(s0);

cbuffer IBLConstants : register(b0)
{
    float envIntensity;
    float maxReflectionLod;
    float exposure;
    float gamma;
};

static const float PI = 3.14159265359;

float2 SampleSphericalMap(float3 direction)
{
    float2 uv = float2(atan2(direction.z, direction.x), asin(direction.y));
    uv *= float2(0.1591, 0.3183);
    uv += 0.5;
    return uv;
}

float3 GetCubemapDirection(uint3 id, uint faceSize)
{
    float2 texCoord = float2(id.xy) / float(faceSize - 1);
    texCoord = texCoord * 2.0 - 1.0;
    
    switch (id.z)
    {
        case 0: return float3(1.0, -texCoord.y, -texCoord.x);
        case 1: return float3(-1.0, -texCoord.y, texCoord.x);
        case 2: return float3(texCoord.x, 1.0, texCoord.y);
        case 3: return float3(texCoord.x, -1.0, -texCoord.y);
        case 4: return float3(texCoord.x, -texCoord.y, 1.0);
        case 5: return float3(-texCoord.x, -texCoord.y, -1.0);
    }
    return float3(0, 0, 0);
}

[numthreads(32, 32, 1)]
void CSMain(uint3 id : SV_DispatchThreadID)
{
    uint width, height, depth;
    outputCubemap.GetDimensions(width, height, depth);
    
    if (id.x >= width || id.y >= height) return;
    
    float3 direction = GetCubemapDirection(id, width);
    direction = normalize(direction);
    
    float2 uv = SampleSphericalMap(direction);
    float3 color = equirectangularMap.SampleLevel(linearSampler, uv, 0).rgb;
    
    color *= envIntensity;
    
    outputCubemap[id] = float4(color, 1.0);
}
