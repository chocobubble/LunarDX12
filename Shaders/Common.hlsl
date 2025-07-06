struct Light
{
	float3 strength;
	float fallOffStart;
	float3 direction;
	float fallOffEnd;
	float3 position;
	float spotPower;
};

cbuffer BasicConstants : register(b0)
{
	float4x4 view;
	float4x4 projection;
	float3 eyePos;
    uint debugMode;  // Debug mode flags
	float4 ambientLight;
	Light lights[3];
	float4x4 shadowTransform;

}

cbuffer ObjectConstants : register(b1)
{
	float4x4 world;
	float4x4 worldInvTranspose;
	int textureIndex;	
}

float3x3 GetTBN(float3 normal, float3 tangent)
{
	float3 N = normalize(normal);
	float3 T = normalize(tangent);
	
	T = normalize(T - dot(T, N) * N);
	
	float3 B = cross(N, T);

	return float3x3(T, B, N);
}

float3 NormalTSToWS(float3 normalSample, float3x3 TBN)
{
	float3 n = normalSample * 2.0f - 1.0f;
	return normalize(mul(n, TBN));
}