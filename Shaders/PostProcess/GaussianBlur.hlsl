// GaussianBlur.hlsl - Separable Gaussian blur for bloom effect

Texture2D<float4> inputTexture : register(t0);
RWTexture2D<float4> outputTexture : register(u0);

cbuffer PostProcessConstants : register(b0)
{
    // Tone Mapping
    float exposure;
    float gamma;
    
    // Bloom
    float bloomThreshold;
    float bloomIntensity;
    float bloomRadius;
    
    // Toon Shading
    uint colorLevels;
    float edgeThreshold;
    
    // Halftone
    float dotSize;
    float dotSpacing;
    
    // Watercolor
    float bleedRadius;
    float paperIntensity;
    uint sampleCount;
    
    // Common
    uint textureWidth;
    uint textureHeight;
    float texelSizeX;
    float texelSizeY;
};

// Blur direction (set via root constants)
cbuffer BlurDirection : register(b1)
{
    float2 blurDirection; // (1,0) for horizontal, (0,1) for vertical
    float2 padding;
};

// Gaussian weights for 9-tap blur
static const float weights[9] = {
    0.0947416f, 0.118318f, 0.147761f, 0.118318f, 0.0947416f,
    0.118318f,  0.147761f, 0.118318f, 0.0947416f
};

// Alternative: 13-tap blur for higher quality
static const float weights13[13] = {
    0.002216f, 0.008764f, 0.026995f, 0.064759f, 0.120985f, 
    0.176033f, 0.199471f, 0.176033f, 0.120985f, 0.064759f, 
    0.026995f, 0.008764f, 0.002216f
};

[numthreads(8, 8, 1)]
void CSMain(uint3 id : SV_DispatchThreadID)
{
    // Bounds check
    if (id.x >= textureWidth || id.y >= textureHeight)
        return;
    
    float4 color = 0;
    float2 texelSize = float2(texelSizeX, texelSizeY);
    
    // 9-tap Gaussian blur
    [unroll]
    for (int i = -4; i <= 4; ++i)
    {
        float2 offset = blurDirection * i * bloomRadius * texelSize;
        int2 samplePos = int2(id.xy) + int2(offset * float2(textureWidth, textureHeight));
        
        // Clamp to texture bounds
        samplePos = clamp(samplePos, int2(0, 0), int2(textureWidth - 1, textureHeight - 1));
        
        color += inputTexture[samplePos] * weights[i + 4];
    }
    
    // Write result
    outputTexture[id.xy] = color;
}

// Alternative high-quality version
[numthreads(8, 8, 1)]
void CSMainHQ(uint3 id : SV_DispatchThreadID)
{
    // Bounds check
    if (id.x >= textureWidth || id.y >= textureHeight)
        return;
    
    float4 color = 0;
    float2 texelSize = float2(texelSizeX, texelSizeY);
    
    // 13-tap Gaussian blur
    [unroll]
    for (int i = -6; i <= 6; ++i)
    {
        float2 offset = blurDirection * i * bloomRadius * texelSize;
        int2 samplePos = int2(id.xy) + int2(offset * float2(textureWidth, textureHeight));
        
        // Clamp to texture bounds
        samplePos = clamp(samplePos, int2(0, 0), int2(textureWidth - 1, textureHeight - 1));
        
        color += inputTexture[samplePos] * weights13[i + 6];
    }
    
    // Write result
    outputTexture[id.xy] = color;
}
