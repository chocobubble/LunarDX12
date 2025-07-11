Texture2D<float4> inputTexture : register(t0, space3);
RWTexture2D<float4> outputTexture : register(u0, space1);

static const float weights[11] = {
    0.0947416f, 0.118318f, 0.147761f, 0.118318f, 0.0947416f,
    0.118318f,  0.147761f, 0.118318f, 0.0947416f, 0.118318f,
    0.147761f
};

static const int blurRadius = 5;

groupshared float4 sharedColors[64 + 2 * blurRadius];

[numthreads(64, 1, 1)]
void main(uint3 groupThreadID : SV_GroupThreadID, uint3 dispatchThreadID : SV_DispatchThreadID)
{
	uint width, height;
	inputTexture.GetDimensions(width, height);
	
    // Caching
    if (groupThreadID.x < blurRadius)
    {
        int x = max(0, dispatchThreadID.x - blurRadius);
        sharedColors[groupThreadID.x] = inputTexture[int2(x, dispatchThreadID.y)];
    }

    if (groupThreadID.x >= 64 - blurRadius)
    {
        int x = min(width - 1, dispatchThreadID.x + blurRadius);
        sharedColors[groupThreadID.x + blurRadius * 2] = inputTexture[int2(x, dispatchThreadID.y)];
    }

    int sampleX = min(dispatchThreadID.x, width - 1);
    sharedColors[groupThreadID.x + blurRadius] = inputTexture[int2(sampleX, dispatchThreadID.y)];

    GroupMemoryBarrierWithGroupSync();

    // Blur X Direction
    float4 color = float4(0.0, 0.0, 0.0, 0.0);
    for (int i = -blurRadius; i <= blurRadius; ++i)
    {
        int index = groupThreadID.x + blurRadius + i;
        color += weights[i + blurRadius] * sharedColors[index];
    }
    outputTexture[int2(dispatchThreadID.x, dispatchThreadID.y)] = color;

}