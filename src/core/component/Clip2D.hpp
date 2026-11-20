#pragma once

#include "Global.hpp"

namespace Lu{
    namespace Component{
        struct Clip2D{
            glm::vec2 position{0.f, 0.f}; // top-left corner
            glm::vec2 size{0.f, 0.f};     // width, height
        };
    }
}