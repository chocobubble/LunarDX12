// EdgeDetection.hlsl - Outline detection for NPR effects

Texture2D<float4> inputTexture : register(t0);
Texture2D<float> depthTexture : register(t1);
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

cbuffer EdgeConstants : register(b1)
{
    float3 edgeColor;
    float edgeWidth;
    float depthSensitivity;
    float normalSensitivity;
    float colorSensitivity;
    float padding;
};

// Sobel edge detection kernels
static const float3x3 sobelX = {
    -1, 0, 1,
    -2, 0, 2,
    -1, 0, 1
};

static const float3x3 sobelY = {
    -1, -2, -1,
     0,  0,  0,
     1,  2,  1
};

// Get luminance from color
float GetLuminance(float3 color)
{
    return dot(color, float3(0.299f, 0.587f, 0.114f));
}

// Sobel edge detection on luminance
float SobelEdgeDetection(uint2 coord)
{
    float edgeX = 0;
    float edgeY = 0;
    
    // Sample 3x3 neighborhood
    [unroll]
    for (int y = -1; y <= 1; ++y)
    {
        [unroll]
        for (int x = -1; x <= 1; ++x)
        {
            int2 sampleCoord = clamp(
                int2(coord) + int2(x, y),
                int2(0, 0),
                int2(textureWidth - 1, textureHeight - 1)
            );
            
            float luminance = GetLuminance(inputTexture[sampleCoord].rgb);
            
            edgeX += luminance * sobelX[y + 1][x + 1];
            edgeY += luminance * sobelY[y + 1][x + 1];
        }
    }
    
    return sqrt(edgeX * edgeX + edgeY * edgeY);
}

// Depth-based edge detection
float DepthEdgeDetection(uint2 coord)
{
    float centerDepth = depthTexture[coord];
    
    float edgeStrength = 0;
    
    // Check 4-connected neighbors
    int2 offsets[4] = {
        int2(1, 0), int2(-1, 0),
        int2(0, 1), int2(0, -1)
    };
    
    [unroll]
    for (int i = 0; i < 4; ++i)
    {
        int2 sampleCoord = clamp(
            int2(coord) + offsets[i],
            int2(0, 0),
            int2(textureWidth - 1, textureHeight - 1)
        );
        
        float sampleDepth = depthTexture[sampleCoord];
        float depthDiff = abs(centerDepth - sampleDepth);
        
        edgeStrength += depthDiff;
    }
    
    return edgeStrength * depthSensitivity;
}

// Roberts cross-gradient edge detection (faster alternative)
float RobertsEdgeDetection(uint2 coord)
{
    int2 coord1 = clamp(int2(coord) + int2(1, 1), int2(0, 0), int2(textureWidth - 1, textureHeight - 1));
    int2 coord2 = clamp(int2(coord) + int2(1, 0), int2(0, 0), int2(textureWidth - 1, textureHeight - 1));
    int2 coord3 = clamp(int2(coord) + int2(0, 1), int2(0, 0), int2(textureWidth - 1, textureHeight - 1));
    
    float lum0 = GetLuminance(inputTexture[coord].rgb);
    float lum1 = GetLuminance(inputTexture[coord1].rgb);
    float lum2 = GetLuminance(inputTexture[coord2].rgb);
    float lum3 = GetLuminance(inputTexture[coord3].rgb);
    
    float edge1 = abs(lum0 - lum1);
    float edge2 = abs(lum2 - lum3);
    
    return sqrt(edge1 * edge1 + edge2 * edge2);
}

[numthreads(8, 8, 1)]
void CSMain(uint3 id : SV_DispatchThreadID)
{
    // Bounds check
    if (id.x >= textureWidth || id.y >= textureHeight)
        return;
    
    // Sample original color
    float3 originalColor = inputTexture[id.xy].rgb;
    
    // Detect edges using multiple methods
    float colorEdge = SobelEdgeDetection(id.xy) * colorSensitivity;
    float depthEdge = DepthEdgeDetection(id.xy);
    
    // Alternative: Use Roberts edge detection for better performance
    // float colorEdge = RobertsEdgeDetection(id.xy) * colorSensitivity;
    
    // Combine edge signals
    float totalEdge = max(colorEdge, depthEdge);
    
    // Apply threshold
    float edgeMask = step(edgeThreshold, totalEdge);
    
    // Smooth edge transition
    float smoothEdge = smoothstep(edgeThreshold * 0.5f, edgeThreshold * 1.5f, totalEdge);
    
    // Mix original color with edge color
    float3 finalColor = lerp(originalColor, edgeColor, smoothEdge);
    
    // Alternative: Pure outline mode
    // float3 finalColor = lerp(originalColor, edgeColor, edgeMask);
    
    // Write result
    outputTexture[id.xy] = float4(finalColor, 1.0f);
}
