#pragma once

#include "UICore.hpp"

namespace Lu {
    namespace UI {

        // Slider: track, filled part and knob are child entities created by the module.
        // value = 0..1, from left to right (vertical: from top to bottom). Set it from code (set / modified) or let the
        // user drag it. Node::size default: 200 x 24 (vertical: 24 x 200).
        struct Slider {
            float value{0.0f};
            Orientation orientation{Orientation::Horizontal};
            float step{0.0f};            // > 0: value snaps to multiples of step (e.g. 0.1)
            float trackThickness{6.0f};
            float knobSize{20.0f};
            Color fillColor{kUnset};     // Style::accent
            Color knobColor{kUnset};     // Style::knob
            std::function<void(flecs::entity, float)> onChange{};
        };
    }
}
