#pragma once

#include "UICore.hpp"

namespace Lu {
    namespace UI {

        // Push button. Colors / state (hover, pressed, disabled) come from the Interact component of the same entity
        // (Interact::enabled = false disables it); the click callback can also be set on Interact::onClick.
        // Node::size default: 160 x 40.
        struct Button {
            std::string text;
            ColorSet colors{};           // Style::button
            Color textColor{kUnset};     // Style::text
            float fontSize{-1.0f};       // Style::fontSize
            bool selected{false};        // accent colored border (tabs, toggle buttons)
            std::function<void(flecs::entity)> onClick{};
        };
    }

    namespace Component {
        // Aliases to avoid name collision with mouse Component::Button from InputState.hpp
        using UIButton = Lu::UI::Button;
    }
}
