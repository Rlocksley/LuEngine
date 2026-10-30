#pragma once

#include "UICore.hpp"

namespace Lu {
    namespace UI {

        // Drop-down list. While open the options are Button entities (children of this entity) created by the module.
        // Node::size default: 220 x 36 (= size of the closed header and of every option).
        struct Dropdown {
            std::vector<std::string> options;
            int selected{-1};            // -1 = nothing, shows `placeholder`
            bool open{false};
            std::string placeholder{"Select..."};
            float fontSize{-1.0f};
            std::function<void(flecs::entity, int)> onChange{};
        };
    }
}
