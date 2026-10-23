#version 460
#extension GL_EXT_nonuniform_qualifier : require

layout(location = 0) in vec3 inWorldNormal;
layout(location = 1) in vec4 inColor;
layout(location = 2) in vec2 inTexCoord;
layout(location = 3) flat in uint inInstanceId;
layout(location = 4) in vec3 inWorldPos;

layout(location = 0) out vec4 outColor;

const float PI = 3.14159265359;

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
    uint skeletonId;
    uint nodeIndex;
    uint valid;
    uint pad0;
    uint pad1;
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

float distributionGGX(vec3 normal, vec3 halfwayDirection, float roughness) {
    float alpha = roughness * roughness;
    float alphaSquared = alpha * alpha;
    float nDotH = max(dot(normal, halfwayDirection), 0.0);
    float denominator = nDotH * nDotH * (alphaSquared - 1.0) + 1.0;
    return alphaSquared / max(PI * denominator * denominator, 1e-6);
}

float geometrySchlickGGX(float nDotDirection, float roughness) {
    float k = (roughness + 1.0) * (roughness + 1.0) / 8.0;
    return nDotDirection / max(nDotDirection * (1.0 - k) + k, 1e-6);
}

float geometrySmith(vec3 normal, vec3 viewDirection, vec3 lightDirection, float roughness) {
    return geometrySchlickGGX(max(dot(normal, viewDirection), 0.0), roughness) *
        geometrySchlickGGX(max(dot(normal, lightDirection), 0.0), roughness);
}

vec3 fresnelSchlick(float cosine, vec3 f0) {
    return f0 + (1.0 - f0) * pow(1.0 - clamp(cosine, 0.0, 1.0), 5.0);
}

vec3 evaluateLight(
    Light light,
    vec3 normal,
    vec3 viewDirection,
    vec3 albedo,
    float metallic,
    float roughness
) {
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
            attenuation *= smoothstep(light.params.z, light.params.y, coneCosine);
        }
    }

    float nDotL = max(dot(normal, toLight), 0.0);
    float nDotV = max(dot(normal, viewDirection), 0.0);
    if (nDotL <= 0.0 || nDotV <= 0.0) {
        return vec3(0.0);
    }

    vec3 halfwayDirection = normalize(toLight + viewDirection);
    vec3 f0 = mix(vec3(0.04), albedo, metallic);
    vec3 fresnel = fresnelSchlick(max(dot(halfwayDirection, viewDirection), 0.0), f0);
    float distribution = distributionGGX(normal, halfwayDirection, roughness);
    float geometry = geometrySmith(normal, viewDirection, toLight, roughness);

    vec3 specular = (distribution * geometry * fresnel) /
        max(4.0 * nDotV * nDotL, 1e-5);
    vec3 diffuseWeight = (1.0 - fresnel) * (1.0 - metallic);
    vec3 diffuse = diffuseWeight * albedo / PI;
    vec3 radiance = light.color.rgb * light.color.a * attenuation;
    return (diffuse + specular) * radiance * nDotL;
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
    vec4 surfaceColor = vec4(inColor.rgb, inColor.a) * material.albedo;
    if (material.textureIds.x != 0xffffffffu) {
        surfaceColor *= texture(
            textureArray[nonuniformEXT(material.textureIds.x)],
            inTexCoord);
    }
    vec3 albedo = surfaceColor.rgb;
    float metallic = clamp(material.metallic, 0.0, 1.0);
    float roughness = clamp(material.roughness, 0.045, 1.0);
    if (material.textureIds.z != 0xffffffffu) {
        roughness = clamp(
            texture(textureArray[nonuniformEXT(material.textureIds.z)], inTexCoord).r,
            0.045,
            1.0);
    }
    if (material.textureIds.w != 0xffffffffu) {
        metallic = clamp(
            texture(textureArray[nonuniformEXT(material.textureIds.w)], inTexCoord).r,
            0.0,
            1.0);
    }

    vec3 emission = material.emission.rgb;
    if (material.emissionTextureId != 0xffffffffu) {
        emission *= texture(
            textureArray[nonuniformEXT(material.emissionTextureId)], inTexCoord).rgb;
    }
    vec3 color = albedo * material.ambient.rgb + emission;
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
        color += evaluateLight(
            lightBuffer.lights[lightIndex],
            normal,
            viewDirection,
            albedo,
            metallic,
            roughness
        );
    }

    outColor = vec4(color, surfaceColor.a);
}
