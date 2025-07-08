struct GeometryIn
{
    float4 posH : SV_POSITION;
    float2 size : TEXCOORD;
    float4 color : COLOR;
    uint isActive : ACTIVE;
};

struct PixelIn 
{
    float4 posH : SV_POSITION;
    float4 color : COLOR;
};

[maxvertexcount(4)]
void main(point GeometryIn gIn[1], inout TriangleStream<PixelIn> stream)
{
    if (gIn[0].isActive == 0) {
        return; 
    }

    PixelIn pIn;
    pIn.color = gIn[0].color;

    float width = gIn[0].size.x * 0.5f;
    float height = gIn[0].size.y * 0.5f;

	// top left
	pIn.posH = gIn[0].posH + float4(-width, height, 0.0, 0.0);
	stream.Append(pIn);

	// top right
	pIn.posH = gIn[0].posH + float4(width, height, 0.0, 0.0);
	stream.Append(pIn);

	// bottom left
	pIn.posH = gIn[0].posH + float4(-width, -height, 0.0, 0.0);
	stream.Append(pIn);
	
	// bottom right
	pIn.posH = gIn[0].posH + float4(width, -height, 0.0, 0.0);
	stream.Append(pIn);
}