#version 460
// Skinned mesh vertex shader.
//  mesh.nodeIndex != 0xFFFFFFFF : whole mesh rigidly attached to that skeleton node
//  inNodeIndex == -1            : deformed by the 4 bone weights (skin matrices)
//  otherwise                    : this vertex is attached to node inNodeIndex
#define MAX_BONES_PER_SKELETON 128u
#define MATS_PER_SKELETON (2u * MAX_BONES_PER_SKELETON)

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec4 inColor;
layout(location = 3) in vec2 inTexCoord;
layout(location = 4) in ivec4 inBones;
layout(location = 5) in vec4 inWeights;
layout(location = 6) in int inNodeIndex;

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
    uvec4 texturesIds;
    vec4 albedo;
    vec4 ambient;
    vec4 emission;
    float roughness;
    float metallic;
    uint emissionTextureId;
    uint pad0;
};

struct SkeletonMesh {
    Material material;
    uint transformId;
    uint pipelineId;
    uint meshInfoId;
    uint skeletonId;
    uint nodeIndex;
    uint valid;
    uint localTransformId;   // MeshTransform slot, 0 = identity
    uint pad1;
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

layout(set = 0, binding = 4) readonly buffer TransformBuffer { Transform transforms[]; } transforms;
layout(set = 0, binding = 5) readonly buffer SkeletonMeshBuffer { SkeletonMesh meshes[]; } meshes;
layout(set = 0, binding = 7) readonly buffer SkeletonBuffer { mat4 data[]; } skeletons;

void main() {
    SkeletonMesh mesh = meshes.meshes[gl_InstanceIndex];
    Transform transform = transforms.transforms[mesh.transformId];
    Transform local = transforms.transforms[mesh.localTransformId];   // MeshTransform (slot 0 = identity)
    mat4 model = transform.model * local.model;
    mat4 normalMatrix = transform.normal * local.normal;              // inverse-transpose of a product
    uint base = mesh.skeletonId * MATS_PER_SKELETON;

    mat4 skinMatrix;
    if (mesh.nodeIndex != 0xFFFFFFFFu) {
        skinMatrix = skeletons.data[base + MAX_BONES_PER_SKELETON + mesh.nodeIndex];
    } else if (inNodeIndex == -1) {
        skinMatrix = inWeights.x * skeletons.data[base + uint(inBones.x)]
                   + inWeights.y * skeletons.data[base + uint(inBones.y)]
                   + inWeights.z * skeletons.data[base + uint(inBones.z)]
                   + inWeights.w * skeletons.data[base + uint(inBones.w)];
    } else {
        skinMatrix = skeletons.data[base + MAX_BONES_PER_SKELETON + uint(inNodeIndex)];
    }

    vec4 skinnedPos = skinMatrix * vec4(inPosition, 1.0);
    vec3 skinnedNormal = mat3(skinMatrix) * inNormal;

    vec4 worldPos = model * skinnedPos;
    gl_Position = camera.viewProjection * worldPos;

    outWorldNormal = normalize((normalMatrix * vec4(skinnedNormal, 0.0)).xyz);
    outColor = inColor;
    outTexCoord = inTexCoord;
    outInstanceId = gl_InstanceIndex;
    outWorldPos = worldPos.xyz;
}
