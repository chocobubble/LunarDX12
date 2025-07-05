struct Particle
{
    float3 position;
    float3 velocity;
    float3 force;
    float4 color;
    float lifetime;
    float age;    
}

StructuredBuffer<Particle> particlesInput : register(t0, space2);
RWStructuredBuffer<Particle> particlesOutput : register(u0);

[numthreads(64, 1, 1)]
void main(int3 dispatchThreadID : SV_DispatchThreadID)
{
    Particle particle = particlesInput[dispatchThreadID.x];

    // For simplicity, assume deltaTime is 0.016 seconds
    float deltaTime = 0.016f;
    particle.age += 0.016f;
    particle.position += particle.velocity * deltaTime;
    particle.velocity += particle.force * deltaTime;

    particlesOutput[dispatchThreadID.x] = particle;
}