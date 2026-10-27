#pragma once

#include "Global.hpp"
#include "flecs.h"
#include "component/Transform2D.hpp"
#include "component/Mesh2D.hpp"
#include "component/Text.hpp"
#include <functional>
#include <string>
#include <algorithm>

namespace Lu {

    namespace UI {

        using Text2D = Lu::Component::Text;
        using Mesh2D = Lu::Component::Mesh2D;

        // TextInput component for typing text in a 2D UI field.
        // Can be attached to the parent entity having Transform2D (which has Mesh2D and Text2D children),
        // or directly to the UI entity with meshEntity/textEntity explicitly set.
        struct TextInput {
            // Text content
            std::string text{""};
            std::string placeholder{"Enter text..."};
            size_t cursorPosition{0};
            size_t maxCharacters{0}; // 0 = unlimited

            // Bounds & layout (in screen pixels)
            glm::vec2 size{200.0f, 40.0f};
            glm::vec2 offset{0.0f, 0.0f}; // Top-left offset relative to Transform2D position

            // Visual feedback colors for Mesh2D (background box)
            glm::vec4 normalColor{0.14f, 0.15f, 0.18f, 1.0f};
            glm::vec4 hoverColor{0.18f, 0.20f, 0.26f, 1.0f};
            glm::vec4 focusedColor{0.20f, 0.25f, 0.36f, 1.0f};
            glm::vec4 disabledColor{0.10f, 0.10f, 0.12f, 0.6f};
            bool autoUpdateMeshColor{true};

            // Visual feedback colors for Text2D (typed text / placeholder)
            glm::vec4 textColor{1.0f, 1.0f, 1.0f, 1.0f};
            glm::vec4 placeholderColor{0.5f, 0.5f, 0.55f, 1.0f};
            glm::vec4 disabledTextColor{0.3f, 0.3f, 0.35f, 0.6f};
            bool autoUpdateText{true};

            // Caret / cursor blinking
            float blinkTimer{0.0f};
            float blinkInterval{0.5f};
            bool cursorVisible{true};
            char cursorChar{'|'};

            // Key repeat configuration
            int lastKey{-1};
            float repeatTimer{0.0f};
            float repeatDelay{0.4f};
            float repeatRate{0.04f};

            // Interaction state
            bool isHovered{false};
            bool isFocused{false};
            bool wasChanged{false};     // True for 1 frame when text content changes
            bool wasSubmitted{false};   // True for 1 frame when Enter is pressed
            bool enabled{true};
            bool isReadOnly{false};

            // Associated visual entities (Mesh2D, Text2D).
            // If invalid, the UI module will automatically discover child entities with Mesh2D or Text.
            flecs::entity meshEntity{};
            flecs::entity textEntity{};

            // Callbacks
            std::function<void(const std::string&)> onChange{};
            std::function<void(const std::string&)> onSubmit{};
            std::function<void(bool)> onFocus{};

            // Helper methods for text editing
            void insertChar(char c) {
                if (maxCharacters > 0 && text.size() >= maxCharacters) return;
                if (cursorPosition > text.size()) cursorPosition = text.size();
                text.insert(cursorPosition, 1, c);
                cursorPosition++;
                wasChanged = true;
            }

            void insertText(const std::string& str) {
                for (char c : str) insertChar(c);
            }

            void deleteBackward() {
                if (cursorPosition > 0 && !text.empty()) {
                    if (cursorPosition > text.size()) cursorPosition = text.size();
                    text.erase(cursorPosition - 1, 1);
                    cursorPosition--;
                    wasChanged = true;
                }
            }

            void deleteForward() {
                if (cursorPosition < text.size()) {
                    text.erase(cursorPosition, 1);
                    wasChanged = true;
                }
            }

            void moveCursor(int delta) {
                int newPos = static_cast<int>(cursorPosition) + delta;
                if (newPos < 0) newPos = 0;
                if (newPos > static_cast<int>(text.size())) newPos = static_cast<int>(text.size());
                cursorPosition = static_cast<size_t>(newPos);
            }

            void setText(const std::string& newText) {
                text = newText;
                if (maxCharacters > 0 && text.size() > maxCharacters) text.resize(maxCharacters);
                cursorPosition = text.size();
                wasChanged = true;
            }

            void clear() {
                text.clear();
                cursorPosition = 0;
                wasChanged = true;
            }

            std::string getDisplayText() const {
                if (!isFocused && text.empty()) {
                    return placeholder.empty() ? " " : placeholder;
                }
                if (isFocused) {
                    std::string s = text;
                    const size_t pos = std::min(cursorPosition, s.size());
                    // Use space when cursor is off to keep constant length and avoid GPU buffer churn
                    s.insert(pos, 1, cursorVisible ? cursorChar : ' ');
                    return s;
                }
                return text.empty() ? " " : text;
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
        using TextInput = Lu::UI::TextInput;
        using TextInputField = Lu::UI::TextInput;
    }
}
