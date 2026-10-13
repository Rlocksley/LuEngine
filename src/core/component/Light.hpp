#pragma once
#include "flecs.h"
#include "component/Transform.hpp"
#include "component/TransformGpu.hpp"

namespace Lu{
    enum class LightType : uint32_t{
        Point = 0,
        Spot = 1,
        Directional = 2
    };

    namespace Component{
        struct Light {
            LightType type;
            glm::vec3 color{1.0f};
            float intensity{1.0f};
            float radius{10.0f};
            float innerCutoff{3.14f / 8.0f};
            float outerCutoff{3.14f / 4.0f};
        };

        struct LightGpuDirty{};
    }
}
