// BloomBrightPass.hlsl - Extract bright areas for bloom effect

Texture2D<float4> sceneTexture : register(t0);
RWTexture2D<float4> brightTexture : register(u0);

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

// Luminance calculation
float GetLuminance(float3 color)
{
    return dot(color, float3(0.299f, 0.587f, 0.114f));
}

// Smooth threshold function
float SmoothThreshold(float luminance, float threshold, float softness)
{
    float knee = threshold * softness;
    float soft = luminance - threshold + knee;
    soft = clamp(soft, 0.0f, 2.0f * knee);
    soft = soft * soft / (4.0f * knee + 1e-4f);
    
    float contribution = max(soft, luminance - threshold);
    contribution /= max(luminance, 1e-4f);
    
    return contribution;
}

[numthreads(8, 8, 1)]
void CSMain(uint3 id : SV_DispatchThreadID)
{
    // Bounds check
    if (id.x >= textureWidth || id.y >= textureHeight)
        return;
    
    // Sample scene color
    float3 color = sceneTexture[id.xy].rgb;
    
    // Calculate luminance
    float luminance = GetLuminance(color);
    
    // Apply threshold with smooth falloff
    float contribution = SmoothThreshold(luminance, bloomThreshold, 0.5f);
    
    // Extract bright areas
    float3 brightColor = color * contribution;
    
    // Write result
    brightTexture[id.xy] = float4(brightColor, 1.0f);
}
