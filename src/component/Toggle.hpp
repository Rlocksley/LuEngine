#pragma once

#include "UICore.hpp"

namespace Lu {
    namespace UI {

        // Check box, or radio button (radio = true). Toggles with the same non-zero `group` and the same parent are
        // exclusive: checking one unchecks the others. Node::size default: 200 x 28.
        struct Toggle {
            bool checked{false};
            std::string text;
            bool radio{false};
            uint32_t group{0};
            ColorSet colors{};           // box colors (Style::input)
            Color textColor{kUnset};
            float fontSize{-1.0f};
            std::function<void(flecs::entity, bool)> onChange{};
        };
    }
}
