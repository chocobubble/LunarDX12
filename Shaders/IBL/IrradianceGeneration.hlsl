TextureCube environmentMap : register(t0);
RWTexture2DArray<float4> irradianceMap : register(u0);
SamplerState linearSampler : register(s0);

cbuffer IBLConstants : register(b0)
{
    float envIntensity;
    float maxReflectionLod;
    float exposure;
    float gamma;
};

static const float PI = 3.14159265359;

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

float3x3 GetTangentBasis(float3 N)
{
    float3 up = abs(N.z) < 0.999 ? float3(0, 0, 1) : float3(1, 0, 0);
    float3 tangent = normalize(cross(up, N));
    float3 bitangent = cross(N, tangent);
    return float3x3(tangent, bitangent, N);
}

float3 TangentToWorld(float3 tangentVec, float3 N)
{
    float3x3 TBN = GetTangentBasis(N);
    return mul(tangentVec, TBN);
}

[numthreads(32, 32, 1)]
void CSMain(uint3 id : SV_DispatchThreadID)
{
    uint width, height, depth;
    irradianceMap.GetDimensions(width, height, depth);
    
    if (id.x >= width || id.y >= height) return;
    
    float3 N = GetCubemapDirection(id, width);
    N = normalize(N);
    
    float3 irradiance = float3(0.0);
    float sampleCount = 0.0;
    
    float sampleDelta = 0.025;
    for (float phi = 0.0; phi < 2.0 * PI; phi += sampleDelta)
    {
        for (float theta = 0.0; theta < 0.5 * PI; theta += sampleDelta)
        {
            float3 tangentSample = float3(
                sin(theta) * cos(phi),
                sin(theta) * sin(phi),
                cos(theta)
            );
            
            float3 sampleVec = TangentToWorld(tangentSample, N);
            
            irradiance += environmentMap.SampleLevel(linearSampler, sampleVec, 0).rgb 
                         * cos(theta) * sin(theta);
            sampleCount++;
        }
    }
    
    irradiance = PI * irradiance / sampleCount;
    irradianceMap[id] = float4(irradiance, 1.0);
}
