#pragma once
#include "Global.hpp"

namespace Lu{
    namespace Core{

    struct Material{
        glm::uvec4 texturesIds{UINT32_MAX, UINT32_MAX, UINT32_MAX, UINT32_MAX}; // x = albedo, y = normal, z = roughness, w = metallic
        glm::vec4 albedo{0.f, 0.f, 0.f, 1.f};
        glm::vec4 ambient{0.f, 0.f, 0.f, 1.f};
        glm::vec4 emission{0.f, 0.f, 0.f, 1.f};
        float roughness{0.5f};
        float metallic{0.0f};
        uint32_t emissionTextureId{UINT32_MAX};
        uint32_t _pad{0};

        Material() = default;

        Material(const uint32_t textureIds[5],
            glm::vec4 albedo, glm::vec4 ambient, glm::vec4 emission, 
            float roughness, float metallic):
            texturesIds(textureIds[0], textureIds[1], textureIds[2], textureIds[3]),
            albedo(albedo),
            ambient(ambient),
            emission(emission),
            roughness(roughness),
            metallic(metallic),
            emissionTextureId(textureIds[4]){}
    };
}
}
