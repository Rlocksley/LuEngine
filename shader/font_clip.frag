#version 460
#extension GL_EXT_nonuniform_qualifier : require

layout(location = 0) in vec2 inTexCoord;
layout(location = 1) in vec4 inColor;
layout(location = 2) flat in uint inTextureId;
layout(location = 3) flat in float inSdf;
layout(location = 4) in vec2 inClip;             // position in the clip rectangle (see font_clip.vert)

layout(location = 0) out vec4 outColor;

// the global texture array (TexturePackage); the font atlas stores its data in the alpha channel
layout(set = 0, binding = 3) uniform sampler2D textureArray[];

// Like font_basic.frag + discards everything outside of the clip rectangle of Component::Text::clip.
void main() {
    float value = texture(textureArray[nonuniformEXT(inTextureId)], inTexCoord).a;

    float coverage = value;
    if (inSdf > 0.5) {
        // signed distance field: 0.5 is the outline, antialias over about one screen pixel
        float width = max(fwidth(value), 1.0 / 255.0);
        coverage = smoothstep(0.5 - width, 0.5 + width, value);
    }

    float alpha = coverage * inColor.a;
    bool clipped = any(lessThan(inClip, vec2(0.0))) || any(greaterThan(inClip, vec2(1.0)));
    if (alpha < 0.004 || clipped) {
        discard;     // transparent texels must not write depth, they would hide glyphs of lower layers
    }
    outColor = vec4(inColor.rgb, alpha);
}
