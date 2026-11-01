#version 460
#extension GL_EXT_nonuniform_qualifier : require

layout(location = 0) in vec4 inColor;
layout(location = 1) in vec2 inUv;
layout(location = 2) flat in uint inTextureId;
layout(location = 3) flat in uint inMeshId;
layout(location = 4) in vec2 inClip;               // position in the clip rectangle (see mesh2d_clip.vert)

layout(location = 0) out vec4 outColor;

// the global texture array (TexturePackage)
layout(set = 0, binding = 3) uniform sampler2D textureArray[];

// Like mesh2d_basic.frag + discards everything outside of the clip rectangle of Component::Mesh2D::clip.
void main() {
    vec4 color = inColor;
    if (inTextureId != 0xFFFFFFFFu) {
        color *= texture(textureArray[nonuniformEXT(inTextureId)], inUv);
    }

    bool clipped = any(lessThan(inClip, vec2(0.0))) || any(greaterThan(inClip, vec2(1.0)));
    if (color.a < 0.004 || clipped) {
        discard;     // transparent texels must not write depth, they would hide meshes and text of lower layers
    }
    outColor = color;
}
