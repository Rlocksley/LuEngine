#pragma once

#include "UICore.hpp"

namespace Lu {
    namespace UI {

        // Single line text field. A click gives it the keyboard focus (Pointer::focused). Node::size default: 240 x 40.
        // There is no text measuring on the ECS side: the caret is a character inside the text and a text that is
        // wider than the field shows its tail (width of a character is estimated).
        struct TextInput {
            std::string text;
            std::string placeholder;
            size_t cursorPosition{0};
            size_t maxCharacters{0};     // 0 = unlimited
            bool readOnly{false};
            bool password{false};        // shows '*'
            ColorSet colors{};           // Style::input (pressed = focused)
            Color textColor{kUnset};
            Color placeholderColor{kUnset};
            float fontSize{-1.0f};

            std::function<void(flecs::entity, const std::string&)> onChange{};
            std::function<void(flecs::entity, const std::string&)> onSubmit{};

            // state, written by the module
            bool focused{false};
            bool cursorVisible{true};
            float blinkTimer{0.0f};
            int lastKey{-1};
            float repeatTimer{0.0f};
        };
    }
}
