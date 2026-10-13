#version 460

layout(location = 0) in vec3 inWorldNormal;
layout(location = 1) in vec4 inColor;
layout(location = 2) in vec2 inTexCoord;
layout(location = 3) flat in uint inInstanceId;
layout(location = 4) in vec3 inWorldPos;

layout(location = 0) out vec4 outColor;

struct Material {
    ivec4 textureIds;
    vec4 albedo;
    vec4 ambient;
    vec4 emission;
    float roughness;
    float metallic;
    vec2 _pad;
};

struct Mesh {
    Material material;
    uint transformId;
    uint pipelineId;
    uint meshInfoId;
    uint valid;
};

struct Transform {
    mat4 model;
    mat4 normal;
    vec4 scale;
    vec4 rotation;
    vec4 position;
};

struct Light {
    vec4 color;
    vec4 params;
    uint type;
    uint transformId;
    uint pad0;
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

layout(set = 0, binding = 1) readonly buffer LightCountBuffer {
    uint count;
} lightCountBuffer;

layout(set = 0, binding = 2) readonly buffer LightBuffer {
    Light lights[];
} lightBuffer;

layout(set = 0, binding = 3) readonly buffer TransformBuffer {
    Transform transforms[];
} transforms;

layout(set = 0, binding = 4) readonly buffer MeshBuffer {
    Mesh meshes[];
} meshes;

vec3 evaluateLight(Light light, vec3 normal, vec3 viewDirection, float roughness) {
    Transform lightTransform = transforms.transforms[light.transformId];
    vec3 lightPosition = lightTransform.position.xyz;
    vec3 lightDirection = normalize(
        (lightTransform.model * vec4(0.0, 0.0, -1.0, 0.0)).xyz);

    vec3 toLight;
    float attenuation = 1.0;
    if (light.type == 2u) {
        toLight = normalize(-lightDirection);
    } else {
        vec3 fromLight = inWorldPos - lightPosition;
        float distanceToLight = length(fromLight);
        if (distanceToLight >= light.params.x) {
            return vec3(0.0);
        }

        toLight = -normalize(fromLight);
        float rangeFalloff = 1.0 - smoothstep(0.0, light.params.x, distanceToLight);
        attenuation = rangeFalloff * rangeFalloff /
            (1.0 + 0.09 * distanceToLight + 0.032 * distanceToLight * distanceToLight);

        if (light.type == 1u) {
            float coneCosine = dot(normalize(fromLight), lightDirection);
            float cone = smoothstep(light.params.z, light.params.y, coneCosine);
            attenuation *= cone;
        }
    }

    float diffuseFactor = max(dot(normal, toLight), 0.0);
    vec3 halfwayDirection = normalize(toLight + viewDirection);
    float shininess = mix(128.0, 4.0, clamp(roughness, 0.0, 1.0));
    float specularFactor = pow(max(dot(normal, halfwayDirection), 0.0), shininess);

    vec3 diffuse = diffuseFactor * light.color.rgb;
    vec3 specular = specularFactor * light.color.rgb * 0.2;
    return (diffuse + specular) * light.color.a * attenuation;
}

void main() {
    Material material = meshes.meshes[inInstanceId].material;
    vec3 normal = normalize(inWorldNormal);
    vec3 viewDirection = normalize(camera.camPos.xyz - inWorldPos);
    vec3 lighting = material.ambient.rgb;

    for (uint index = 0; index < lightCountBuffer.count; ++index) {
        lighting += evaluateLight(
            lightBuffer.lights[index], normal, viewDirection, material.roughness);
    }

    vec3 albedo = inColor.rgb * material.albedo.rgb;
    vec3 color = albedo * lighting + material.emission.rgb;
    outColor = vec4(color, inColor.a * material.albedo.a);
}
