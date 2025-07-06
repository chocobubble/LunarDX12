cbuffer MaterialConstants : register(b2)
{
	float3 albedo; 
	float metallic; 
	float3 emissive;
	float roughness; 
	float3 F0;       
	float ao;        
};

// D Function: GGX Microfacet Distribution Function
float GGX(float nDotH)
{
    // D(h) = alpha^2 / (PI * ((cosθh)^2 * (alpha^2 - 1) + 1)^2)
    // h : halfway vector
    // cosθh : dot product of normal and halfway vector
    // alpha : roughness^2

    float alphaSq = max(roughness * roughness, 0.001f); // Avoid division by zero

    return alphaSq / (3.141592 * pow(nDotH * nDotH * (alphaSq - 1) + 1, 2));
}

// F Function: Fresnel Reflectance
float3 SchlickFresnel(float3 F0, float nDotH)
{
    // F(θ) = F0 + (1 - F0) * (1 - cosθ)^5
    // F0 : base reflectance at normal incidence

    return F0 + (1 - F0) * pow(1 - nDotH, 5);
}

float G1(float cosTheta)
{
    // G1(v) = 2 / (1 + sqrt(1 + alpha^2 * tan^2(θv)))
    // θv : angle between normal and view direction
    // α : roughness

    float alpha = roughness;

    // tan^2(θ) = (1 - cos^2(θ)) / cos^2(θ)
    return 2 / (1 + sqrt(1 + alpha * alpha * (1 - cosTheta * cosTheta) / (cosTheta * cosTheta)));
}

// G Function: Smith Function
float SmithG(float nDotL, float nDotV)
{
	// G = G1(lightDir) * G1(viewDir)

	float G1V = G1(nDotV);
	float G1L = G1(nDotL);

	return G1V * G1L;
}

float3 CookTorrance(float3 normal, float3 nLightDir, float3 nToEye)
{
	// fr = kd * diffuse + ks * specular  (kd = diffuse weight, ks = specular weight)
	float3 diffuse = albedo / 3.141592; 
	float3 nHalfVector = normalize(nLightDir + nToEye); 
	float nDotH = max(dot(normal, nHalfVector), 0.01f); // Avoid division by zero
	float nDotL = max(dot(normal, nLightDir), 0.01f);
	float nDotV = max(dot(normal, nToEye), 0.01f);

	// (D·F·G)/(4·cosθᵢ·cosθₒ)` - Light that reflects off the surface
	float D = GGX(nDotH);
	float3 F = SchlickFresnel(F0, nDotH);
	float G = SmithG(nDotL, nDotV);
	float3 specular = (D * F * G) / (4 * nDotL * nDotV);
    
	float3 ks = F;
	float3 kd = (1 - metallic) * (1 - F);
	return diffuse * kd + specular * ks;
}