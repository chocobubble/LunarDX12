struct PatchConstOutput
{
	float edges[3] : SV_TessFactor;
	float inside : SV_InsideTessFactor;
};

struct HullOut
{
	float4 pos : SV_POSITION;
	float4 color : COLOR;
	float2 texCoord : TEXCOORD;
	float3 normal : NORMAL;
	float3 posW : POSITION;
	float3 tangent : TANGENT;
};

struct DomainOut
{
	float4 pos : SV_POSITION;
	float4 color : COLOR;
	float2 texCoord : TEXCOORD;
	float3 normal : NORMAL;
	float3 posW : POSITION;
	float3 tangent : TANGENT;
};

[domain("tri")]
DomainOut main(PatchConstOutput patchConst, float3 uv : SV_DomainLocation, const OutputPatch<HullOut, 3> tri)
{
	DomainOut domainOut;

	// barycentric coordinate
	float u = uv.x;
	float v = uv.y;
	float w = uv.z;
	
	domainOut.pos    = tri[0].pos    * w + tri[1].pos    * u + tri[2].pos    * v;
	domainOut.color  = tri[0].color  * w + tri[1].color  * u + tri[2].color  * v;
	domainOut.texCoord = tri[0].texCoord * w + tri[1].texCoord * u + tri[2].texCoord * v;
	domainOut.normal = normalize(tri[0].normal * w + tri[1].normal * u + tri[2].normal * v);
	domainOut.posW   = tri[0].posW   * w + tri[1].posW   * u + tri[2].posW   * v;
	domainOut.tangent= normalize(tri[0].tangent * w + tri[1].tangent * u + tri[2].tangent * v);

	return domainOut;
}