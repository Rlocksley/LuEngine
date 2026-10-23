#version 460

layout(location = 0) in vec3 inPosition;

struct Transform {
    mat4 model;
    mat4 normal;
    vec4 scale;
    vec4 rotation;
    vec4 position;
};

struct Material {
    uvec4 texturesIds; // x = albedo, y = normal, z = roughness, w = metallic
    vec4 albedo;
    vec4 ambient;
    vec4 emission;
    float roughness;
    float metallic;
    uint emissionTextureId;
    uint pad0;
};

struct Mesh {
    Material material;
    uint transformId;
    uint pipelineId;
    uint meshInfoId;
    uint valid;
    uint localTransformId;   // MeshTransform slot, 0 = identity
    uint pad1;
    uint pad2;
    uint pad3;
};

layout(set = 0, binding = 0) uniform CameraBuffer {
    vec4 frustumPlanes[6];
    mat4 projection;
    mat4 view;
    mat4 viewProjection;
    mat4 inverseProjection;
    vec4 camPos;
    vec4 camDir;
    vec2 screenSize;
    float nearClip;
    float farClip;
    float deltaTime;
    float time;
} camera;

layout(set = 0, binding = 1) readonly buffer TransformBuffer {
    Transform transforms[];
} transforms;

layout(set = 0, binding = 2) readonly buffer MeshBuffer {
    Mesh meshes[];
} meshes;

void main() {
    Mesh mesh = meshes.meshes[gl_InstanceIndex];
    Transform transform = transforms.transforms[mesh.transformId];
    Transform local = transforms.transforms[mesh.localTransformId];   // MeshTransform (slot 0 = identity)
    gl_Position = camera.viewProjection * transform.model * local.model * vec4(inPosition, 1.0);
}
