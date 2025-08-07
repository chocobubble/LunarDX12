#include "ParticleSystemEnhanced.h"

namespace Lunar::ParticlePresets
{

ParticleSystemEnhanced::ParticleType CreateFireParticle()
{
    ParticleSystemEnhanced::ParticleType fire;
    fire.name = "Fire";
    fire.blendMode = ParticleSystemEnhanced::BlendMode::Additive;
    fire.texturePath = "textures/fire_particle.png";
    fire.sizeRange = {0.02f, 0.08f};
    fire.lifetimeRange = {0.5f, 2.0f};
    fire.velocityBase = {0.0f, 2.0f, 0.0f};
    fire.velocityVariation = {0.5f, 1.0f, 0.5f};
    fire.colorStart = {1.0f, 0.8f, 0.2f, 1.0f}; // 밝은 주황색
    fire.colorEnd = {1.0f, 0.2f, 0.0f, 0.0f};   // 빨간색으로 페이드
    fire.gravity = -1.0f; // 약한 중력 (불은 위로 올라감)
    fire.drag = 0.2f;
    fire.fadeOverLifetime = true;
    fire.scaleOverLifetime = true;
    return fire;
}

ParticleSystemEnhanced::ParticleType CreateSmokeParticle()
{
    ParticleSystemEnhanced::ParticleType smoke;
    smoke.name = "Smoke";
    smoke.blendMode = ParticleSystemEnhanced::BlendMode::Alpha;
    smoke.texturePath = "textures/smoke_particle.png";
    smoke.sizeRange = {0.05f, 0.15f};
    smoke.lifetimeRange = {2.0f, 5.0f};
    smoke.velocityBase = {0.0f, 1.0f, 0.0f};
    smoke.velocityVariation = {1.0f, 0.5f, 1.0f};
    smoke.colorStart = {0.8f, 0.8f, 0.8f, 0.8f}; // 회색
    smoke.colorEnd = {0.6f, 0.6f, 0.6f, 0.0f};   // 어두운 회색으로 페이드
    smoke.gravity = -2.0f; // 연기는 위로 올라감
    smoke.drag = 0.5f; // 높은 공기 저항
    smoke.fadeOverLifetime = true;
    smoke.scaleOverLifetime = true;
    return smoke;
}

ParticleSystemEnhanced::ParticleType CreateSparkParticle()
{
    ParticleSystemEnhanced::ParticleType spark;
    spark.name = "Spark";
    spark.blendMode = ParticleSystemEnhanced::BlendMode::Additive;
    spark.texturePath = "textures/spark_particle.png";
    spark.sizeRange = {0.005f, 0.02f};
    spark.lifetimeRange = {0.2f, 1.0f};
    spark.velocityBase = {0.0f, 3.0f, 0.0f};
    spark.velocityVariation = {3.0f, 2.0f, 3.0f};
    spark.colorStart = {1.0f, 1.0f, 0.8f, 1.0f}; // 밝은 노란색
    spark.colorEnd = {1.0f, 0.5f, 0.0f, 0.0f};   // 주황색으로 페이드
    spark.gravity = -9.81f; // 일반 중력
    spark.drag = 0.1f;
    spark.fadeOverLifetime = true;
    spark.scaleOverLifetime = false;
    return spark;
}

ParticleSystemEnhanced::ParticleType CreateMagicParticle()
{
    ParticleSystemEnhanced::ParticleType magic;
    magic.name = "Magic";
    magic.blendMode = ParticleSystemEnhanced::BlendMode::Screen;
    magic.texturePath = "textures/magic_particle.png";
    magic.sizeRange = {0.03f, 0.06f};
    magic.lifetimeRange = {1.0f, 3.0f};
    magic.velocityBase = {0.0f, 0.5f, 0.0f};
    magic.velocityVariation = {2.0f, 1.0f, 2.0f};
    magic.colorStart = {0.5f, 0.8f, 1.0f, 1.0f}; // 밝은 파란색
    magic.colorEnd = {0.8f, 0.3f, 1.0f, 0.0f};   // 보라색으로 페이드
    magic.gravity = 0.0f; // 중력 없음 (마법은 떠다님)
    magic.drag = 0.3f;
    magic.fadeOverLifetime = true;
    magic.scaleOverLifetime = true;
    return magic;
}

ParticleSystemEnhanced::ParticleType CreateExplosionParticle()
{
    ParticleSystemEnhanced::ParticleType explosion;
    explosion.name = "Explosion";
    explosion.blendMode = ParticleSystemEnhanced::BlendMode::Additive;
    explosion.texturePath = "textures/explosion_particle.png";
    explosion.sizeRange = {0.1f, 0.3f};
    explosion.lifetimeRange = {0.3f, 1.0f};
    explosion.velocityBase = {0.0f, 0.0f, 0.0f};
    explosion.velocityVariation = {5.0f, 5.0f, 5.0f}; // 모든 방향으로 폭발
    explosion.colorStart = {1.0f, 1.0f, 1.0f, 1.0f}; // 밝은 흰색
    explosion.colorEnd = {1.0f, 0.3f, 0.0f, 0.0f};   // 빨간색으로 페이드
    explosion.gravity = 0.0f;
    explosion.drag = 0.8f; // 높은 저항으로 빠르게 감속
    explosion.fadeOverLifetime = true;
    explosion.scaleOverLifetime = true;
    return explosion;
}

ParticleSystemEnhanced::ParticleType CreateSnowParticle()
{
    ParticleSystemEnhanced::ParticleType snow;
    snow.name = "Snow";
    snow.blendMode = ParticleSystemEnhanced::BlendMode::Alpha;
    snow.texturePath = "textures/snow_particle.png";
    snow.sizeRange = {0.01f, 0.03f};
    snow.lifetimeRange = {5.0f, 10.0f};
    snow.velocityBase = {0.0f, -1.0f, 0.0f}; // 아래로 떨어짐
    snow.velocityVariation = {0.5f, 0.2f, 0.5f};
    snow.colorStart = {1.0f, 1.0f, 1.0f, 0.9f}; // 흰색
    snow.colorEnd = {1.0f, 1.0f, 1.0f, 0.0f};   // 투명하게 페이드
    snow.gravity = -2.0f; // 약한 중력
    snow.drag = 0.9f; // 높은 공기 저항 (천천히 떨어짐)
    snow.fadeOverLifetime = true;
    snow.scaleOverLifetime = false;
    return snow;
}

ParticleSystemEnhanced::ParticleType CreateRainParticle()
{
    ParticleSystemEnhanced::ParticleType rain;
    rain.name = "Rain";
    rain.blendMode = ParticleSystemEnhanced::BlendMode::Alpha;
    rain.texturePath = "textures/rain_particle.png";
    rain.sizeRange = {0.005f, 0.01f};
    rain.lifetimeRange = {1.0f, 3.0f};
    rain.velocityBase = {0.0f, -10.0f, 0.0f}; // 빠르게 아래로
    rain.velocityVariation = {1.0f, 2.0f, 1.0f};
    rain.colorStart = {0.7f, 0.8f, 1.0f, 0.8f}; // 연한 파란색
    rain.colorEnd = {0.5f, 0.6f, 0.8f, 0.0f};   // 어두운 파란색으로 페이드
    rain.gravity = -9.81f; // 일반 중력
    rain.drag = 0.05f; // 낮은 공기 저항
    rain.fadeOverLifetime = true;
    rain.scaleOverLifetime = false;
    return rain;
}

} // namespace Lunar::ParticlePresets
