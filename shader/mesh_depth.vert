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
    ivec4 textureIds;
    vec4 albedo;
    vec4 ambient;
    vec4 emission;
    float roughness;
    float metallic;
    uint pad0;
    uint pad1;
};

struct Mesh {
    Material material;
    uint transformId;
    uint pipelineId;
    uint meshInfoId;
    uint valid;
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
    gl_Position = camera.viewProjection * transform.model * vec4(inPosition, 1.0);
}
