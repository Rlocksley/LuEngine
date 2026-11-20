#pragma once

#include "Global.hpp"
#include "component/Clip2D.hpp"

namespace Lu::Core {
    using Clip2DId = uint32_t;
    struct Clip2D {
        glm::vec2 position{0.f};
        glm::vec2 size{0.f};
        Clip2D() = default;
        explicit Clip2D(const Component::Clip2D& c) : position(c.position), size(c.size) {}
    };
    struct Clip2DInstance {
        Clip2D clip{};
        Clip2DId id{0};
        uint32_t pad[3]{};
        Clip2DInstance() = default;
        Clip2DInstance(const Clip2D& c, Clip2DId i) : clip(c), id(i) {}
    };
    static_assert(sizeof(Clip2D) == 16);
    static_assert(sizeof(Clip2DInstance) == 32);
}
