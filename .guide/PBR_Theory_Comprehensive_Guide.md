# Physically Based Rendering (PBR) - Comprehensive Theory Guide

A complete theoretical foundation for understanding and implementing Physically Based Rendering, explained from first principles for beginners.

## 📖 Table of Contents

1. [What is PBR? - Complete Introduction](#what-is-pbr---complete-introduction)
2. [Physics Foundations - Light and Matter](#physics-foundations---light-and-matter)
3. [Mathematical Framework - BRDF Theory](#mathematical-framework---brdf-theory)
4. [Material Modeling - How Surfaces Work](#material-modeling---how-surfaces-work)
5. [Lighting Models - Direct and Indirect](#lighting-models---direct-and-indirect)
6. [Energy Conservation - The Fundamental Law](#energy-conservation---the-fundamental-law)
7. [DirectX 12 Implementation Theory](#directx-12-implementation-theory)
8. [Mathematical Optimizations](#mathematical-optimizations)
9. [Practical Application Guidelines](#practical-application-guidelines)

---

## What is PBR? - Complete Introduction

### The Problem PBR Solves

Before PBR, computer graphics used **ad-hoc lighting models** - mathematical formulas that looked good but weren't based on real physics. This created several problems:

**1. Inconsistent Results:**
- A material might look good under one lighting condition but terrible under another
- Artists had to manually tweak materials for each scene
- No predictable behavior when lighting changed

**2. Non-Physical Behavior:**
- Materials could reflect more light than they received (violating energy conservation)
- Surfaces didn't behave like real-world materials
- Difficult to achieve photorealistic results

**3. Artist Workflow Issues:**
- Complex, non-intuitive parameters
- Trial-and-error approach to material creation
- Difficult to match real-world references

### What PBR Actually Means

**Physically Based Rendering** means using the actual laws of physics to simulate how light interacts with surfaces. Instead of guessing what looks good, we calculate what **should** happen based on real-world physics.

**Key Insight:** If we simulate physics correctly, the results will automatically look realistic because they match what our eyes expect to see in the real world.

### The Three Pillars of PBR

**1. Energy Conservation**
```
Reflected Light ≤ Incident Light
```
A surface can never reflect more light than it receives. This seems obvious, but many old rendering techniques violated this rule.

**2. Physical Material Properties**
Instead of arbitrary parameters, we use measurements from real materials:
- **Metallic/Non-metallic:** Based on electrical conductivity
- **Roughness:** Based on surface microscopic structure
- **Index of Refraction:** Based on optical properties

**3. Physically Accurate Light Transport**
We use the actual equations that describe how light bounces around in the real world.

### Why PBR Matters for Real-Time Graphics

**Consistency:** Materials look correct under any lighting condition
**Predictability:** Artists can use real-world references and measurements
**Efficiency:** Less trial-and-error means faster content creation
**Quality:** Automatic photorealism when done correctly

---

## Physics Foundations - Light and Matter

### What is Light? - The Electromagnetic Perspective

Light is an **electromagnetic wave** - a combination of oscillating electric and magnetic fields that travel through space.

**Maxwell's Equations** (the fundamental laws of electromagnetism):
```
∇ × E = -∂B/∂t    (Faraday's Law)
∇ × H = J + ∂D/∂t  (Ampère's Law)
∇ · D = ρ          (Gauss's Law)
∇ · B = 0          (No Magnetic Monopoles)
```

**What this means in simple terms:**
- **E**: Electric field strength
- **B**: Magnetic field strength
- Light is a wave where E and B oscillate perpendicular to each other
- The frequency of oscillation determines the color we see

### How Light Interacts with Matter

When light hits a surface, several things can happen:

**1. Reflection**
- Light bounces off the surface
- Angle of incidence = Angle of reflection (for perfect mirrors)
- Real surfaces scatter light in many directions

**2. Refraction**
- Light enters the material and bends
- The amount of bending depends on the **refractive index**
- Different colors bend by different amounts (dispersion)

**3. Absorption**
- Light energy is converted to heat
- Different materials absorb different wavelengths
- This is why objects have color

**4. Transmission**
- Light passes through the material
- Important for transparent/translucent materials

### Refractive Index - The Key Material Property

The **refractive index (n)** tells us how much light slows down when entering a material:

```
n = c / v
```
- **c**: Speed of light in vacuum (299,792,458 m/s)
- **v**: Speed of light in the material
- **n**: Refractive index (dimensionless)

**Examples:**
- Air: n ≈ 1.0
- Water: n ≈ 1.33
- Glass: n ≈ 1.5
- Diamond: n ≈ 2.42

**Complex Refractive Index:**
For materials that absorb light (like metals), we use:
```
n = n' + ik'
```
- **n'**: Real part (refraction)
- **k'**: Imaginary part (absorption)

### Fresnel Equations - The Math of Reflection

The **Fresnel equations** tell us exactly how much light is reflected vs. transmitted when light hits a surface.

**For unpolarized light hitting a smooth interface:**

The reflection coefficient depends on:
- **θᵢ**: Angle of incidence
- **n₁**: Refractive index of first medium
- **n₂**: Refractive index of second medium

**S-polarized reflection:**
```
rs = (n₁cosθᵢ - n₂cosθₜ) / (n₁cosθᵢ + n₂cosθₜ)
```

**P-polarized reflection:**
```
rp = (n₂cosθᵢ - n₁cosθₜ) / (n₂cosθᵢ + n₁cosθₜ)
```

**Total reflectance:**
```
R = (|rs|² + |rp|²) / 2
```

**What this means practically:**
- At normal incidence (straight on): R depends only on refractive indices
- At grazing angles (nearly parallel): R approaches 1 (total reflection)
- This is why water looks more reflective when viewed from the side

### Schlick's Approximation - Making Fresnel Practical

The full Fresnel equations are expensive to compute in real-time. **Christophe Schlick** developed a simple approximation:

```
F(θ) = F₀ + (1 - F₀)(1 - cosθ)⁵
```

**Where:**
- **F₀**: Reflectance at normal incidence
- **θ**: Angle between surface normal and view direction
- **cosθ**: Dot product of normal and view vectors

**Calculating F₀:**
```
F₀ = ((n₁ - n₂) / (n₁ + n₂))²
```

**Typical F₀ values:**
- Water: 0.02 (2% reflection at normal incidence)
- Glass: 0.04 (4% reflection)
- Plastic: 0.04-0.06
- Metals: 0.5-1.0 (much higher!)

### Why Metals are Different

**Dielectrics (Non-metals):**
- Low F₀ values (2-8%)
- Colored diffuse reflection
- Colorless specular reflection

**Conductors (Metals):**
- High F₀ values (50-100%)
- No diffuse reflection (light doesn't penetrate)
- Colored specular reflection

This fundamental difference is why PBR treats metals and non-metals so differently.

---

## Mathematical Framework - BRDF Theory

### What is a BRDF?

**BRDF** stands for **Bidirectional Reflectance Distribution Function**. It's a mathematical function that describes how light reflects off a surface.

**The Fundamental Question BRDF Answers:**
"If light comes from direction A and hits this surface, how much of that light will be reflected in direction B?"

### BRDF Definition - Breaking it Down

**Mathematical Definition:**
```
fr(ωᵢ, ωₒ) = dLₒ(ωₒ) / (dEᵢ(ωᵢ) · cosθᵢ)
```

**Let's understand each symbol:**

**ωᵢ (omega-i):** Incident light direction (where light comes FROM)
**ωₒ (omega-o):** Outgoing light direction (where light goes TO)
**dLₒ:** Differential radiance leaving the surface
**dEᵢ:** Differential irradiance arriving at the surface
**cosθᵢ:** Cosine of angle between incident direction and surface normal

**In Simple Terms:**
The BRDF tells us: "For every unit of light energy arriving from direction ωᵢ, how much light energy leaves in direction ωₒ?"

### Understanding Radiance and Irradiance

**Radiance (L):**
- Light energy flowing in a specific direction
- Units: Watts per square meter per steradian [W/m²/sr]
- What a camera sensor or your eye actually measures

**Irradiance (E):**
- Total light energy arriving at a surface from all directions
- Units: Watts per square meter [W/m²]
- Related to how "bright" a surface appears to be lit

**The Relationship:**
```
dE = L · cosθ · dω
```
- **dω**: Differential solid angle (tiny cone of directions)
- **cosθ**: Lambert's cosine law (surfaces appear dimmer when viewed at an angle)

### BRDF Properties - The Rules Every BRDF Must Follow

**1. Reciprocity (Helmholtz Reciprocity):**
```
fr(ωᵢ, ωₒ) = fr(ωₒ, ωᵢ)
```
**What this means:** Light paths are reversible. If you swap the light source and camera positions, you get the same result.

**2. Energy Conservation:**
```
∫Ω fr(ωᵢ, ωₒ) cosθᵢ dωᵢ ≤ 1
```
**What this means:** The total amount of light reflected in all directions cannot exceed the amount of light received.

**3. Non-negativity:**
```
fr(ωᵢ, ωₒ) ≥ 0
```
**What this means:** You can't have "negative light" - surfaces can't absorb light and emit darkness.

### The Rendering Equation - The Master Formula

The **rendering equation** describes how light bounces around in a scene:

```
Lₒ(p, ωₒ) = Le(p, ωₒ) + ∫Ω fr(p, ωᵢ, ωₒ) Li(p, ωᵢ) cosθᵢ dωᵢ
```

**Breaking it down:**
- **Lₒ(p, ωₒ):** Light leaving point p in direction ωₒ
- **Le(p, ωₒ):** Light emitted by the surface itself (for light sources)
- **∫Ω:** Integral over all incoming directions (hemisphere above the surface)
- **fr(p, ωᵢ, ωₒ):** BRDF at point p
- **Li(p, ωᵢ):** Light arriving from direction ωᵢ
- **cosθᵢ:** Lambert's cosine law

**In Plain English:**
"The light leaving a surface equals the light it emits plus all the light it reflects from every possible incoming direction."

### Monte Carlo Integration - Solving the Unsolvable

The rendering equation involves an integral over all directions - infinite directions! We can't compute this exactly in real-time.

**Monte Carlo Solution:**
Instead of integrating over all directions, we sample a few random directions and average the results:

```
Lₒ ≈ (1/N) Σᵢ fr(ωᵢ, ωₒ) Li(ωᵢ) cosθᵢ / pdf(ωᵢ)
```

**Where:**
- **N:** Number of samples
- **pdf(ωᵢ):** Probability density function (how likely we are to pick direction ωᵢ)

**The Key Insight:** If we pick directions randomly but weight them properly, we can get a good approximation with just a few samples.

### Types of BRDFs

**1. Lambertian (Perfect Diffuse):**
```
fr = albedo / π
```
- Reflects light equally in all directions
- Appears equally bright from any viewing angle
- Examples: chalk, unfinished wood, matte paint

**2. Perfect Mirror (Specular):**
```
fr = δ(ωᵢ - reflect(ωₒ))
```
- Reflects light only in the mirror direction
- δ is the Dirac delta function (infinite at one point, zero everywhere else)
- Examples: polished metal, still water

**3. Microfacet Models (Realistic):**
- Surfaces are made of tiny mirrors (microfacets) at different orientations
- Combines diffuse and specular reflection
- Examples: most real-world materials

### Microfacet Theory - The Foundation of Modern PBR

**The Big Idea:** Real surfaces aren't perfectly smooth. Under a microscope, they're covered with tiny bumps and valleys. Each tiny surface acts like a little mirror.

**Key Insight:** The overall reflection is the average of millions of tiny mirror reflections.

**Microfacet BRDF Structure:**
```
fr = kd · fLambert + ks · fmicrofacet
```

**Where:**
- **kd:** Diffuse weight (how much light penetrates the surface)
- **ks:** Specular weight (how much light reflects off the surface)
- **fLambert:** Lambertian diffuse term
- **fmicrofacet:** Microfacet specular term

**The Microfacet Specular Term:**
```
fmicrofacet = (D · F · G) / (4 · cosθᵢ · cosθₒ)
```

**The Three Functions:**
- **D:** Normal Distribution Function (what directions do the microfacets face?)
- **F:** Fresnel Function (how much light reflects vs. refracts?)
- **G:** Geometry Function (how much light is blocked by neighboring microfacets?)

This is the heart of modern PBR - we'll explore each function in detail next.

---

## Material Modeling - How Surfaces Work

### The Cook-Torrance BRDF - Industry Standard

The **Cook-Torrance model** is the most widely used BRDF in real-time rendering. It was developed by Robert Cook and Kenneth Torrance in 1982.

**Complete Cook-Torrance BRDF:**
```
fr = kd · (albedo/π) + ks · (D·F·G)/(4·cosθᵢ·cosθₒ)
```

**Two Parts:**
1. **Diffuse Term:** `kd · (albedo/π)` - Light that penetrates the surface and scatters
2. **Specular Term:** `ks · (D·F·G)/(4·cosθᵢ·cosθₒ)` - Light that reflects off the surface

### The D Term - Normal Distribution Function (NDF)

**What D Represents:**
The D function describes the distribution of microfacet orientations on the surface.

**Physical Meaning:**
- **Smooth surface:** Most microfacets point in the same direction (sharp peak)
- **Rough surface:** Microfacets point in many directions (wide distribution)

**GGX/Trowbridge-Reitz Distribution (Most Common):**
```
D(h) = α² / (π · ((cosθh)² · (α² - 1) + 1)²)
```

**Understanding the Variables:**
- **h:** Half-vector between light and view directions: `h = normalize(l + v)`
- **θh:** Angle between half-vector and surface normal
- **α:** Roughness parameter: `α = roughness²`

**Why the Half-Vector?**
Only microfacets oriented along the half-vector can reflect light from the light source to the viewer. The D function tells us how many such microfacets exist.

**Roughness Mapping:**
```
α = roughness²
```
This quadratic mapping gives more intuitive control:
- **roughness = 0:** Perfect mirror (α = 0)
- **roughness = 0.5:** Moderately rough (α = 0.25)
- **roughness = 1:** Very rough (α = 1)

**Alternative NDF Models:**

**Beckmann Distribution:**
```
D(h) = exp(-tan²θh/α²) / (π · α² · cos⁴θh)
```
- More physically accurate for some materials
- More expensive to compute
- Sharper falloff than GGX

**Phong Distribution (Legacy):**
```
D(h) = ((n+2)/(2π)) · cos^n θh
```
- Simple but not physically based
- Still used in some older engines

### The F Term - Fresnel Function

**What F Represents:**
The Fresnel function describes how reflectance changes with viewing angle.

**Key Observations:**
- Looking straight down at water: you see through it (low reflection)
- Looking across a lake: it acts like a mirror (high reflection)

**Schlick's Approximation (Standard in Real-Time):**
```
F(θ) = F₀ + (1 - F₀)(1 - cosθ)⁵
```

**Understanding F₀ (F-naught):**
F₀ is the reflectance at normal incidence (looking straight down).

**Calculating F₀ for Dielectrics:**
```
F₀ = ((n - 1) / (n + 1))²
```

**Common F₀ Values:**
- **Water:** 0.02 (2%)
- **Plastic:** 0.04 (4%)
- **Glass:** 0.04-0.08 (4-8%)
- **Skin:** 0.028 (2.8%)
- **Fabric:** 0.04-0.06 (4-6%)

**F₀ for Metals:**
Metals have much higher and colored F₀ values:
- **Iron:** (0.56, 0.57, 0.58)
- **Copper:** (0.95, 0.64, 0.54)
- **Gold:** (1.00, 0.71, 0.29)
- **Silver:** (0.95, 0.93, 0.88)

**Why Metals are Different:**
- Metals have free electrons that interact strongly with light
- High reflectance even at normal incidence
- Colored reflection (different reflectance for different wavelengths)

### The G Term - Geometry Function

**What G Represents:**
The geometry function accounts for **masking** and **shadowing** of microfacets.

**Two Types of Occlusion:**
1. **Masking:** Microfacets hidden from the viewer
2. **Shadowing:** Microfacets hidden from the light source

**Physical Intuition:**
Imagine a rough surface under a microscope. Some tiny bumps cast shadows on other bumps, and some bumps hide behind others when viewed from certain angles.

**Smith G Function (Industry Standard):**
```
G(l, v, h) = G₁(l) · G₁(v)
```

**Where G₁ is:**
```
G₁(v) = 2 / (1 + √(1 + α² · tan²θv))
```

**Height-Correlated Smith G (More Accurate):**
```
G(l, v, h) = 1 / (1 + Λ(l) + Λ(v))
```

**Where Λ (Lambda) is:**
```
Λ(v) = (-1 + √(1 + α² · tan²θv)) / 2
```

**Practical Difference:**
- **Uncorrelated:** Assumes masking and shadowing are independent
- **Height-correlated:** More physically accurate, accounts for correlation
- **Performance:** Height-correlated is slightly more expensive but more accurate

### Material Parameters in PBR

**The PBR Material Model uses these parameters:**

**1. Base Color (Albedo)**
- **For Metals:** Becomes the F₀ value (specular color)
- **For Non-metals:** Becomes the diffuse color
- **Range:** [0, 1] for each RGB channel
- **Physical Meaning:** The intrinsic color of the material

**2. Metallic**
- **Range:** [0, 1] (often just 0 or 1)
- **0:** Dielectric (non-metal)
- **1:** Conductor (metal)
- **In-between:** Rare, used for oxidized metals or special effects

**3. Roughness**
- **Range:** [0, 1]
- **0:** Perfect mirror
- **1:** Completely rough (Lambertian-like)
- **Physical Meaning:** RMS slope of microfacets

**4. Normal Map**
- **Purpose:** Adds surface detail without geometry
- **Format:** Tangent-space normal vectors
- **Effect:** Changes the surface normal used in lighting calculations

**5. Additional Maps (Optional)**
- **Ambient Occlusion:** Pre-computed shadowing in crevices
- **Height/Displacement:** Actual geometry modification
- **Subsurface Scattering:** For materials like skin, wax, marble

### The Metallic Workflow

**Key Insight:** The metallic parameter controls how the base color is interpreted.

**For Dielectrics (metallic = 0):**
```
diffuse_color = base_color
specular_color = F₀ (typically 0.04)
```

**For Metals (metallic = 1):**
```
diffuse_color = black (0, 0, 0)
specular_color = base_color
```

**Mixed Materials (0 < metallic < 1):**
```
diffuse_color = base_color * (1 - metallic)
specular_color = lerp(F₀, base_color, metallic)
```

**Why This Works:**
- Metals don't have diffuse reflection (light doesn't penetrate)
- Non-metals have colorless specular reflection
- This parameterization matches physical reality

### Energy Conservation Implementation

**The Fundamental Rule:**
```
kd + ks ≤ 1
```

**In Practice:**
```
ks = F  // Fresnel determines specular contribution
kd = (1 - F) * (1 - metallic)  // Remaining energy goes to diffuse
```

**Why (1 - metallic)?**
- Metals have no diffuse reflection
- The metallic parameter linearly interpolates between metal and non-metal behavior

**Complete Energy Balance:**
```
total_reflection = kd * diffuse_brdf + ks * specular_brdf
where:
  kd = (1 - F) * (1 - metallic)
  ks = F
  kd + ks ≤ 1 (automatically satisfied)
```

---

## Lighting Models - Direct and Indirect

### Understanding Light Sources

**Direct Lighting:** Light that travels directly from a light source to the surface to your eye.
**Indirect Lighting:** Light that bounces off other surfaces before reaching the surface you're looking at.

### Direct Lighting Models

**1. Point Lights**

**Physical Model:**
A point light emits light uniformly in all directions from a single point.

**Intensity Calculation:**
```
Li = (Φ · cosθ) / (4π · r²)
```

**Breaking it down:**
- **Φ (Phi):** Total luminous flux (total light power) in lumens
- **4π:** Surface area of a unit sphere (light spreads in all directions)
- **r²:** Distance squared (inverse square law)
- **cosθ:** Lambert's cosine law (surface orientation)

**Why r²?**
Light spreads out as it travels. At distance r, the same amount of light is spread over a sphere of area 4πr². Double the distance = quarter the intensity.

**Practical Implementation:**
```
float distance = length(light_position - surface_position);
float attenuation = 1.0 / (distance * distance);
vec3 light_intensity = light_color * light_power * attenuation;
```

**2. Directional Lights (Sun)**

**Physical Model:**
Light from an infinitely distant source (like the sun). All rays are parallel.

**Intensity Calculation:**
```
Li = E · cosθ
```

**Where:**
- **E:** Illuminance (light per unit area) in lux
- **cosθ:** Lambert's cosine law

**Why No Distance Falloff?**
The sun is so far away that the distance variation across a scene is negligible.

**Practical Implementation:**
```
float NdotL = max(dot(surface_normal, light_direction), 0.0);
vec3 light_intensity = light_color * light_illuminance * NdotL;
```

**3. Area Lights**

**Physical Model:**
Light emitted from a surface (like a fluorescent panel or window).

**Intensity Calculation (Integral):**
```
Li = ∫A (L · cosθ · cosθ') / (π · r²) dA
```

**Breaking it down:**
- **L:** Radiance of the light surface
- **θ:** Angle at the receiving surface
- **θ':** Angle at the light surface
- **r:** Distance between surface points
- **dA:** Differential area element

**Why This is Hard:**
We need to integrate over the entire light surface. This is expensive for real-time rendering.

**Approximation Methods:**
- **Representative Point:** Treat as point light at center
- **Linearly Transformed Cosines (LTC):** Efficient area light approximation
- **Spherical Gaussians:** Approximate area lights as Gaussian lobes

### Indirect Lighting - The Global Illumination Problem

**The Challenge:**
In the real world, light bounces everywhere. A red wall makes nearby objects appear slightly red. This is **global illumination**.

**Why It's Hard:**
Every surface receives light from every other surface. This creates a system of millions of interdependent equations.

### Image-Based Lighting (IBL)

**The Idea:**
Instead of calculating all light bounces, capture the lighting environment in a panoramic image (HDR environment map).

**Environment Map Types:**

**1. Equirectangular Maps**
- Spherical panorama mapped to rectangle
- Longitude/latitude mapping
- Common format for HDR environments

**2. Cube Maps**
- Six square faces of a cube
- More uniform sampling
- Hardware-friendly format

**HDR vs LDR:**
- **HDR:** High Dynamic Range - can represent very bright values
- **LDR:** Low Dynamic Range - limited to [0,1] range
- **Why HDR Matters:** Real sunlight is 100,000x brighter than indoor lighting

### Diffuse IBL - Irradiance Environment Maps

**The Problem:**
For diffuse reflection, we need to integrate the environment map over the entire hemisphere.

**Diffuse Integral:**
```
Ld = ∫Ω Li(ωᵢ) cosθᵢ dωᵢ / π
```

**Solution - Precomputation:**
1. For each direction, integrate the environment map
2. Store results in an **irradiance environment map**
3. At runtime, just look up the precomputed value

**Spherical Harmonics Alternative:**
Represent the environment lighting using spherical harmonic coefficients:
```
L(θ, φ) = Σₗ Σₘ cₗᵐ · Yₗᵐ(θ, φ)
```

**Advantages:**
- Very compact (9 coefficients for reasonable quality)
- Fast evaluation
- Easy to rotate and blend

### Specular IBL - The Split-Sum Approximation

**The Problem:**
For specular reflection, we need to evaluate:
```
∫Ω Li(ωᵢ) fr(ωᵢ, ωₒ) cosθᵢ dωᵢ
```

This depends on both the environment AND the BRDF - can't precompute easily.

**Epic Games' Solution - Split-Sum Approximation:**
```
∫Ω Li(ωᵢ) fr(ωᵢ, ωₒ) cosθᵢ dωᵢ ≈ 
(∫Ω Li(ωᵢ) dωᵢ) · (∫Ω fr(ωᵢ, ωₒ) cosθᵢ dωᵢ)
```

**Two Precomputed Terms:**

**1. Pre-filtered Environment Map:**
- Convolve environment map with BRDF lobe
- Different roughness levels → different mip levels
- Rougher surfaces sample wider area

**2. BRDF Integration Map (LUT):**
- 2D texture: roughness vs. viewing angle
- Stores scale and bias for Fresnel term
- Independent of environment

**Runtime Calculation:**
```
vec2 brdf_lut = texture(brdf_lut_texture, vec2(NdotV, roughness)).xy;
vec3 prefiltered = textureLod(prefiltered_env, reflection_vector, roughness * max_mip_level).rgb;
vec3 specular = prefiltered * (F0 * brdf_lut.x + brdf_lut.y);
```

### Multiple Scattering

**The Problem:**
Standard microfacet models only account for single scattering - light bouncing once off microfacets.

**Real surfaces have multiple scattering:**
- Light bounces between microfacets multiple times
- Especially important for rough surfaces
- Causes energy loss in standard models

**Energy Compensation:**
```
fms = (1 - Ess) · (1 - Ems) / (π · (1 - Ems))
```

**Where:**
- **Ess:** Energy reflected by single scattering
- **Ems:** Average energy reflected over all viewing angles

**Practical Implementation:**
- Precompute energy loss tables
- Add compensation term to BRDF
- Maintains energy conservation

---

## Energy Conservation - The Fundamental Law

### Why Energy Conservation Matters

**The Physical Law:**
In the real world, a surface cannot reflect more light than it receives. This seems obvious, but many older rendering techniques violated this rule.

**Common Violations in Non-PBR:**
- Adding ambient + diffuse + specular without limits
- Specular highlights that are too bright
- Materials that "glow" without being light sources

**Visual Consequences of Violations:**
- Materials look "plastic" or "fake"
- Inconsistent appearance under different lighting
- Scenes that are too bright or have wrong color balance

### Mathematical Formulation

**Energy Conservation Constraint:**
```
∫Ω fr(ωᵢ, ωₒ) cosθᵢ dωᵢ ≤ 1
```

**In Plain English:**
"The total amount of light reflected in all directions cannot exceed the amount of light received."

### Implementing Energy Conservation in PBR

**The Diffuse-Specular Balance:**
```
total_reflection = kd · diffuse_term + ks · specular_term
where: kd + ks ≤ 1
```

**Automatic Energy Conservation:**
```
F = fresnel_schlick(NdotV, F0);  // Specular contribution
ks = F;                          // Specular weight
kd = (1.0 - F) * (1.0 - metallic); // Diffuse weight
```

**Why This Works:**
- Fresnel determines how much light reflects vs. refracts
- Reflected light becomes specular
- Refracted light becomes diffuse (for non-metals)
- Metals have no diffuse component

### Handling Edge Cases

**Numerical Stability:**
```
// Prevent division by zero
float denominator = 4.0 * NdotL * NdotV;
denominator = max(denominator, 0.001);

// Prevent roughness from being too small
float roughness = max(material_roughness, 0.045);
```

**Why These Limits?**
- Very small denominators cause numerical instability
- Very smooth surfaces (roughness < 0.045) can cause NaN values
- These limits are imperceptible visually but prevent crashes

---

## DirectX 12 Implementation Theory

### Shader Pipeline Architecture

**Vertex Shader Responsibilities:**
```hlsl
// Transform vertices to clip space
output.position = mul(input.position, world_view_proj_matrix);

// Transform normals to world space
output.world_normal = mul(input.normal, (float3x3)world_matrix);

// Transform tangents for normal mapping
output.world_tangent = mul(input.tangent, (float3x3)world_matrix);

// Pass through texture coordinates
output.texcoord = input.texcoord;

// Calculate world position for lighting
output.world_position = mul(input.position, world_matrix).xyz;
```

**Pixel Shader Structure:**
```hlsl
float4 PBR_PixelShader(VertexOutput input) : SV_Target
{
    // Sample material properties
    float3 albedo = SampleAlbedo(input.texcoord);
    float metallic = SampleMetallic(input.texcoord);
    float roughness = SampleRoughness(input.texcoord);
    float3 normal = SampleAndTransformNormal(input);
    
    // Calculate lighting vectors
    float3 V = normalize(camera_position - input.world_position);
    float3 N = normalize(normal);
    
    // Calculate PBR lighting
    float3 color = CalculatePBRLighting(albedo, metallic, roughness, N, V, input.world_position);
    
    // Apply tone mapping and gamma correction
    color = ToneMap(color);
    color = LinearToSRGB(color);
    
    return float4(color, 1.0);
}
```

### Texture Management in DirectX 12

**Material Texture Layout:**
```
Texture2D albedo_texture     : register(t0);
Texture2D normal_texture     : register(t1);
Texture2D metallic_texture   : register(t2);
Texture2D roughness_texture  : register(t3);
Texture2D ao_texture         : register(t4);
```

**Channel Packing Optimization:**
```
// Pack multiple properties into single texture
// R: Metallic, G: Roughness, B: AO, A: Height
Texture2D material_properties : register(t1);

float4 props = material_properties.Sample(sampler, uv);
float metallic = props.r;
float roughness = props.g;
float ao = props.b;
float height = props.a;
```

**sRGB Handling:**
```hlsl
// Albedo textures are in sRGB space
Texture2D<float4> albedo_texture : register(t0);
SamplerState srgb_sampler : register(s0);

// Hardware automatically converts sRGB to linear
float3 albedo = albedo_texture.Sample(srgb_sampler, uv).rgb;

// OR manual conversion:
float3 albedo_srgb = albedo_texture.Sample(linear_sampler, uv).rgb;
float3 albedo = pow(albedo_srgb, 2.2); // Approximate sRGB to linear
```

### Normal Mapping Implementation

**Tangent Space to World Space:**
```hlsl
float3 SampleAndTransformNormal(VertexOutput input)
{
    // Sample normal map (tangent space)
    float3 tangent_normal = normal_texture.Sample(sampler, input.texcoord).xyz;
    tangent_normal = tangent_normal * 2.0 - 1.0; // [0,1] to [-1,1]
    
    // Build TBN matrix
    float3 N = normalize(input.world_normal);
    float3 T = normalize(input.world_tangent);
    float3 B = cross(N, T);
    float3x3 TBN = float3x3(T, B, N);
    
    // Transform to world space
    return normalize(mul(tangent_normal, TBN));
}
```

### IBL Implementation in DirectX 12

**Environment Map Setup:**
```hlsl
TextureCube environment_map : register(t5);
TextureCube irradiance_map : register(t6);
TextureCube prefiltered_map : register(t7);
Texture2D brdf_lut : register(t8);
```

**Diffuse IBL:**
```hlsl
float3 CalculateDiffuseIBL(float3 normal)
{
    return irradiance_map.Sample(sampler, normal).rgb;
}
```

**Specular IBL:**
```hlsl
float3 CalculateSpecularIBL(float3 reflection_vector, float roughness, float NdotV, float3 F0)
{
    // Sample prefiltered environment
    float mip_level = roughness * MAX_MIP_LEVEL;
    float3 prefiltered = prefiltered_map.SampleLevel(sampler, reflection_vector, mip_level).rgb;
    
    // Sample BRDF LUT
    float2 brdf_lut_coords = float2(NdotV, roughness);
    float2 brdf = brdf_lut.Sample(sampler, brdf_lut_coords).xy;
    
    // Combine
    return prefiltered * (F0 * brdf.x + brdf.y);
}
```

### Performance Optimization Strategies

**1. Shader Optimization:**
```hlsl
// Use built-in functions when possible
float NdotV = saturate(dot(N, V)); // saturate() is faster than max(x, 0)

// Avoid expensive operations in loops
float roughness_squared = roughness * roughness; // Calculate once, use multiple times

// Use mad() instruction pattern
float result = a * b + c; // Compiles to single mad instruction
```

**2. Texture Streaming:**
- Use DirectX 12's tiled resources for large texture atlases
- Implement mip-level streaming based on distance
- Use BC7 compression for albedo textures
- Use BC5 compression for normal maps

**3. Constant Buffer Organization:**
```hlsl
cbuffer PerFrameConstants : register(b0)
{
    float4x4 view_matrix;
    float4x4 projection_matrix;
    float3 camera_position;
    float time;
};

cbuffer PerObjectConstants : register(b1)
{
    float4x4 world_matrix;
    float4x4 world_inverse_transpose;
    float metallic_factor;
    float roughness_factor;
};
```

### Multi-Threading Considerations

**Command List Recording:**
- Record PBR rendering commands on multiple threads
- Use separate command lists for different material types
- Bundle frequently used state changes

**Resource Barriers:**
```cpp
// Transition textures to shader resource state
D3D12_RESOURCE_BARRIER barrier = {};
barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
barrier.Transition.pResource = texture_resource;
barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
command_list->ResourceBarrier(1, &barrier);
```

---

## Mathematical Optimizations

### Numerical Stability Techniques

**1. Avoiding Division by Zero:**
```hlsl
// Instead of: result = numerator / denominator;
float safe_denominator = max(denominator, 1e-6);
float result = numerator / safe_denominator;

// Or use a small epsilon
const float EPSILON = 0.001;
float result = numerator / (denominator + EPSILON);
```

**2. Clamping Input Values:**
```hlsl
// Prevent extreme roughness values
float roughness = clamp(input_roughness, 0.045, 1.0);

// Prevent extreme dot products
float NdotV = max(dot(N, V), 1e-6);
float NdotL = max(dot(N, L), 0.0);
```

**3. Handling Grazing Angles:**
```hlsl
// At grazing angles, numerical precision becomes critical
float NdotV = dot(N, V);
if (NdotV < 0.0) {
    // Flip normal for back-facing surfaces
    N = -N;
    NdotV = -NdotV;
}
NdotV = max(NdotV, 1e-6);
```

### Fast Approximations

**1. Fast Inverse Square Root:**
```hlsl
// Modern GPUs have hardware rsqrt instruction
float fast_inverse_sqrt(float x) {
    return rsqrt(x); // Single instruction on GPU
}

// For normalization:
float3 fast_normalize(float3 v) {
    return v * rsqrt(dot(v, v));
}
```

**2. Polynomial Approximations:**
```hlsl
// Fast pow(x, 5) for Schlick Fresnel
float pow5(float x) {
    float x2 = x * x;
    return x2 * x2 * x; // 3 multiplications instead of pow()
}

// Schlick Fresnel approximation
float3 fresnel_schlick_fast(float cosTheta, float3 F0) {
    return F0 + (1.0 - F0) * pow5(1.0 - cosTheta);
}
```

**3. Trigonometric Approximations:**
```hlsl
// Fast acos approximation for environment mapping
float fast_acos(float x) {
    float x_abs = abs(x);
    float result = -0.0187293 * x_abs;
    result = result + 0.0742610;
    result = result * x_abs;
    result = result - 0.2121144;
    result = result * x_abs;
    result = result + 1.5707288;
    result = result * sqrt(1.0 - x_abs);
    return x < 0 ? 3.14159265 - result : result;
}
```

### SIMD Optimization Opportunities

**1. Vectorized Light Calculations:**
```hlsl
// Process multiple lights simultaneously
float4 light_distances = float4(dist1, dist2, dist3, dist4);
float4 attenuations = 1.0 / (light_distances * light_distances);
```

**2. Parallel Material Evaluation:**
```hlsl
// Evaluate BRDF for multiple samples at once
float4 roughness_values = float4(r1, r2, r3, r4);
float4 ndf_values = GGX_Distribution_x4(roughness_values, cos_theta_h);
```

### Memory Access Optimization

**1. Texture Cache Efficiency:**
```hlsl
// Group related texture samples
float4 material_props = material_texture.Sample(sampler, uv);
float metallic = material_props.r;
float roughness = material_props.g;
float ao = material_props.b;

// Instead of separate texture samples for each property
```

**2. Constant Buffer Layout:**
```hlsl
// Align data to 16-byte boundaries for optimal GPU access
cbuffer MaterialConstants : register(b2)
{
    float4 base_color_and_metallic;  // xyz = color, w = metallic
    float4 roughness_and_padding;    // x = roughness, yzw = padding
    float4x4 texture_transform;      // 64 bytes, naturally aligned
};
```

---

## Practical Application Guidelines

### Material Authoring Best Practices

**1. Physically Plausible Values:**

**Albedo Guidelines:**
- **Metals:** Very dark (0.02-0.05) - metals get their color from specular reflection
- **Non-metals:** Brighter (0.04-0.9) - but avoid pure white
- **Charcoal:** ~0.02
- **Fresh snow:** ~0.9
- **Human skin:** ~0.3-0.5

**Roughness Guidelines:**
- **Mirror:** 0.0
- **Polished metal:** 0.1-0.3
- **Worn metal:** 0.4-0.7
- **Concrete:** 0.8-0.9
- **Fabric:** 0.9-1.0

**2. Metallic Values:**
```
0.0: Pure dielectric (plastic, wood, skin, etc.)
1.0: Pure metal (iron, gold, silver, etc.)
0.5-0.8: Oxidized metals, special effects only
```

**3. F₀ Reference Values:**
```hlsl
// Common dielectric F0 values
const float3 F0_WATER = float3(0.02, 0.02, 0.02);
const float3 F0_PLASTIC = float3(0.04, 0.04, 0.04);
const float3 F0_GLASS = float3(0.04, 0.04, 0.04);
const float3 F0_DIAMOND = float3(0.17, 0.17, 0.17);

// Common metal F0 values
const float3 F0_IRON = float3(0.56, 0.57, 0.58);
const float3 F0_COPPER = float3(0.95, 0.64, 0.54);
const float3 F0_GOLD = float3(1.00, 0.71, 0.29);
const float3 F0_ALUMINUM = float3(0.91, 0.92, 0.92);
const float3 F0_SILVER = float3(0.95, 0.93, 0.88);
```

### Debugging and Validation

**1. Energy Conservation Checks:**
```hlsl
// Visualize energy conservation
float3 debug_energy_conservation(float3 albedo, float metallic, float roughness, float NdotV)
{
    float3 F0 = lerp(0.04, albedo, metallic);
    float3 F = fresnel_schlick(NdotV, F0);
    
    float kd = (1.0 - F) * (1.0 - metallic);
    float ks = F;
    
    float total_energy = kd + ks;
    
    // Should never exceed 1.0
    return total_energy > 1.0 ? float3(1, 0, 0) : float3(0, 1, 0);
}
```

**2. BRDF Validation:**
```hlsl
// Visualize individual BRDF components
float3 debug_brdf_components(int debug_mode, ...)
{
    switch(debug_mode)
    {
        case 0: return albedo; // Base color
        case 1: return float3(metallic, metallic, metallic); // Metallic
        case 2: return float3(roughness, roughness, roughness); // Roughness
        case 3: return normal * 0.5 + 0.5; // Normal map
        case 4: return float3(NdotV, NdotV, NdotV); // View angle
        case 5: return F; // Fresnel
        case 6: return float3(D, D, D); // Normal distribution
        case 7: return float3(G, G, G); // Geometry function
    }
}
```

### Performance Profiling

**1. GPU Timing:**
```cpp
// DirectX 12 GPU timing
ID3D12QueryHeap* timestamp_heap;
ID3D12Resource* timestamp_buffer;

// Begin timing
command_list->EndQuery(timestamp_heap, D3D12_QUERY_TYPE_TIMESTAMP, 0);

// PBR rendering commands here...

// End timing
command_list->EndQuery(timestamp_heap, D3D12_QUERY_TYPE_TIMESTAMP, 1);
```

**2. Shader Profiling:**
- Use PIX for Windows to analyze shader performance
- Profile different roughness values (smooth surfaces are more expensive)
- Test with various light counts
- Measure IBL vs. analytical lighting performance

### Common Pitfalls and Solutions

**1. Incorrect sRGB Handling:**
```hlsl
// WRONG: Treating sRGB as linear
float3 albedo = albedo_texture.Sample(linear_sampler, uv).rgb;

// CORRECT: Use sRGB sampler or manual conversion
float3 albedo = albedo_texture.Sample(srgb_sampler, uv).rgb;
// OR
float3 albedo_srgb = albedo_texture.Sample(linear_sampler, uv).rgb;
float3 albedo = pow(albedo_srgb, 2.2);
```

**2. Normal Map Artifacts:**
```hlsl
// WRONG: Not normalizing after interpolation
float3 normal = input.world_normal;

// CORRECT: Always normalize interpolated normals
float3 normal = normalize(input.world_normal);
```

**3. Energy Non-Conservation:**
```hlsl
// WRONG: Adding terms without energy balance
float3 final_color = ambient + diffuse + specular;

// CORRECT: Proper energy balance
float3 kd = (1.0 - F) * (1.0 - metallic);
float3 final_color = kd * diffuse + F * specular;
```

### Integration with Game Engines

**1. Material System Design:**
```cpp
struct PBRMaterial {
    Texture2D* albedo_map;
    Texture2D* normal_map;
    Texture2D* material_properties; // R=metallic, G=roughness, B=AO
    
    Vector3 base_color_factor = {1.0f, 1.0f, 1.0f};
    float metallic_factor = 0.0f;
    float roughness_factor = 1.0f;
    
    bool double_sided = false;
    AlphaMode alpha_mode = AlphaMode::Opaque;
};
```

**2. Lighting System Integration:**
```cpp
class PBRRenderer {
    void RenderPBRMaterials(const std::vector<RenderObject>& objects);
    void SetupIBLEnvironment(const HDRTexture& environment);
    void PrecomputeIBLData(); // Generate irradiance and prefiltered maps
    void UpdateLightingUniforms(const LightingEnvironment& lights);
};
```

---

## Conclusion and Further Learning

### Key Takeaways

**1. Physics First:** PBR works because it's based on real physics, not arbitrary artistic choices.

**2. Energy Conservation:** The fundamental rule that makes everything work correctly.

**3. Material Parameterization:** Metallic workflow provides intuitive, physically-based controls.

**4. Implementation Details Matter:** Proper sRGB handling, numerical stability, and optimization are crucial.

### Recommended Next Steps

**1. Implement Basic PBR:**
- Start with Cook-Torrance BRDF
- Add simple directional lighting
- Implement proper sRGB handling

**2. Add IBL:**
- Implement diffuse IBL with irradiance maps
- Add specular IBL with split-sum approximation
- Generate precomputed environment data

**3. Optimize:**
- Profile your implementation
- Add fast approximations where appropriate
- Implement proper LOD systems

**4. Advanced Features:**
- Multiple scattering compensation
- Subsurface scattering for skin/wax
- Clearcoat for car paint
- Cloth shading models

### Essential References

**Academic Papers:**
- "Microfacet Models for Refraction through Rough Surfaces" (Walter et al., 2007)
- "Real Shading in Unreal Engine 4" (Karis, 2013)
- "Moving Frostbite to Physically Based Rendering" (Lagarde & de Rousiers, 2014)

**Books:**
- "Physically Based Rendering: From Theory to Implementation" (Pharr, Jakob, Humphreys)
- "Real-Time Rendering" (Akenine-Möller, Haines, Hoffman)

**Online Resources:**
- Substance Academy (material authoring)
- Learn OpenGL PBR tutorial
- Filament documentation (Google's PBR implementation)

This guide provides the theoretical foundation you need to understand and implement PBR in your DirectX 12 projects. Remember: start simple, validate your implementation against known references, and build complexity gradually.
