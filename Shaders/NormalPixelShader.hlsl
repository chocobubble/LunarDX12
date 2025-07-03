struct PixelIn
{
	float4 pos : SV_POSITION;
	float3 color : COLOR;
};

float4 main(PixelIn pIn) : SV_TARGET
{
	return float4(pIn.color, 1.0);
}
