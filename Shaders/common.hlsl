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