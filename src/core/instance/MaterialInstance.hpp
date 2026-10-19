#pragma once
#include "Global.hpp"

namespace Lu{
    namespace Core{

    struct Material{
        glm::ivec4 texturesIds{-1, -1, -1, -1}; // x = albedo, y = normal, z = roughness, w = metallic
        glm::vec4 albedo{0.f, 0.f, 0.f, 1.f};
        glm::vec4 ambient{0.f, 0.f, 0.f, 1.f};
        glm::vec4 emission{0.f, 0.f, 0.f, 1.f};
        float roughness{0.5f};
        float metallic{0.0f};
        glm::vec2 _pad{0.f, 0.f};

        Material() = default;

        Material(const uint32_t textureIds[4], 
            glm::vec4 albedo, glm::vec4 ambient, glm::vec4 emission, 
            float roughness, float metallic):
            texturesIds(texturesIds),
            albedo(albedo),
            ambient(ambient),
            emission(emission),
            roughness(roughness),
            metallic(metallic){}
    };
}
}