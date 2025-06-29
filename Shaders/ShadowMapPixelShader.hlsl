struct PixelIn
{
	float4 pos : SV_POSITION;
	float2 texCoord : TEXCOORD;
};

float4 main(PixelIn pIn) : SV_TARGET
{
	return pIn.pos;
}
