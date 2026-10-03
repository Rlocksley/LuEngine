#version 460

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec4 inColor;
layout(location = 3) in vec2 inTexCoord;

layout(location = 0) out vec3 outWorldNormal;
layout(location = 1) out vec4 outColor;
layout(location = 2) out vec2 outTexCoord;
layout(location = 3) flat out uint outInstanceId;
layout(location = 4) out vec3 outWorldPos;

struct Transform {
    mat4 model;
    mat4 normal;
    vec4 scale;
    vec4 rotation;
    vec4 position;
};

struct Material{
    ivec4 texturesIds; // x = albedo, y = normal, z = roughness, w = metallic
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
    mat4 projection;       // Projection matrix
    mat4 view;             // View matrix (world → view)
    mat4 viewProjection;
    mat4 inverseProjection;// Inverse of projection
    vec4 camPos;
    vec4 camDir;
    vec2 screenSize;       // Width, height in pixels
    float nearClip;        // Near plane distance
    float farClip;         // Far plane distance
}camera;

layout(set = 0, binding = 1) readonly buffer TransformBuffer {
    Transform transforms[];
} transforms;

layout(set = 0, binding = 2) readonly buffer MeshBuffer {
    Mesh meshes[];
} meshes;


void main() {
    Mesh mesh = meshes.meshes[gl_InstanceIndex];
    Transform transform = transforms.transforms[mesh.transformId];

    vec4 worldPos = transform.model * vec4(inPosition, 1.0);
    gl_Position = camera.viewProjection * worldPos;

    outWorldNormal = normalize((transform.normal * vec4(inNormal, 0.0)).xyz);
    outColor = inColor;
    outTexCoord = inTexCoord;
    outInstanceId = gl_InstanceIndex;
    outWorldPos = worldPos.xyz;
}
