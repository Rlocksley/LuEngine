#pragma once

#include "../component/Transform.hpp"

namespace Lu{
    namespace Core{
        struct Transform {
            glm::mat4 model{glm::mat4(1.0f)};
            glm::mat4 normal{glm::mat4(1.0f)};
            glm::vec4 scale{glm::vec4(1.0f)};
            glm::vec4 rotation{glm::vec4(0.0f, 0.0f, 0.0f, 1.0f)};
            glm::vec4 position{glm::vec4(0.0f)};

            Transform() = default;

            Transform(const Component::Transform& comp):
            model(comp.getMatrix()),
            scale(glm::vec4(comp.scale, 1.0f)),
            rotation(glm::vec4(comp.rotation.x, comp.rotation.y, comp.rotation.z, comp.rotation.w)),
            position(glm::vec4(comp.position, 1.0f))
            {
                normal = glm::transpose(glm::inverse(model));
            }
        };

        using TransformId = uint32_t; 
        using TransformEntity = flecs::entity_t;

        struct TransformInstance{ 
            Transform transform{};
            TransformId id{0};
            glm::vec3 _pad{0,0,0};


            TransformInstance() = default;

            TransformInstance(const Transform& transform, const TransformId id):
            transform(transform),
            id(id)
            {}
        };
    }
}