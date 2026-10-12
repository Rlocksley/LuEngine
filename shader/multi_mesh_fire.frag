#version 460

// =============================================================================
// multi_mesh_fire.frag   (flame layer)
//
// Every billboard is shaded with signed distance fields. Two fields are combined:
//
//  1. PER SQUARE  (depends on uv + particle age):
//     a teardrop built from three smooth-unioned circles. Its tip stretches and
//     its silhouette is eroded more and more as the particle ages, so each
//     square lives through ignition -> licking tongue -> torn-off wisp.
//
//  2. WHOLE FIRE  (depends on the fragment position in fire-local space + clock):
//     a tapered flame volume (radius shrinks with height, tip height flickers
//     per angular sector). Every fragment evaluates it at its own 3D position,
//     so all the squares together are trimmed to ONE coherent flame shape.
//
// All noise is sampled at the fire-local 3D position and scrolled upwards with
// the shared fire clock, so neighbouring squares agree on the same flow field
// instead of each showing its own texture.
//
// Output is premultiplied: rgb = emitted light, a = soot / occlusion.
// Pipeline blend: src = ONE, dst = ONE_MINUS_SRC_ALPHA.
// =============================================================================

layout(location = 0) in vec2 inUV;
layout(location = 1) flat in vec4 inState;       // age01, life, seed, clock
layout(location = 2) flat in vec4 inLook;        // brightness, lean, nearFade, screen speed
layout(location = 3) flat in vec3 inTint;
layout(location = 4) in vec3 inLocalPos;         // fire-local fragment position
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


const float FLAME_HEIGHT = 1.9;    // nominal height of the flame volume
const float OPACITY      = 0.110;  // opacity of one fully covered square
const float OVER         = 0.85;   // 1 = pure over blending, 0 = pure additive
const float HOT_CORE     = 0.12;   // strength of the additive white-hot centre

// Teardrop: three circles of decreasing radius, smooth-unioned. a = particle age 0..1
float sdFlame(vec2 p, float a) {
    float stretch = 1.0 + 0.55 * a;
    vec2 q = vec2(p.x, p.y / stretch);
    float d1 = sdCircle(q, vec2(0.0, -0.38), 0.40);
    float d2 = sdCircle(q, vec2(0.0,  0.00), 0.28);
    float d3 = sdCircle(q, vec2(0.0,  0.34), 0.13);
    return smin(smin(d1, d2, 0.30), d3, 0.25);
}

void main() {
    vec2 uv = inUV;
    if (dot(uv, uv) > 1.0) discard;

    float a = inState.x;
    float clock = inState.w;
    float seed = inState.z;
    vec3 P = inLocalPos;
    float h = max(P.y, 0.0);

    // ---- shared flow field (fire-local, advected upwards) -------------------
    vec3 flow = vec3(P.x * 2.0, P.y * 1.4 - clock * 3.0, P.z * 2.0);
    vec2 warp = vec2(noise3(flow), noise3(flow + vec3(5.2, 1.3, 7.7))) - 0.5;

    // ---- 1. per-square SDF ---------------------------------------------------
    vec2 p = uv;
    p += warp * (0.30 + 0.55 * a);          // older particles are torn around more
    p.x += inLook.y * p.y * 0.35;           // lean into the direction of motion
    p.x *= mix(0.85, 1.25, fract(seed * 7.13));   // per particle width variation

    float detail = fbm3(vec3(P.x * 4.6, P.y * 3.0 - clock * 5.0, P.z * 4.6), 3);
    float erode = 0.38 + 0.60 * a;
    float d = sdFlame(p, a) - (detail - 0.5) * erode;

    float coverage = 1.0 - smoothstep(-0.02, 0.12, d);
    float core = smoothstep(0.0, -0.34, d);            // 1 deep inside the shape

    // ---- 2. whole fire volume SDF --------------------------------------------
    float rad = length(P.xz);
    float hn = clamp(h / FLAME_HEIGHT, 0.0, 1.0);
    float sector = atan(P.z, P.x);
    // per-sector tongue height: neighbouring angles get different tip heights => distinct tongues
    float tipNoise = noise3(vec3(cos(sector) * 2.6, sin(sector) * 2.6, clock * 2.2 + h * 0.4));
    float tipHeight = FLAME_HEIGHT * (0.45 + 0.95 * tipNoise);
    float R = mix(0.58, 0.05, pow(hn, 0.75)) + (detail - 0.5) * 0.34;
    float dVolume = rad - R;
    float envelope = 1.0 - smoothstep(-0.10, 0.22, dVolume);
    envelope *= 1.0 - smoothstep(0.72 * tipHeight, tipHeight, h);

    // ---- temperature ---------------------------------------------------------
    // Position dependent part (the same for every square): hottest on the axis
    // close to the base, cooling towards the rim and the tip of the flame volume.
    float axisHeat = (1.0 - smoothstep(0.0, max(R, 0.08), rad)) * (1.0 - smoothstep(0.0, 0.75 * FLAME_HEIGHT, h));
    // Particle part: cools with the age of the square, hotter in its own centre.
    float T = mix(0.62, 0.05, pow(a, 0.80)) + 0.12 * core;
    T += 0.50 * axisHeat;
    T *= mix(1.0, 0.50, smoothstep(0.15, FLAME_HEIGHT, h));
    // vertical streaks (noise stretched along y) read as flame tongues
    float streak = noise3(vec3(P.x * 5.5, P.y * 1.3 - clock * 6.5, P.z * 5.5));
    T += (detail - 0.5) * 0.30 + (streak - 0.5) * 0.70;
    T = clamp(T, 0.0, 1.3);

    // ---- light ---------------------------------------------------------------
    // Blending trick: the alpha written is almost equal to the emitted light
    // (premultiplied "over"), so thousands of overlapping squares converge to the
    // AVERAGE of their temperature colours instead of saturating additively into
    // a yellow-green / pink mess. The look therefore does not depend on how many
    // particles overlap. A small purely additive term adds the white-hot centre.
    float life = pow(1.0 - a, 0.55) * smoothstep(0.0, 0.25, a);   // ignite, burn, die
    float opacity = coverage * envelope * life * (0.55 + 0.9 * streak) * inLook.x * inLook.z * OPACITY;

    vec3 col = fireRamp(T) * inTint;
    vec3 whiteHot = vec3(1.0, 0.78, 0.42) * core * axisHeat * (1.0 - a);
    vec3 emitted = (col + whiteHot * HOT_CORE) * opacity;

    outColor = vec4(emitted, opacity * OVER);
}
