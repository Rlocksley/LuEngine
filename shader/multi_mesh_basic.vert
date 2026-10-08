#version 460
#extension GL_EXT_buffer_reference2 : require
#extension GL_EXT_shader_explicit_arithmetic_types_int64 : require
#extension GL_ARB_shader_draw_parameters : require

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec4 inColor;
layout(location = 3) in vec2 inTexCoord;

layout(location = 0) out vec3 outWorldNormal;
layout(location = 1) out vec4 outColor;
layout(location = 2) out vec2 outTexCoord;
layout(location = 3) flat out vec4 outMaterialAlbedo;
layout(location = 4) flat out vec4 outMaterialAmbient;
layout(location = 5) out vec3 outWorldPos;
layout(location = 6) flat out vec4 outMaterialEmission;

struct Transform {
    mat4 model;
    mat4 normal;
    vec4 scale;
    vec4 rotation;
    vec4 position;
};

struct Material {
    ivec4 texturesIds;
    vec4 albedo;
    vec4 ambient;
    vec4 emission;
    float roughness;
    float metallic;
    uint pad0;
    uint pad1;
};

struct MeshInstance {
    Material material;
    Transform transform;
};

struct MultiMesh {
    uint transformId;
    uint computePipeId;
    uint pipelineId;
    uint meshInfoId;
    uint size;
    uint valid;
    uint _pad0;
    uint _pad1;
    uint64_t meshInstanceBufferAddress[2];
    vec4 cullSphere;
};

layout(buffer_reference, std430, buffer_reference_align = 16) readonly buffer MeshInstanceBuffer {
    MeshInstance values[];
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
    float deltaTime;
    float time;
} camera;

layout(set = 0, binding = 1, std430) readonly buffer ParentTransformBuffer {
    Transform values[];
} parentTransforms;

layout(set = 0, binding = 2, std430) readonly buffer MultiMeshBuffer {
    MultiMesh values[];
} multiMeshes;

void main() {
    MultiMesh mesh = multiMeshes.values[gl_DrawID];
    MeshInstanceBuffer transformBuffer =
        MeshInstanceBuffer(mesh.meshInstanceBufferAddress[0]);
    MeshInstance instance = transformBuffer.values[gl_InstanceIndex];
    Transform parent = parentTransforms.values[mesh.transformId];

    mat4 model = parent.model * instance.transform.model;
    vec4 worldPosition = model * vec4(inPosition, 1.0);
    gl_Position = camera.viewProjection * worldPosition;

    outWorldNormal = normalize((parent.normal * instance.transform.normal * vec4(inNormal, 0.0)).xyz);
    outColor = inColor;
    outTexCoord = inTexCoord;
    outMaterialAlbedo = instance.material.albedo;
    outMaterialAmbient = instance.material.ambient;
    outWorldPos = worldPosition.xyz;
    outMaterialEmission = instance.material.emission;
}