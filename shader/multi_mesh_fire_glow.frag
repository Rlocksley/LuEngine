#version 460

// =============================================================================
// multi_mesh_fire_glow.frag   (light halo layer)
//
// A handful of huge, very faint additive squares sitting inside the flame. They
// fake the bloom / light scattering of the fire in the surrounding air, and
// flicker randomly with the fire clock.
// =============================================================================

layout(location = 0) in vec2 inUV;
layout(location = 1) flat in vec4 inState;       // age01, life, seed, clock
layout(location = 2) flat in vec4 inLook;        // brightness, lean, nearFade, screen speed
layout(location = 3) flat in vec3 inTint;
layout(location = 4) in vec3 inLocalPos;
layout(location = 5) flat in vec3 inLocalCenter;

layout(location = 0) out vec4 outColor;

// ---- shared fire helpers (noise, SDF, palette) --------------------------------
uint fireHash(uint x) {
    uint state = x * 747796405u + 2891336453u;
    uint word = ((state >> ((state >> 28u) + 4u)) ^ state) * 277803737u;
    return (word >> 22u) ^ word;
}

// Uniform random float in [0,1). Advances the state.
float fireRand(inout uint s) {
    s = fireHash(s);
    return float(s) * (1.0 / 4294967296.0);
}

// Sin-free float hash for lattice noise (Dave Hoskins).
float hash13(vec3 p3) {
    p3 = fract(p3 * 0.1031);
    p3 += dot(p3, p3.zyx + 31.32);
    return fract((p3.x + p3.y) * p3.z);
}

// 3D value noise, quintic interpolation. Output in [0,1].
float noise3(vec3 x) {
    vec3 i = floor(x);
    vec3 f = fract(x);
    f = f * f * f * (f * (f * 6.0 - 15.0) + 10.0);

    float n000 = hash13(i);
    float n100 = hash13(i + vec3(1.0, 0.0, 0.0));
    float n010 = hash13(i + vec3(0.0, 1.0, 0.0));
    float n110 = hash13(i + vec3(1.0, 1.0, 0.0));
    float n001 = hash13(i + vec3(0.0, 0.0, 1.0));
    float n101 = hash13(i + vec3(1.0, 0.0, 1.0));
    float n011 = hash13(i + vec3(0.0, 1.0, 1.0));
    float n111 = hash13(i + vec3(1.0, 1.0, 1.0));

    return mix(mix(mix(n000, n100, f.x), mix(n010, n110, f.x), f.y),
               mix(mix(n001, n101, f.x), mix(n011, n111, f.x), f.y), f.z);
}

// Fractal Brownian motion, `octaves` in 1..5. Output ~[0,1].
float fbm3(vec3 p, int octaves) {
    const mat3 rot = mat3(0.00, 0.80, 0.60,
                         -0.80, 0.36, -0.48,
                         -0.60, -0.48, 0.64);
    float sum = 0.0;
    float amp = 0.5;
    float norm = 0.0;
    for (int i = 0; i < octaves; ++i) {
        sum += amp * noise3(p);
        norm += amp;
        p = rot * p * 2.02 + vec3(1.7, 9.2, 3.1);
        amp *= 0.5;
    }
    return sum / norm;
}

// Divergence-free turbulence (curl of a noise potential). Magnitude ~ 0..1.5.
// 12 noise lookups; used by the compute shader only.
vec3 curlNoise(vec3 p) {
    const float e = 0.12;
    const vec3 o1 = vec3(31.4, 17.7, 5.3);
    const vec3 o2 = vec3(11.9, 47.1, 23.7);
    const vec3 dx = vec3(e, 0.0, 0.0);
    const vec3 dy = vec3(0.0, e, 0.0);
    const vec3 dz = vec3(0.0, 0.0, e);

    // potential psi = (n(p), n(p+o1), n(p+o2))
    float dPsiZ_dy = noise3(p + o2 + dy) - noise3(p + o2 - dy);
    float dPsiY_dz = noise3(p + o1 + dz) - noise3(p + o1 - dz);
    float dPsiX_dz = noise3(p + dz) - noise3(p - dz);
    float dPsiZ_dx = noise3(p + o2 + dx) - noise3(p + o2 - dx);
    float dPsiY_dx = noise3(p + o1 + dx) - noise3(p + o1 - dx);
    float dPsiX_dy = noise3(p + dy) - noise3(p - dy);

    return vec3(dPsiZ_dy - dPsiY_dz,
                dPsiX_dz - dPsiZ_dx,
                dPsiY_dx - dPsiX_dy) / (2.0 * e);
}

// ---------------------------------------------------------------------------
// SDF helpers
// ---------------------------------------------------------------------------

// Polynomial smooth minimum (smooth union of two distance fields).
float smin(float a, float b, float k) {
    float h = max(k - abs(a - b), 0.0) / k;
    return min(a, b) - h * h * k * 0.25;
}

float sdCircle(vec2 p, vec2 c, float r) {
    return length(p - c) - r;
}

// Capsule between (0,-h) and (0,+h), radius r.
float sdVerticalCapsule(vec2 p, float h, float r) {
    p.y -= clamp(p.y, -h, h);
    return length(p) - r;
}

// ---------------------------------------------------------------------------
// Colour (all values are LINEAR, the swapchain is sRGB)
// ---------------------------------------------------------------------------

// Black-body-ish flame ramp. t = 0 cold/dark, 1 = hot yellow-white, >1 = white.
vec3 fireRamp(float t) {
    const vec3 c0 = vec3(0.00, 0.00, 0.00);
    const vec3 c1 = vec3(0.30, 0.012, 0.0);   // deep ember red
    const vec3 c2 = vec3(0.90, 0.10, 0.008);  // red-orange
    const vec3 c3 = vec3(1.00, 0.34, 0.02);   // orange
    const vec3 c4 = vec3(1.00, 0.62, 0.10);   // yellow-orange
    const vec3 c5 = vec3(1.00, 0.86, 0.42);   // yellow
    const vec3 c6 = vec3(1.00, 0.97, 0.80);   // white-hot
    vec3 c = mix(c0, c1, smoothstep(0.00, 0.18, t));
    c = mix(c, c2, smoothstep(0.18, 0.38, t));
    c = mix(c, c3, smoothstep(0.38, 0.58, t));
    c = mix(c, c4, smoothstep(0.58, 0.78, t));
    c = mix(c, c5, smoothstep(0.78, 0.98, t));
    c = mix(c, c6, smoothstep(0.98, 1.25, t));
    return c;
}


void main() {
    float seed = inState.z;
    float clock = inState.w;

    float r = length(inUV);
    float falloff = exp(-r * r * 5.0) * (1.0 - smoothstep(0.7, 1.0, r));

    float flicker = 0.55 + 0.9 * noise3(vec3(clock * 3.2 + seed * 50.0, seed * 7.0, 1.7));
    vec3 color = mix(vec3(1.0, 0.30, 0.045), vec3(1.0, 0.50, 0.12), noise3(vec3(clock * 1.1, seed * 9.0, 3.0)));

    outColor = vec4(color * falloff * flicker * inLook.x * inLook.z * 0.020, 0.0);
}
