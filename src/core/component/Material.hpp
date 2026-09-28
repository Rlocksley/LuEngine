#pragma once

#include "Global.hpp"
#include "flecs.h"

namespace Lu{
    namespace Component{

        struct Material{
            glm::ivec4 texturesIds{-1, -1, -1, -1}; // x = albedo, y = normal, z = roughness, w = metallic
            glm::vec4 albedo{1.f, 1.f, 1.f, 1.f};
            glm::vec4 ambient{0.f, 0.f, 0.f, 0.f};
            glm::vec4 emission{0.f,0.f,0.f,0.f};
            float roughness{0.f};
            float metallic{0.f};
        };

    }

    namespace Module{
        struct Material{
            Material(flecs::world& world){
                world.component<Component::Material>();
            }
        };
    }
}