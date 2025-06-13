struct VertexIn
{
    float3 pos : POSITION;
    float4 color : COLOR;
};

struct PixelIn
{
    float4 pos : SV_POSITION;
    float4 color : COLOR;
};

PixelIn main(VertexIn vIn)
{
    PixelIn pIn;
    pIn.pos = float4(vIn.pos, 1.0f);
    pIn.color = vIn.color;
    return pIn;
}