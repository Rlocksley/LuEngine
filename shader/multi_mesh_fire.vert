#version 460
#extension GL_EXT_buffer_reference2 : require
#extension GL_EXT_shader_explicit_arithmetic_types_int64 : require
#extension GL_ARB_shader_draw_parameters : require

// =============================================================================
// multi_mesh_fire.vert
//
// Shared vertex shader of all fire layers. Turns every instance of the 4-vertex
// Shape::Square into a camera facing billboard:
//
//  * the quad is spanned by the camera right / up vectors (taken from the view
//    matrix), with "up" re-aligned to world-up on screen so flames stay upright
//  * optional roll around the view axis (smoke)
//  * speed dependent elongation (tongues / streaks); sparks also align with the velocity
//  * the particle state written by multi_mesh_fire_move.comp is forwarded to
//    the fragment shader (see the slot table in that file)
//
// The fragment shader receives the fragment position in the PARENT's local
// space, so all procedural noise is anchored to the fire, not to the world.
// =============================================================================

layout(location = 0) in vec3 inPosition;
layout(location = 3) in vec2 inTexCoord;

layout(location = 0) out vec2 outUV;                 // [-1,1]^2 over the quad
layout(location = 1) flat out vec4 outState;         // age01, life, seed, clock
layout(location = 2) flat out vec4 outLook;          // brightness, lean, nearFade, screen speed
layout(location = 3) flat out vec3 outTint;
layout(location = 4) out vec3 outLocalPos;           // fragment position, parent local space
layout(location = 5) flat out vec3 outLocalCenter;   // particle centre, parent local space

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
    mat4 projection;
    mat4 view;
    mat4 viewProjection;
    mat4 inverseProjection;
    vec4 camPos;
    vec4 camDir;
    vec2 screenSize;
    float nearClip;
    float farClip;
} camera;

layout(set = 0, binding = 1, std430) readonly buffer ParentTransformBuffer {
    Transform values[];
} parentTransforms;

layout(set = 0, binding = 2, std430) readonly buffer MultiMeshBuffer {
    MultiMesh values[];
} multiMeshes;

void main() {
    MultiMesh mesh = multiMeshes.values[gl_DrawID];
    MeshInstanceBuffer instances = MeshInstanceBuffer(mesh.meshInstanceBufferAddress[0]);
    MeshInstance instance = instances.values[gl_InstanceIndex];
    Transform parent = parentTransforms.values[mesh.transformId];

    // ---- particle state ------------------------------------------------------
    vec3 localCenter = instance.transform.position.xyz;
    float size = instance.transform.scale.x;
    vec3 velocity = instance.material.ambient.xyz;
    float roll = instance.material.ambient.w;
    float age = instance.material.emission.x;
    float life = max(instance.material.emission.y, 1e-3);
    float stretch = instance.material.metallic;
    bool alignToVelocity = (instance.material.texturesIds.w & 1) != 0;

    mat3 parentRot = mat3(parent.model);
    float parentScale = length(parent.model[0].xyz);
    vec3 worldCenter = (parent.model * vec4(localCenter, 1.0)).xyz;

    // ---- camera basis --------------------------------------------------------
    vec3 camRight = vec3(camera.view[0][0], camera.view[1][0], camera.view[2][0]);
    vec3 camUp    = vec3(camera.view[0][1], camera.view[1][1], camera.view[2][1]);

    // Keep world-up pointing up on screen (identical to camUp unless the camera rolls).
    vec2 upScreen = vec2(camRight.y, camUp.y);
    float upLen = length(upScreen);
    upScreen = (upLen > 1e-3) ? upScreen / upLen : vec2(0.0, 1.0);
    vec3 axisY = camRight * upScreen.x + camUp * upScreen.y;
    vec3 axisX = camRight * upScreen.y - camUp * upScreen.x;

    // ---- shape modifiers -----------------------------------------------------
    vec3 velWorld = parentRot * velocity;
    vec2 v2 = vec2(dot(velWorld, axisX), dot(velWorld, axisY)); // velocity on screen
    float speed2 = length(v2);

    float lenY = 1.0;
    float lean = clamp(v2.x / (abs(v2.y) + 0.6), -1.0, 1.0);

    if (alignToVelocity) {
        // sparks: align the quad with the motion (motion-blur like streaks)
        vec2 d = (speed2 > 1e-3) ? v2 / speed2 : vec2(0.0, 1.0);
        vec3 ax = axisX * d.y - axisY * d.x;
        vec3 ay = axisX * d.x + axisY * d.y;
        axisX = ax;
        axisY = ay;
        lean = 0.0;
    } else if (roll != 0.0) {
        // smoke: tumble around the view axis
        float c = cos(roll), s = sin(roll);
        vec3 ax = axisX * c + axisY * s;
        vec3 ay = axisY * c - axisX * s;
        axisX = ax;
        axisY = ay;
        lean = 0.0;
    }
    // elongate along the quad's y axis with speed (flame tongues, spark streaks)
    lenY = 1.0 + stretch * speed2 * 2.0;

    // ---- build the billboard -------------------------------------------------
    vec2 uv = inTexCoord * 2.0 - 1.0;
    vec3 offsetWorld = (axisX * uv.x + axisY * uv.y * lenY) * (0.5 * size * parentScale);
    vec3 worldPos = worldCenter + offsetWorld;
    gl_Position = camera.viewProjection * vec4(worldPos, 1.0);

    // fade out particles that would swallow the camera
    float distToCam = length(worldCenter - camera.camPos.xyz);
    float nearFade = smoothstep(camera.nearClip, camera.nearClip + 0.6 + size * parentScale, distToCam);

    outUV = uv;
    outState = vec4(clamp(age / life, 0.0, 1.0), life, instance.material.emission.z, instance.material.emission.w);
    outLook = vec4(instance.material.albedo.a, lean, nearFade, speed2);
    outTint = instance.material.albedo.rgb;
    outLocalCenter = localCenter;
    outLocalPos = localCenter + inverse(parentRot) * offsetWorld;
}
