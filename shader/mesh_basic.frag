#version 460
#extension GL_EXT_nonuniform_qualifier : require

layout(location = 0) in vec3 inWorldNormal;
layout(location = 1) in vec4 inColor;
layout(location = 2) in vec2 inTexCoord;
layout(location = 3) flat in uint inInstanceId;
layout(location = 4) in vec3 inWorldPos;

layout(location = 0) out vec4 outColor;

struct Material {
    uvec4 textureIds;
    vec4 albedo;
    vec4 ambient;
    vec4 emission;
    float roughness;
    float metallic;
    uint emissionTextureId;
    uint _pad;
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

layout(set = 0, binding = 1) uniform LightCountBuffer {
    uint count;
} lightCountBuffer;

layout(set = 0, binding = 2) readonly buffer LightBuffer {
    Light lights[];
} lightBuffer;

layout(set = 0, binding = 3) readonly buffer LightIndicesBuffer {
    uint lightIndices[];
} tileLightIndices;

layout(set = 0, binding = 4) readonly buffer TransformBuffer {
    Transform transforms[];
} transforms;

layout(set = 0, binding = 5) readonly buffer MeshBuffer {
    Mesh meshes[];
} meshes;

layout(set = 0, binding = 6) uniform sampler2D textureArray[];

#define TILE_WIDTH 16u
#define TILE_HEIGHT 16u
#define MAX_LIGHTS_PER_TILE 60u

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

vec3 applyNormalMap(vec3 surfaceNormal, vec2 texCoord, uint normalTextureId) {
    vec3 normal = normalize(surfaceNormal);
    vec3 positionDx = dFdx(inWorldPos);
    vec3 positionDy = dFdy(inWorldPos);
    vec2 texCoordDx = dFdx(texCoord);
    vec2 texCoordDy = dFdy(texCoord);
    float uvDeterminant = texCoordDx.x * texCoordDy.y -
        texCoordDx.y * texCoordDy.x;
    if (abs(uvDeterminant) <= 1e-8) {
        return normal;
    }

    vec3 tangent = (positionDx * texCoordDy.y - positionDy * texCoordDx.y) /
        uvDeterminant;
    tangent -= normal * dot(normal, tangent);
    float tangentLengthSquared = dot(tangent, tangent);
    if (tangentLengthSquared <= 1e-12) {
        return normal;
    }
    tangent *= inversesqrt(tangentLengthSquared);
    vec3 bitangent = normalize(cross(normal, tangent)) * sign(uvDeterminant);
    vec3 tangentNormal = texture(
        textureArray[nonuniformEXT(normalTextureId)], texCoord).xyz * 2.0 - 1.0;
    return normalize(mat3(tangent, bitangent, normal) * tangentNormal);
}

void main() {
    Material material = meshes.meshes[inInstanceId].material;
    vec3 normal = normalize(inWorldNormal);
    if (material.textureIds.y != 0xffffffffu) {
        normal = applyNormalMap(normal, inTexCoord, material.textureIds.y);
    }
    vec3 viewDirection = normalize(camera.camPos.xyz - inWorldPos);
    vec3 lighting = material.ambient.rgb;

    uint px = uint(gl_FragCoord.x);
    uint py = uint(gl_FragCoord.y);
    uint tilesX = (uint(camera.screenSize.x) + TILE_WIDTH - 1u) / TILE_WIDTH;
    uint tileX = px / TILE_WIDTH;
    uint tileY = py / TILE_HEIGHT;
    uint tileIndex = tileY * tilesX + tileX;
    uint entriesPerTile = MAX_LIGHTS_PER_TILE + 1u;
    uint base = tileIndex * entriesPerTile;
    uint count = min(tileLightIndices.lightIndices[base], MAX_LIGHTS_PER_TILE);

    for (uint index = 0u; index < count; ++index) {
        uint lightIndex = tileLightIndices.lightIndices[base + 1u + index];
        if (lightIndex >= lightCountBuffer.count ||
            lightBuffer.lights[lightIndex].transformId >= transforms.transforms.length()) {
            continue;
        }
        float roughness = material.roughness;
        if (material.textureIds.z != 0xffffffffu) {
            roughness = texture(
                textureArray[nonuniformEXT(material.textureIds.z)], inTexCoord).r;
        }
        lighting += evaluateLight(
            lightBuffer.lights[lightIndex], normal, viewDirection, roughness);
    }

    vec4 surfaceColor = vec4(inColor.rgb, inColor.a) * material.albedo;
    if (material.textureIds.x != 0xffffffffu) {
        surfaceColor *= texture(
            textureArray[nonuniformEXT(material.textureIds.x)],
            inTexCoord);
    }
    vec3 albedo = surfaceColor.rgb;
    vec3 emission = material.emission.rgb;
    if (material.emissionTextureId != 0xffffffffu) {
        emission *= texture(
            textureArray[nonuniformEXT(material.emissionTextureId)], inTexCoord).rgb;
    }
    vec3 color = albedo * lighting + emission;
    outColor = vec4(color, surfaceColor.a);
}
