Texture2D albedoTexture : register(t3);
Texture2D aoTexture : register(t5);
Texture2D metallicTexture : register(t7);
Texture2D roughnessTexture : register(t8);
SamplerState g_sampler : register(s0);

cbuffer MaterialConstants : register(b2)
{
	float3 albedo; 
	float metallic; 
	float3 emissive;
	float roughness; 
	float3 F0;       
	float ao;        
};

struct Material 
{
    float3 albedo; 
    float metallic; 
    float3 emissive;
    float roughness; 
    float3 F0;       
    float ao;       
};

Material GetMaterial(float2 texCoord)
{
    Material mat;
    mat.albedo = albedo;
    mat.metallic = metallic;
    mat.emissive = emissive;
    mat.roughness = roughness;
    mat.F0 = F0;
    mat.ao = ao;

    uint AOEnabledMask = 1 << 6; 
    if ((debugFlags & AOEnabledMask) != 0)
    {        
        mat.ao = aoTexture.Sample(g_sampler, texCoord).r;
    }
    uint MetallicDebugMask = 1 << 9; 
    if ((debugFlags & MetallicDebugMask) != 0)
    {
        mat.metallic = metallicTexture.Sample(g_sampler, texCoord).r;
    }
    uint RoughnessDebugMask = 1 << 10; 
    if ((debugFlags & RoughnessDebugMask) != 0)
    {        
        mat.roughness = roughnessTexture.Sample(g_sampler, texCoord).r;
    }
    uint AlbedoDebugMask = 1 << 11;
    if ((debugFlags & AlbedoDebugMask) != 0)
    {        
        mat.albedo = albedoTexture.Sample(g_sampler, texCoord).rgb;
    }

    return mat;
}

// D Function: GGX Microfacet Distribution Function
float GGX(float nDotH, float roughness)
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

float G1(float cosTheta, float roughness)
{
    // G1(v) = 2 / (1 + sqrt(1 + alpha^2 * tan^2(θv)))
    // θv : angle between normal and view direction
    // α : roughness

    float alpha = roughness;

    // tan^2(θ) = (1 - cos^2(θ)) / cos^2(θ)
    return 2 / (1 + sqrt(1 + alpha * alpha * (1 - cosTheta * cosTheta) / (cosTheta * cosTheta)));
}

// G Function: Smith Function
float SmithG(float nDotL, float nDotV, float roughness)
{
	// G = G1(lightDir) * G1(viewDir)

	float G1V = G1(nDotV, roughness);
	float G1L = G1(nDotL, roughness);

	return G1V * G1L;
}

float3 CookTorrance(float3 normal, float3 nLightDir, float3 nToEye, Material material)
{
	// fr = kd * diffuse + ks * specular  (kd = diffuse weight, ks = specular weight)
	float3 diffuse = material.albedo / 3.141592; 
	float3 nHalfVector = normalize(nLightDir + nToEye); 
	float nDotH = max(dot(normal, nHalfVector), 0.01f); // Avoid division by zero
	float nDotL = max(dot(normal, nLightDir), 0.01f);
	float nDotV = max(dot(normal, nToEye), 0.01f);

	// (D·F·G)/(4·cosθᵢ·cosθₒ)` - Light that reflects off the surface
	float D = GGX(nDotH, material.roughness);
	float3 F = SchlickFresnel(material.F0, nDotH);
	float G = SmithG(nDotL, nDotV, material.roughness);
	float3 specular = (D * F * G) / (4 * nDotL * nDotV);
    
	float3 ks = F;
	float3 kd = (1 - material.metallic) * (1 - F);
	return diffuse * kd + specular * ks;
}