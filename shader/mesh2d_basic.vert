#version 460

// Vertex2D: local pixels, placed by the Transform2D of the parent entity and the Camera2D.
layout(location = 0) in vec2 inPosition;
layout(location = 1) in vec2 inUv;
layout(location = 2) in vec4 inColor;

layout(location = 0) out vec4 outColor;
layout(location = 1) out vec2 outUv;
layout(location = 2) flat out uint outTextureId;
layout(location = 3) flat out uint outMeshId;      // slot of the Mesh2D, e.g. to read meshes[outMeshId].params in the fragment shader

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

struct Mesh2D{
    vec4 color;
    vec4 uvRect;          // u offset, v offset, u scale, v scale
    vec4 params;
    uint textureId;
    uint transform2DId;
    uint pipelineId;
    uint meshInfoId;
    uint valid;
    uint pad0;
    uint pad1;
    uint pad2;
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

layout(set = 0, binding = 2) readonly buffer Mesh2DBuffer {
    Mesh2D meshes[];
} meshes;

// must match TRANSFORM_2D_LAYER_COUNT in Constant.hpp (same mapping as font_basic.vert: text and meshes share the depth)
const float LAYER_COUNT = 65536.0;

void main() {
    Mesh2D mesh = meshes.meshes[gl_InstanceIndex];
    Transform2D transform = transforms.transforms[mesh.transform2DId];

    vec2 world = (transform.model * vec4(inPosition, 0.0, 1.0)).xy;
    vec4 clip = camera.viewProjection * vec4(world, 0.0, 1.0);

    // Transform2D::positionZ is the layer, a higher layer is closer to the viewer (depth test LESS / LESS_OR_EQUAL).
    float depth = 1.0 - (float(min(transform.positionZ, 65535u)) + 1.0) / (LAYER_COUNT + 1.0);

    gl_Position = vec4(clip.xy, depth, 1.0);
    outColor = inColor * mesh.color;
    outUv = mesh.uvRect.xy + inUv * mesh.uvRect.zw;
    outTextureId = mesh.textureId;
    outMeshId = gl_InstanceIndex;
}
