#pragma once

#include "../component/Transform2D.hpp"

namespace Lu{
    namespace Core{
        // GPU side of Component::Transform2D (std430, 112 bytes; shaders: transform2d_transfer.comp, font_*.vert/comp)
        struct Transform2D {
            glm::mat4 model{glm::mat4(1.0f)};       // translate * rotate * scale in pixel space
            glm::vec2 position{0.0f, 0.0f};
            glm::vec2 scale{1.0f, 1.0f};
            glm::vec4 rotation{0.0f, 0.0f, 0.0f, 1.0f};   // quaternion x,y,z,w
            uint32_t positionZ{0};
            uint32_t _pad0{0};
            uint32_t _pad1{0};
            uint32_t _pad2{0};

            Transform2D() = default;

            Transform2D(const Component::Transform2D& comp):
            model(comp.getMatrix()),
            position(comp.position),
            scale(comp.scale),
            rotation(comp.rotation.x, comp.rotation.y, comp.rotation.z, comp.rotation.w),
            positionZ(comp.positionZ < TRANSFORM_2D_LAYER_COUNT_CLAMP ? comp.positionZ : TRANSFORM_2D_LAYER_COUNT_CLAMP)
            {}

            static constexpr uint32_t TRANSFORM_2D_LAYER_COUNT_CLAMP = 65535;
        };

        using Transform2DId = uint32_t;
        using Transform2DEntity = flecs::entity_t;

        struct Transform2DInstance{
            Transform2D transform{};
            Transform2DId id{0};
            uint32_t _pad[3]{0, 0, 0};

            Transform2DInstance() = default;

            Transform2DInstance(const Transform2D& transform, const Transform2DId id):
            transform(transform),
            id(id)
            {}
        };

        static_assert(sizeof(Transform2D) == 112);
        static_assert(offsetof(Transform2DInstance, id) == 112);
        static_assert(sizeof(Transform2DInstance) == 128);
    }
}
