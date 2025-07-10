Texture2D<float4> inputTexture : register(t0);
RWTexture2D<float4> outputTexture : register(u0);

cbuffer PostProcessConstants : register(b0)
{
    float exposure;
    float gamma;
    
    float bloomThreshold;
    float bloomIntensity;
    float bloomRadius;
    
    uint colorLevels;
    float edgeThreshold;
    
    float dotSize;
    float dotSpacing;
    
    float bleedRadius;
    float paperIntensity;
    uint sampleCount;
    
    uint textureWidth;
    uint textureHeight;
    float texelSizeX;
    float texelSizeY;
};

float3 ACESFilm(float3 color)
{
    float a = 2.51f;
    float b = 0.03f;
    float c = 2.43f;
    float d = 0.59f;
    float e = 0.14f;
    return saturate((color * (a * color + b)) / (color * (c * color + d) + e));
}

float3 Reinhard(float3 color)
{
    return color / (1.0f + color);
}

float3 Uncharted2Tonemap(float3 color)
{
    float A = 0.15f;
    float B = 0.50f;
    float C = 0.10f;
    float D = 0.20f;
    float E = 0.02f;
    float F = 0.30f;
    
    return ((color * (A * color + C * B) + D * E) / (color * (A * color + B) + D * F)) - E / F;
}

[numthreads(8, 8, 1)]
void CSMain(uint3 id : SV_DispatchThreadID)
{
    if (id.x >= textureWidth || id.y >= textureHeight)
        return;
    
    float3 hdrColor = inputTexture[id.xy].rgb;
    
    hdrColor *= exposure;
    
    float3 ldrColor = ACESFilm(hdrColor);
    
    ldrColor = pow(abs(ldrColor), 1.0f / gamma);
    
    outputTexture[id.xy] = float4(ldrColor, 1.0f);
}
