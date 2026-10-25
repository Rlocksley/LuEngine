#version 460
#extension GL_EXT_nonuniform_qualifier : require

layout(location = 0) in vec4 inColor;
layout(location = 1) in vec2 inUv;
layout(location = 2) flat in uint inTextureId;
layout(location = 3) flat in uint inMeshId;

layout(location = 0) out vec4 outColor;

// the global texture array (TexturePackage)
layout(set = 0, binding = 3) uniform sampler2D textureArray[];

void main() {
    vec4 color = inColor;
    if (inTextureId != 0xFFFFFFFFu) {
        color *= texture(textureArray[nonuniformEXT(inTextureId)], inUv);
    }

    if (color.a < 0.004) {
        discard;     // transparent texels must not write depth, they would hide meshes and text of lower layers
    }
    outColor = color;
}
