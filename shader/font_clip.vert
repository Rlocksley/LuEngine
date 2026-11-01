#version 460

// Like font_basic.vert + CLIPPING: Component::Text::clip names an entity with a Transform2D; glyphs are only drawn inside the
// unit square (0,0)..(1,1) of the local space of that transform (a rotated / scaled rectangle that follows its hierarchy).
// The vertex shader computes the position of every glyph corner in that local space (affine, so the interpolated value is
// exact and glyphs are cut pixel exact), the fragment shader (font_clip.frag) discards what is outside.
//
// Draws one glyph quad per indirect command. gl_VertexIndex = glyphSlot * 6 + corner (see font_cull.comp).
layout(location = 0) out vec2 outTexCoord;
layout(location = 1) out vec4 outColor;
layout(location = 2) flat out uint outTextureId;
layout(location = 3) flat out float outSdf;
layout(location = 4) out vec2 outClip;           // position in the clip rectangle: inside = 0..1 (no clip: 0.5)

struct Transform2D{
    mat4 model;
    vec2 position;
    vec2 scale;
    vec4 rotation;
    uint positionZ;
    uint pad0;
    uint pad1;
    uint pad2;
};

struct Font{
    vec4 uvRect;          // u0, v0, u1, v1
    vec4 color;
    vec2 offset;
    vec2 size;
    uint textureId;
    uint transform2DId;
    uint pipelineId;
    uint valid;
    uint glyphIndex;      // index of the glyph in its text   (for per-glyph effects)
    uint glyphCount;      // number of glyphs of the text
    float sdf;
    uint clipTransform2DId;   // slot of the Transform2D that is the clip rectangle, 0xFFFFFFFF = none
};

layout(set = 0, binding = 0) uniform Camera2DBuffer {
    mat4 viewProjection;  // world pixels -> clip space xy
    mat4 view;            // world pixels -> screen pixels
    vec2 position;
    vec2 screenSize;
    float zoom;
    float rotation;
    float deltaTime;
    float time;
} camera;

layout(set = 0, binding = 1) readonly buffer Transform2DBuffer {
    Transform2D transforms[];
} transforms;

layout(set = 0, binding = 2) readonly buffer FontBuffer {
    Font fonts[];
} fontBuffer;

// 2 triangles; (0,0) = top-left of the glyph quad
const vec2 corners[6] = vec2[6](
    vec2(0.0, 0.0), vec2(1.0, 0.0), vec2(0.0, 1.0),
    vec2(0.0, 1.0), vec2(1.0, 0.0), vec2(1.0, 1.0)
);

// must match TRANSFORM_2D_LAYER_COUNT in Constant.hpp
const float LAYER_COUNT = 65536.0;

void main() {
    uint glyph = uint(gl_VertexIndex) / 6u;
    vec2 corner = corners[uint(gl_VertexIndex) % 6u];

    Font font = fontBuffer.fonts[glyph];
    Transform2D transform = transforms.transforms[font.transform2DId];

    vec2 local = font.offset + corner * font.size;
    vec2 world = (transform.model * vec4(local, 0.0, 1.0)).xy;
    vec4 clip = camera.viewProjection * vec4(world, 0.0, 1.0);

    // Transform2D::positionZ is the layer, a higher layer is closer to the viewer (depth test LESS / LESS_OR_EQUAL).
    float depth = 1.0 - (float(min(transform.positionZ, 65535u)) + 1.0) / (LAYER_COUNT + 1.0);

    gl_Position = vec4(clip.xy, depth, 1.0);
    outTexCoord = mix(font.uvRect.xy, font.uvRect.zw, corner);
    outColor = font.color;
    outTextureId = font.textureId;
    outSdf = font.sdf;

    outClip = vec2(0.5);
    if (font.clipTransform2DId != 0xFFFFFFFFu) {
        mat4 clipModel = transforms.transforms[font.clipTransform2DId].model;
        if (abs(determinant(mat2(clipModel))) > 1e-12) {
            outClip = (inverse(clipModel) * vec4(world, 0.0, 1.0)).xy;
        } else {
            outClip = vec2(-1.0);     // a clip rectangle without area clips everything
        }
    }
}
