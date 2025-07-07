struct Particle
{
    float3 position;
    float3 velocity;
    float3 force;
    float4 color;
    float lifetime;
    float age;
};

struct VertexIn 
{
    float3 pos : POSITION;
};

struct GeometryIn
{
    float4 posH : SV_POSITION;
    float2 size : TEXCOORD;
    float4 color : COLOR;
    uint isActive : ACTIVE; 
};

cbuffer BasicConstants : register(b0)
{
    float4x4 view;
    float4x4 projection;
    float4x4 viewProjection;
    float3 eyePosition;
    float padding1;
    float totalTime;
    float deltaTime;
    float2 padding2;
};

StructuredBuffer<Particle> particles : register(t0, space2);

GeometryIn main(VertexIn vIn, uint instanceID : SV_InstanceID)
{
    Particle particle = particles[instanceID];
    GeometryIn gIn;
    
    bool isActive = particle.age < particle.lifetime;
    gIn.isActive = isActive ? 1 : 0;
    
    float4 posW = float4(particle.position, 1.0f);
    float4 posV = mul(posW, view);
    gIn.posH = mul(posV, projection);
    
    gIn.size = float2(0.2f, 0.2f);
    gIn.color = particle.color;
    
    return gIn;
}