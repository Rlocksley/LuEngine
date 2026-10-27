#pragma once

#include "Global.hpp"
#include "flecs.h"
#include "component/Transform2D.hpp"
#include "component/Mesh2D.hpp"
#include "component/Text.hpp"
#include <functional>
#include <string>

namespace Lu {

    namespace Component {
        // Text2D alias for Component::Text matching 2D component naming convention
        using Text2D = Text;
    }

    namespace UI {

        using Text2D = Lu::Component::Text;
        using Mesh2D = Lu::Component::Mesh2D;

        // Button component for 2D UI button press and hover interactions.
        // Can be attached to the parent entity having Transform2D (which has Mesh2D and optional Text2D children),
        // or directly to the UI entity with meshEntity/textEntity explicitly set.
        struct Button {
            // Button bounding size and offset (in screen pixels)
            glm::vec2 size{120.0f, 40.0f};
            glm::vec2 offset{0.0f, 0.0f}; // Top-left offset relative to Transform2D position

            // Visual feedback colors for Mesh2D
            glm::vec4 normalColor{0.25f, 0.28f, 0.35f, 1.0f};
            glm::vec4 hoverColor{0.35f, 0.40f, 0.50f, 1.0f};
            glm::vec4 pressedColor{0.18f, 0.20f, 0.25f, 1.0f};
            glm::vec4 disabledColor{0.15f, 0.15f, 0.15f, 0.6f};
            bool autoUpdateMeshColor{true};

            // Visual feedback colors for Text2D (if the button has a text label)
            glm::vec4 textNormalColor{1.0f, 1.0f, 1.0f, 1.0f};
            glm::vec4 textHoverColor{1.0f, 1.0f, 1.0f, 1.0f};
            glm::vec4 textPressedColor{0.8f, 0.8f, 0.8f, 1.0f};
            glm::vec4 textDisabledColor{0.5f, 0.5f, 0.5f, 0.6f};
            bool autoUpdateTextColor{true};

            // Interaction state
            bool isHovered{false};
            bool isPressed{false};
            bool wasClicked{false};              // True for 1 frame when click completes (mouse pressed & released on button)
            bool wasPressedThisFrame{false};      // True on frame press starts
            bool forcedHold{false};               // Tracking internal click state
            bool enabled{true};

            // Optional explicit entity references for visual representation (Mesh2D / Text2D).
            // If invalid, the UI module will automatically discover child entities with Mesh2D or Text.
            flecs::entity meshEntity{};
            flecs::entity textEntity{};

            // Optional callbacks
            std::function<void(flecs::entity)> onClick{};
            std::function<void(flecs::entity, bool)> onHover{};

            // Helper to update the text label directly
            void setLabel(const std::string& newText) {
                if (textEntity.is_valid() && textEntity.is_alive()) {
                    auto& t = textEntity.get_mut<Lu::Component::Text>();
                    t.text = newText;
                    textEntity.modified<Lu::Component::Text>();
                }
            }

            // Helper to get the current label string
            std::string getLabel() const {
                if (textEntity.is_valid() && textEntity.is_alive() && textEntity.has<Lu::Component::Text>()) {
                    return textEntity.get<Lu::Component::Text>().text;
                }
                return "";
            }

            // Hit test helper
            bool contains(const glm::vec2& point, const glm::vec2& worldPos, const glm::vec2& scale = glm::vec2(1.0f)) const {
                const glm::vec2 minPos = worldPos + offset * scale;
                const glm::vec2 maxPos = minPos + size * scale;
                return (point.x >= minPos.x && point.x <= maxPos.x &&
                        point.y >= minPos.y && point.y <= maxPos.y);
            }
        };
    }

    namespace Component {
        // Aliases to avoid name collision with mouse Component::Button from InputState.hpp
        using UIButton = Lu::UI::Button;
        using Button2D = Lu::UI::Button;
    }
}
