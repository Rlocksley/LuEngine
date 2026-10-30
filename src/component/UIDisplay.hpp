#pragma once

#include "UICore.hpp"

namespace Lu {
    namespace UI {

        // Filled rectangle with a border. Children are placed on top of it.
        struct Panel {
            Color color{kUnset};         // fill (Style::panel)
            Color borderColor{kUnset};   // Style::panelBorder
            float borderWidth{-1.0f};    // < 0: Style::borderWidth, 0: no border
        };

        // Text. Node::size follows the text (estimated) when autoSize is on.
        struct Label {
            std::string text;
            Color color{kUnset};         // Style::text
            float fontSize{-1.0f};       // < 0: Style::fontSize
            TextAlign align{TextAlign::Left};   // Center / Right: the text is aligned inside Node::size
            bool verticalCenter{false};  // center the first line vertically inside Node::size
            float maxWidth{0.0f};        // > 0: word wrap
            bool autoSize{true};
        };

        // Rectangle with a texture (icon, portrait, ...). Texture = App::registerTexture entity.
        struct Image {
            flecs::entity texture{};
            Color tint{1.0f, 1.0f, 1.0f, 1.0f};
            glm::vec4 uvRect{0.0f, 0.0f, 1.0f, 1.0f};
        };

        // Progress / health / production bar. value = 0..1.
        struct ProgressBar {
            float value{0.0f};
            Orientation orientation{Orientation::Horizontal};
            Color fillColor{kUnset};     // Style::accent
            Color trackColor{kUnset};    // Style::track
            bool showText{false};        // "NN%" in the middle
        };

        // Text popup that follows the cursor after the node was hovered for `delay` seconds.
        struct Tooltip {
            std::string text;
            float delay{0.45f};
            float maxWidth{280.0f};
        };
    }
}
