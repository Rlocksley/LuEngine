#version 460
#extension GL_EXT_nonuniform_qualifier : enable

layout(location = 0) in vec3 inWorldNormal;
layout(location = 1) in vec4 inColor;
layout(location = 2) in vec2 inTexCoord;
layout(location = 3) flat in uint inInstanceId;
layout(location = 4) in vec3 inWorldPos;

layout(location = 0) out vec4 outColor;

struct Material{
    ivec4 textureIds; // x = albedo, y = normal, z = roughness, w = metallic
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

struct Light {
    vec4 position;   // xyz = world position, w = type (0=point, 1=spot, 2=directional)
    vec4 direction;  // xyz = direction (spot/directional), w = unused
    vec4 color;      // rgb = color, a = intensity
    vec4 params;     // x = radius, y = cos(innerCutoff), z = cos(outerCutoff), w = unused
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
} camera;

layout(set = 0, binding = 2) readonly buffer MeshBuffer {
    Mesh meshes[];
} meshes;


vec3 calculateDirectionalLight(Light light, vec3 N, vec3 V) {
    vec3 L = normalize(-light.direction.xyz);

    // Diffuse
    float NdotL = max(dot(N, L), 0.0);
    vec3 diffuse = NdotL * light.color.rgb * light.color.a;

    // Specular (Blinn-Phong)
    vec3 H = normalize(L + V);
    float NdotH = max(dot(N, H), 0.0);
    vec3 specular = pow(NdotH, 32.0) * light.color.rgb * light.color.a;

    return diffuse + specular;
}

void main() {
    Light light;
    light.direction = vec4(-1,-1,-1,1);
    light.color = vec4(1,1,1,1000);

    Material material = meshes.meshes[inInstanceId].material;

    vec4 albedo = material.albedo; 
    
    vec3 N = normalize(inWorldNormal);
    vec3 V = normalize(camera.camPos.xyz - inWorldPos);

    vec3 lighting =  material.ambient.xyz;
    lighting += calculateDirectionalLight(light, N, V);


    outColor = vec4(inColor.rgb * albedo.rgb * lighting, inColor.a * albedo.a);
}
