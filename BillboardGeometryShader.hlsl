struct GeometryInput
{
	float4 posH : SV_POSITION;
	float2 size : TEXCOORD;
};

struct PixelInput
{
	float4 posH : SV_POSITION;
	float2 uv : TEXCOORD;
	uint primID : SV_PrimitiveID;
};

[maxvertexcount(4)]
void main(point GeometryInput gIn[1], inout TriangleStream<PixelInput> stream, uint primID : SV_PrimitiveID)
{
	float width = gIn[0].size.x * 0.5;
	float height = gIn[0].size.y * 0.5;

	PixelInput pIn;
	pIn.primID = primID;
	
	// top right
	pIn.posH = gIn[0].posH + float4(width, height, 0.0, 0.0);
	pIn.uv = float2(1.0, 0.0);
	stream.Append(pIn);

	// top left
	pIn.posH = gIn[0].posH + float4(-width, height, 0.0, 0.0);
	pIn.uv = float2(0.0, 0.0);
	stream.Append(pIn);

	// bottom right
	pIn.posH = gIn[0].posH + float4(width, -height, 0.0, 0.0);
	pIn.uv = float2(1.0, 1.0);
	stream.Append(pIn);

	// bottom left
	pIn.posH = gIn[0].posH + float4(-width, -height, 0.0, 0.0);
	pIn.uv = float2(0.0, 1.0);
	stream.Append(pIn);
}
