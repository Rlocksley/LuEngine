#pragma once

#include "Global.hpp"
#include "flecs.h"
#include "component/InputState.hpp"
#include "component/Transform2D.hpp"
#include "component/Transform2DGpu.hpp"
#include "component/Mesh2D.hpp"
#include "component/Text.hpp"

#include "Button.hpp"
#include "TextInput.hpp"

#include <algorithm>
#include <string>
#include <vector>

namespace Lu {
    namespace Module {

        // Non-core Flecs ECS UI module for 2D button and text input interactions.
        // Uses the InputState singleton component for mouse and keyboard input,
        // and updates Mesh2D and Text / Text2D components accordingly.
        struct UI {
            // Helper to resolve the entity holding Mesh2D
            static flecs::entity resolveMeshEntity(flecs::entity e, flecs::entity explicitMesh) {
                if (explicitMesh.is_valid() && explicitMesh.is_alive()) return explicitMesh;
                if (e.has<Lu::Component::Mesh2D>()) return e;
                flecs::entity found{};
                e.children([&](flecs::entity child) {
                    if (!found.is_valid() && child.has<Lu::Component::Mesh2D>()) {
                        found = child;
                    }
                });
                return found;
            }

            // Helper to resolve the entity holding Text
            static flecs::entity resolveTextEntity(flecs::entity e, flecs::entity explicitText) {
                if (explicitText.is_valid() && explicitText.is_alive()) return explicitText;
                if (e.has<Lu::Component::Text>()) return e;
                flecs::entity found{};
                e.children([&](flecs::entity child) {
                    if (!found.is_valid()) {
                        if (child.has<Lu::Component::Text>()) {
                            found = child;
                        } else {
                            // Check one level deeper (e.g. root -> label parent -> text)
                            child.children([&](flecs::entity grandchild) {
                                if (!found.is_valid() && grandchild.has<Lu::Component::Text>()) {
                                    found = grandchild;
                                }
                            });
                        }
                    }
                });
                return found;
            }

            explicit UI(flecs::world& world) {
                // Register components with Flecs
                world.component<Lu::UI::Button>("Lu::UI::Button");
                world.component<Lu::UI::TextInput>("Lu::UI::TextInput");

                // =========================================================================
                // 1. Button Interaction System
                // =========================================================================
                world.system<Lu::UI::Button, const Lu::Component::WorldTransform2D, const Component::InputState>("UIButtonSystem")
                    .kind(flecs::PreUpdate)
                    .each([](flecs::iter& it, size_t index, Lu::UI::Button& button, const Lu::Component::WorldTransform2D& transform, const Component::InputState& input) {
                        flecs::entity e = it.entity(index);

                        // Reset per-frame single pulse flags
                        button.wasClicked = false;
                        button.wasPressedThisFrame = false;

                        // Disabled state handling
                        if (!button.enabled) {
                            button.isHovered = false;
                            button.isPressed = false;
                            button.forcedHold = false;

                            if (button.autoUpdateMeshColor) {
                                flecs::entity meshEnt = resolveMeshEntity(e, button.meshEntity);
                                if (meshEnt.is_valid() && meshEnt.has<Lu::Component::Mesh2D>()) {
                                    auto& mesh = meshEnt.get_mut<Lu::Component::Mesh2D>();
                                    if (mesh.color != button.disabledColor) {
                                        mesh.color = button.disabledColor;
                                        meshEnt.modified<Lu::Component::Mesh2D>();
                                    }
                                }
                            }
                            if (button.autoUpdateTextColor) {
                                flecs::entity textEnt = resolveTextEntity(e, button.textEntity);
                                if (textEnt.is_valid() && textEnt.has<Lu::Component::Text>()) {
                                    auto& textComp = textEnt.get_mut<Lu::Component::Text>();
                                    if (textComp.color != button.textDisabledColor) {
                                        textComp.color = button.textDisabledColor;
                                        textEnt.modified<Lu::Component::Text>();
                                    }
                                }
                            }
                            return;
                        }

                        // Cursor hit test
                        const glm::vec2 cursorPos = input.getCursorPosition();
                        const bool hovered = button.contains(cursorPos, transform.position, transform.scale);
                        const bool wasHovered = button.isHovered;
                        button.isHovered = hovered;

                        if (hovered != wasHovered && button.onHover) {
                            button.onHover(e, hovered);
                        }

                        // Mouse interaction via InputState
                        const bool mouseLeftDown = input.isMouseButtonDown(MouseButton::mouseLeft);
                        const bool mouseLeftPressed = input.wasMouseButtonPressedThisFrame(MouseButton::mouseLeft);
                        const bool mouseLeftReleased = input.wasMouseButtonReleasedThisFrame(MouseButton::mouseLeft);

                        if (hovered && mouseLeftPressed) {
                            button.isPressed = true;
                            button.forcedHold = true;
                            button.wasPressedThisFrame = true;
                        }

                        if (button.forcedHold) {
                            if (mouseLeftReleased) {
                                button.isPressed = false;
                                button.forcedHold = false;
                                if (hovered) {
                                    button.wasClicked = true;
                                    if (button.onClick) {
                                        button.onClick(e);
                                    }
                                }
                            } else if (!mouseLeftDown) {
                                button.isPressed = false;
                                button.forcedHold = false;
                            } else {
                                // While holding mouse down, show pressed only if cursor is over the button
                                button.isPressed = hovered;
                            }
                        } else {
                            button.isPressed = false;
                        }

                        // Visual styling updates
                        glm::vec4 targetMeshColor = button.normalColor;
                        glm::vec4 targetTextColor = button.textNormalColor;

                        if (button.isPressed) {
                            targetMeshColor = button.pressedColor;
                            targetTextColor = button.textPressedColor;
                        } else if (button.isHovered) {
                            targetMeshColor = button.hoverColor;
                            targetTextColor = button.textHoverColor;
                        }

                        if (button.autoUpdateMeshColor) {
                            flecs::entity meshEnt = resolveMeshEntity(e, button.meshEntity);
                            if (meshEnt.is_valid() && meshEnt.has<Lu::Component::Mesh2D>()) {
                                auto& mesh = meshEnt.get_mut<Lu::Component::Mesh2D>();
                                if (mesh.color != targetMeshColor) {
                                    mesh.color = targetMeshColor;
                                    meshEnt.modified<Lu::Component::Mesh2D>();
                                }
                            }
                        }

                        if (button.autoUpdateTextColor) {
                            flecs::entity textEnt = resolveTextEntity(e, button.textEntity);
                            if (textEnt.is_valid() && textEnt.has<Lu::Component::Text>()) {
                                auto& textComp = textEnt.get_mut<Lu::Component::Text>();
                                if (textComp.color != targetTextColor) {
                                    textComp.color = targetTextColor;
                                    textEnt.modified<Lu::Component::Text>();
                                }
                            }
                        }
                    });

                // =========================================================================
                // 2. TextInput Interaction & Typing System
                // =========================================================================
                world.system<Lu::UI::TextInput, const Lu::Component::WorldTransform2D, const Component::InputState>("UITextInputSystem")
                    .kind(flecs::PreUpdate)
                    .each([](flecs::iter& it, size_t index, Lu::UI::TextInput& field, const Lu::Component::WorldTransform2D& transform, const Component::InputState& input) {
                        flecs::entity e = it.entity(index);
                        const float dt = it.delta_time();

                        // Reset per-frame single pulse flags
                        field.wasChanged = false;
                        field.wasSubmitted = false;

                        // Disabled state handling
                        if (!field.enabled) {
                            field.isHovered = false;
                            field.isFocused = false;
                            field.cursorVisible = false;

                            if (field.autoUpdateMeshColor) {
                                flecs::entity meshEnt = resolveMeshEntity(e, field.meshEntity);
                                if (meshEnt.is_valid() && meshEnt.has<Lu::Component::Mesh2D>()) {
                                    auto& mesh = meshEnt.get_mut<Lu::Component::Mesh2D>();
                                    if (mesh.color != field.disabledColor) {
                                        mesh.color = field.disabledColor;
                                        meshEnt.modified<Lu::Component::Mesh2D>();
                                    }
                                }
                            }
                            if (field.autoUpdateText) {
                                flecs::entity textEnt = resolveTextEntity(e, field.textEntity);
                                if (textEnt.is_valid() && textEnt.has<Lu::Component::Text>()) {
                                    auto& textComp = textEnt.get_mut<Lu::Component::Text>();
                                    const std::string displayStr = field.getDisplayText();
                                    if (textComp.text != displayStr || textComp.color != field.disabledTextColor) {
                                        textComp.text = displayStr;
                                        textComp.color = field.disabledTextColor;
                                        textEnt.modified<Lu::Component::Text>();
                                    }
                                }
                            }
                            return;
                        }

                        // Cursor hit test
                        const glm::vec2 cursorPos = input.getCursorPosition();
                        field.isHovered = field.contains(cursorPos, transform.position, transform.scale);

                        // Focus handling via mouse click
                        if (input.wasMouseButtonPressedThisFrame(MouseButton::mouseLeft)) {
                            const bool prevFocused = field.isFocused;
                            if (field.isHovered) {
                                field.isFocused = true;
                                field.cursorVisible = true;
                                field.blinkTimer = 0.0f;
                            } else {
                                field.isFocused = false;
                                field.cursorVisible = false;
                            }
                            if (prevFocused != field.isFocused && field.onFocus) {
                                field.onFocus(field.isFocused);
                            }
                        }

                        // Keyboard typing when focused
                        if (field.isFocused && !field.isReadOnly) {
                            const bool shift = input.isKeyDown(KeyCode::keyShift) ||
                                               input.keys[GLFW_KEY_RIGHT_SHIFT].down;

                            auto executeKey = [&](int k) {
                                if (k == GLFW_KEY_BACKSPACE) {
                                    field.deleteBackward();
                                    field.cursorVisible = true;
                                    field.blinkTimer = 0.0f;
                                    if (field.onChange) field.onChange(field.text);
                                } else if (k == GLFW_KEY_DELETE) {
                                    field.deleteForward();
                                    field.cursorVisible = true;
                                    field.blinkTimer = 0.0f;
                                    if (field.onChange) field.onChange(field.text);
                                } else if (k == GLFW_KEY_LEFT) {
                                    field.moveCursor(-1);
                                    field.cursorVisible = true;
                                    field.blinkTimer = 0.0f;
                                } else if (k == GLFW_KEY_RIGHT) {
                                    field.moveCursor(1);
                                    field.cursorVisible = true;
                                    field.blinkTimer = 0.0f;
                                } else if (k == GLFW_KEY_HOME) {
                                    field.cursorPosition = 0;
                                    field.cursorVisible = true;
                                    field.blinkTimer = 0.0f;
                                } else if (k == GLFW_KEY_END) {
                                    field.cursorPosition = field.text.size();
                                    field.cursorVisible = true;
                                    field.blinkTimer = 0.0f;
                                } else if (k == GLFW_KEY_ENTER || k == GLFW_KEY_KP_ENTER) {
                                    field.wasSubmitted = true;
                                    if (field.onSubmit) field.onSubmit(field.text);
                                } else if (k == GLFW_KEY_ESCAPE) {
                                    field.isFocused = false;
                                    field.cursorVisible = false;
                                    if (field.onFocus) field.onFocus(false);
                                } else {
                                    char c = '\0';
                                    if (k >= GLFW_KEY_A && k <= GLFW_KEY_Z) {
                                        c = shift ? static_cast<char>('A' + (k - GLFW_KEY_A))
                                                  : static_cast<char>('a' + (k - GLFW_KEY_A));
                                    } else if (k >= GLFW_KEY_0 && k <= GLFW_KEY_9) {
                                        if (shift) {
                                            constexpr char shiftDigits[] = {')', '!', '@', '#', '$', '%', '^', '&', '*', '('};
                                            c = shiftDigits[k - GLFW_KEY_0];
                                        } else {
                                            c = static_cast<char>('0' + (k - GLFW_KEY_0));
                                        }
                                    } else if (k >= GLFW_KEY_KP_0 && k <= GLFW_KEY_KP_9) {
                                        c = static_cast<char>('0' + (k - GLFW_KEY_KP_0));
                                    } else if (k == GLFW_KEY_SPACE) {
                                        c = ' ';
                                    } else if (k == GLFW_KEY_MINUS) {
                                        c = shift ? '_' : '-';
                                    } else if (k == GLFW_KEY_EQUAL) {
                                        c = shift ? '+' : '=';
                                    } else if (k == GLFW_KEY_LEFT_BRACKET) {
                                        c = shift ? '{' : '[';
                                    } else if (k == GLFW_KEY_RIGHT_BRACKET) {
                                        c = shift ? '}' : ']';
                                    } else if (k == GLFW_KEY_BACKSLASH) {
                                        c = shift ? '|' : '\\';
                                    } else if (k == GLFW_KEY_SEMICOLON) {
                                        c = shift ? ':' : ';';
                                    } else if (k == GLFW_KEY_APOSTROPHE) {
                                        c = shift ? '"' : '\'';
                                    } else if (k == GLFW_KEY_COMMA) {
                                        c = shift ? '<' : ',';
                                    } else if (k == GLFW_KEY_PERIOD) {
                                        c = shift ? '>' : '.';
                                    } else if (k == GLFW_KEY_SLASH) {
                                        c = shift ? '?' : '/';
                                    } else if (k == GLFW_KEY_GRAVE_ACCENT) {
                                        c = shift ? '~' : '`';
                                    } else if (k == GLFW_KEY_KP_DECIMAL) {
                                        c = '.';
                                    } else if (k == GLFW_KEY_KP_DIVIDE) {
                                        c = '/';
                                    } else if (k == GLFW_KEY_KP_MULTIPLY) {
                                        c = '*';
                                    } else if (k == GLFW_KEY_KP_SUBTRACT) {
                                        c = '-';
                                    } else if (k == GLFW_KEY_KP_ADD) {
                                        c = '+';
                                    }

                                    if (c != '\0') {
                                        field.insertChar(c);
                                        field.cursorVisible = true;
                                        field.blinkTimer = 0.0f;
                                        if (field.onChange) field.onChange(field.text);
                                    }
                                }
                            };

                            // Check all key presses this frame
                            int pressedKey = -1;
                            for (int k = GLFW_KEY_SPACE; k <= GLFW_KEY_LAST; ++k) {
                                if (input.keys[static_cast<size_t>(k)].pressedThisFrame) {
                                    pressedKey = k;
                                    break;
                                }
                            }

                            if (pressedKey != -1) {
                                field.lastKey = pressedKey;
                                field.repeatTimer = 0.0f;
                                executeKey(pressedKey);
                            } else if (field.lastKey != -1 && input.keys[static_cast<size_t>(field.lastKey)].down) {
                                field.repeatTimer += dt;
                                if (field.repeatTimer >= field.repeatDelay) {
                                    field.repeatTimer -= field.repeatRate;
                                    executeKey(field.lastKey);
                                }
                            } else if (field.lastKey != -1 && !input.keys[static_cast<size_t>(field.lastKey)].down) {
                                field.lastKey = -1;
                                field.repeatTimer = 0.0f;
                            }
                        }

                        // Caret blinking animation
                        if (field.isFocused) {
                            field.blinkTimer += dt;
                            if (field.blinkTimer >= field.blinkInterval) {
                                field.blinkTimer = 0.0f;
                                field.cursorVisible = !field.cursorVisible;
                            }
                        } else {
                            field.cursorVisible = false;
                        }

                        // Visual background updates for Mesh2D
                        glm::vec4 targetBgColor = field.normalColor;
                        if (field.isFocused) {
                            targetBgColor = field.focusedColor;
                        } else if (field.isHovered) {
                            targetBgColor = field.hoverColor;
                        }

                        if (field.autoUpdateMeshColor) {
                            flecs::entity meshEnt = resolveMeshEntity(e, field.meshEntity);
                            if (meshEnt.is_valid() && meshEnt.has<Lu::Component::Mesh2D>()) {
                                auto& mesh = meshEnt.get_mut<Lu::Component::Mesh2D>();
                                if (mesh.color != targetBgColor) {
                                    mesh.color = targetBgColor;
                                    meshEnt.modified<Lu::Component::Mesh2D>();
                                }
                            }
                        }

                        // Visual text updates for Text / Text2D
                        if (field.autoUpdateText) {
                            flecs::entity textEnt = resolveTextEntity(e, field.textEntity);
                            if (textEnt.is_valid() && textEnt.has<Lu::Component::Text>()) {
                                auto& textComp = textEnt.get_mut<Lu::Component::Text>();
                                const std::string displayStr = field.getDisplayText();
                                const glm::vec4 displayColor = (!field.isFocused && field.text.empty())
                                                             ? field.placeholderColor
                                                             : field.textColor;
                                if (textComp.text != displayStr || textComp.color != displayColor) {
                                    textComp.text = displayStr;
                                    textComp.color = displayColor;
                                    textEnt.modified<Lu::Component::Text>();
                                }
                            }
                        }
                    });
            }

            // =========================================================================
            // Factory helper to construct a complete Button entity hierarchy
            // =========================================================================
            static flecs::entity createButton(
                flecs::world& world,
                const char* name,
                const std::string& label,
                const Lu::Component::Transform2D& transform,
                const glm::vec2& size = {160.0f, 40.0f},
                const char* geometry = "Mesh2D::Button",
                const char* meshPipe = "Mesh2DPipe::Basic",
                const char* font = "Font::Main",
                const char* fontPipe = "FontPipe::Basic",
                float fontSize = 22.0f
            ) {
                // Root entity holds the Transform2D, Transform2DGpu, and Button component
                auto root = world.entity(name)
                    .set<Lu::Component::Transform2D>(transform)
                    .add<Lu::Component::Transform2DGpu>();

                Lu::UI::Button btn;
                btn.size = size;

                // Child 1: Mesh2D background
                auto meshChild = world.entity((std::string(name) + "::Mesh").c_str())
                    .set(flecs::Parent{root})
                    .set(Lu::Component::Mesh2D{
                        .mesh = world.lookup(geometry),
                        .pipeline = world.lookup(meshPipe),
                        .color = btn.normalColor
                    });
                btn.meshEntity = meshChild;

                // Child 2: Text2D label (placed at layer z+1 centered horizontally on the button)
                if (!label.empty()) {
                    const float labelX = size.x * 0.5f;
                    const float labelY = std::max(0.0f, (size.y - fontSize) * 0.5f);

                    auto labelParent = world.entity((std::string(name) + "::Label").c_str())
                        .set<Lu::Component::Transform2D>({
                            .position = transform.position + glm::vec2(labelX, labelY),
                            .positionZ = transform.positionZ + 1
                        })
                        .add<Lu::Component::Transform2DGpu>();

                    auto textChild = world.entity((std::string(name) + "::Text").c_str())
                        .set(flecs::Parent{labelParent})
                        .set(Lu::Component::Text{
                            .text = label,
                            .font = world.lookup(font),
                            .pipeline = world.lookup(fontPipe),
                            .color = btn.textNormalColor,
                            .size = fontSize,
                            .align = Lu::TextAlign::Center
                        });
                    btn.textEntity = textChild;
                }

                root.set<Lu::UI::Button>(btn);
                return root;
            }

            // =========================================================================
            // Factory helper to construct a complete TextInput entity hierarchy
            // =========================================================================
            static flecs::entity createTextInput(
                flecs::world& world,
                const char* name,
                const std::string& initialText,
                const std::string& placeholder,
                const Lu::Component::Transform2D& transform,
                const glm::vec2& size = {240.0f, 40.0f},
                const char* geometry = "Mesh2D::TextInput",
                const char* meshPipe = "Mesh2DPipe::Basic",
                const char* font = "Font::Main",
                const char* fontPipe = "FontPipe::Basic",
                float fontSize = 20.0f
            ) {
                // Root entity holds Transform2D, Transform2DGpu, and TextInput component
                auto root = world.entity(name)
                    .set<Lu::Component::Transform2D>(transform)
                    .add<Lu::Component::Transform2DGpu>();

                Lu::UI::TextInput field;
                field.size = size;
                field.placeholder = placeholder;
                if (!initialText.empty()) {
                    field.setText(initialText);
                }

                // Child 1: Mesh2D background
                auto meshChild = world.entity((std::string(name) + "::Mesh").c_str())
                    .set(flecs::Parent{root})
                    .set(Lu::Component::Mesh2D{
                        .mesh = world.lookup(geometry),
                        .pipeline = world.lookup(meshPipe),
                        .color = field.normalColor
                    });
                field.meshEntity = meshChild;

                // Child 2: Text2D content (placed at layer z+1 with padding inside the input box)
                const glm::vec2 padding = {12.0f, std::max(0.0f, (size.y - fontSize) * 0.5f)};
                auto textParent = world.entity((std::string(name) + "::TextParent").c_str())
                    .set<Lu::Component::Transform2D>({
                        .position = transform.position + padding,
                        .positionZ = transform.positionZ + 1
                    })
                    .add<Lu::Component::Transform2DGpu>();

                auto textChild = world.entity((std::string(name) + "::Text").c_str())
                    .set(flecs::Parent{textParent})
                    .set(Lu::Component::Text{
                        .text = field.getDisplayText(),
                        .font = world.lookup(font),
                        .pipeline = world.lookup(fontPipe),
                        .color = field.placeholderColor,
                        .size = fontSize,
                        .align = Lu::TextAlign::Left
                    });
                field.textEntity = textChild;

                root.set<Lu::UI::TextInput>(field);
                return root;
            }
        };

        using UIModule = UI;
    }
}
