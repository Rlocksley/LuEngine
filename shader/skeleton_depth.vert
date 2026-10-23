#version 460
#define MAX_BONES_PER_SKELETON 128u
#define MATS_PER_SKELETON (2u * MAX_BONES_PER_SKELETON)

layout(location = 0) in vec3 inPosition;
layout(location = 1) in ivec4 inBones;
layout(location = 2) in vec4 inWeights;
layout(location = 3) in int inNodeIndex;

struct Transform { mat4 model; mat4 normal; vec4 scale; vec4 rotation; vec4 position; };
struct Material { uvec4 texturesIds; vec4 albedo; vec4 ambient; vec4 emission; float roughness; float metallic; uint emissionTextureId; uint pad0; };
struct SkeletonMesh { Material material; uint transformId; uint pipelineId; uint meshInfoId; uint skeletonId; uint nodeIndex; uint valid; uint localTransformId; uint pad1; };

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

layout(set = 0, binding = 1) readonly buffer TransformBuffer { Transform transforms[]; } transforms;
layout(set = 0, binding = 2) readonly buffer SkeletonMeshBuffer { SkeletonMesh meshes[]; } meshes;
layout(set = 0, binding = 3) readonly buffer SkeletonBuffer { mat4 data[]; } skeletons;

void main() {
    SkeletonMesh mesh = meshes.meshes[gl_InstanceIndex];
    Transform transform = transforms.transforms[mesh.transformId];
    Transform local = transforms.transforms[mesh.localTransformId];   // MeshTransform (slot 0 = identity)
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
    gl_Position = camera.viewProjection * transform.model * local.model * skinMatrix * vec4(inPosition, 1.0);
}
