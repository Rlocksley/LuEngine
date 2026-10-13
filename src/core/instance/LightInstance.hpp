#pragma once
#include "Global.hpp"
#include "component/Light.hpp"

namespace Lu{
    namespace Core{
 
        struct Light{
            glm::vec4 color{0.0f};  // rgb = color, a = intensity
            glm::vec4 params{0.0f}; // x = radius, y = cos(innerCutoff), z = cos(outerCutoff), w = unused
            uint32_t type{0};   // 0 = point, 1 = spot, 2 = directional
            uint32_t transformId{0}; // ID of the transform associated with this light
            uint32_t _pad[2]{0, 0}; // Padding to ensure 16-byte alignment

            Light() = default;

            Light(const Component::Light& light, const uint32_t transformId):
                color(glm::vec4(light.color, light.intensity)),
                params(glm::vec4(light.radius, cos(light.innerCutoff), cos(light.outerCutoff), 0.0f)),
                type(static_cast<uint32_t>(light.type)),
                transformId(transformId)
                {}
        };



    }
}