#version 460

layout(location = 0) in vec3 inWorldNormal;
layout(location = 1) in vec4 inColor;
layout(location = 2) in vec2 inTexCoord;
layout(location = 3) flat in vec4 inMaterialAlbedo;
layout(location = 4) flat in vec4 inMaterialAmbient;
layout(location = 5) in vec3 inWorldPos;

layout(location = 0) out vec4 outColor;

struct Light {
    vec4 position;
    vec4 direction;
    vec4 color;
    vec4 params;
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

vec3 calculateDirectionalLight(Light light, vec3 normal, vec3 viewDirection) {
    vec3 lightDirection = normalize(-light.direction.xyz);
    float normalLight = max(dot(normal, lightDirection), 0.0);
    vec3 diffuse = normalLight * light.color.rgb * light.color.a;
    vec3 halfwayDirection = normalize(lightDirection + viewDirection);
    float normalHalfway = max(dot(normal, halfwayDirection), 0.0);
    vec3 specular = pow(normalHalfway, 32.0) * light.color.rgb * light.color.a;
    return diffuse + specular;
}

void main() {
    Light light;
    light.direction = vec4(-1.0, -1.0, -1.0, 1.0);
    light.color = vec4(1.0, 1.0, 1.0, 1000.0);

    vec3 normal = normalize(inWorldNormal);
    vec3 viewDirection = normalize(camera.camPos.xyz - inWorldPos);
    vec3 lighting = inMaterialAmbient.xyz + calculateDirectionalLight(light, normal, viewDirection);

    outColor = vec4(inColor.rgb * inMaterialAlbedo.rgb * lighting,
                    inColor.a * inMaterialAlbedo.a);
}