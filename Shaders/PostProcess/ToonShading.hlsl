// ToonShading.hlsl - Cel shading effect with color quantization

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

// Color quantization function
float3 QuantizeColor(float3 color, uint levels)
{
    float levelScale = (float)levels;
    return floor(color * levelScale) / levelScale;
}

// Alternative: Smooth quantization with dithering
float3 QuantizeColorSmooth(float3 color, uint levels, float2 screenPos)
{
    float levelScale = (float)levels;
    
    // Add dithering to reduce banding
    float dither = frac(sin(dot(screenPos, float2(12.9898f, 78.233f))) * 43758.5453f);
    dither = (dither - 0.5f) / levelScale;
    
    color += dither;
    return floor(color * levelScale) / levelScale;
}

// HSV color space conversion for better toon effect
float3 RGBtoHSV(float3 rgb)
{
    float4 K = float4(0.0f, -1.0f / 3.0f, 2.0f / 3.0f, -1.0f);
    float4 p = lerp(float4(rgb.bg, K.wz), float4(rgb.gb, K.xy), step(rgb.b, rgb.g));
    float4 q = lerp(float4(p.xyw, rgb.r), float4(rgb.r, p.yzx), step(p.x, rgb.r));
    
    float d = q.x - min(q.w, q.y);
    float e = 1.0e-10f;
    return float3(abs(q.z + (q.w - q.y) / (6.0f * d + e)), d / (q.x + e), q.x);
}

float3 HSVtoRGB(float3 hsv)
{
    float4 K = float4(1.0f, 2.0f / 3.0f, 1.0f / 3.0f, 3.0f);
    float3 p = abs(frac(hsv.xxx + K.xyz) * 6.0f - K.www);
    return hsv.z * lerp(K.xxx, clamp(p - K.xxx, 0.0f, 1.0f), hsv.y);
}

// Toon shading with HSV quantization
float3 ToonShadingHSV(float3 color, uint levels)
{
    float3 hsv = RGBtoHSV(color);
    
    // Quantize hue and saturation more aggressively
    hsv.x = floor(hsv.x * (levels * 2)) / (levels * 2);
    hsv.y = floor(hsv.y * levels) / levels;
    hsv.z = floor(hsv.z * levels) / levels;
    
    return HSVtoRGB(hsv);
}

[numthreads(8, 8, 1)]
void CSMain(uint3 id : SV_DispatchThreadID)
{
    // Bounds check
    if (id.x >= textureWidth || id.y >= textureHeight)
        return;
    
    // Sample original color
    float3 originalColor = inputTexture[id.xy].rgb;
    
    // Apply toon shading
    float3 toonColor = QuantizeColor(originalColor, colorLevels);
    
    // Alternative: Use HSV quantization for more artistic effect
    // float3 toonColor = ToonShadingHSV(originalColor, colorLevels);
    
    // Alternative: Use smooth quantization with dithering
    // float3 toonColor = QuantizeColorSmooth(originalColor, colorLevels, float2(id.xy));
    
    // Enhance contrast for more cartoon-like appearance
    toonColor = pow(abs(toonColor), 0.8f);
    
    // Write result
    outputTexture[id.xy] = float4(toonColor, 1.0f);
}
