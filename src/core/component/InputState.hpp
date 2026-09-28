#pragma once

#include "Global.hpp"
#include "flecs.h"
#include "../core/Input.hpp"
#include "InputChannel.hpp"

namespace Lu
{
    enum class KeyCode : int
    {
        keyA = GLFW_KEY_A,
        keyB = GLFW_KEY_B,
        keyC = GLFW_KEY_C,
        keyD = GLFW_KEY_D,
        keyE = GLFW_KEY_E,
        keyF = GLFW_KEY_F,
        keyG = GLFW_KEY_G,
        keyH = GLFW_KEY_H,
        keyI = GLFW_KEY_I,
        keyJ = GLFW_KEY_J,
        keyK = GLFW_KEY_K,
        keyL = GLFW_KEY_L,
        keyM = GLFW_KEY_M,
        keyN = GLFW_KEY_N,
        keyO = GLFW_KEY_O,
        keyP = GLFW_KEY_P,
        keyQ = GLFW_KEY_Q,
        keyR = GLFW_KEY_R,
        keyS = GLFW_KEY_S,
        keyT = GLFW_KEY_T,
        keyU = GLFW_KEY_U,
        keyV = GLFW_KEY_V,
        keyW = GLFW_KEY_W,
        keyX = GLFW_KEY_X,
        keyY = GLFW_KEY_Y,
        keyZ = GLFW_KEY_Z,
        key0 = GLFW_KEY_0,
        key1 = GLFW_KEY_1,
        key2 = GLFW_KEY_2,
        key3 = GLFW_KEY_3,
        key4 = GLFW_KEY_4,
        key5 = GLFW_KEY_5,
        key6 = GLFW_KEY_6,
        key7 = GLFW_KEY_7,
        key8 = GLFW_KEY_8,
        key9 = GLFW_KEY_9,
        keyLeft = GLFW_KEY_LEFT,
        keyRight = GLFW_KEY_RIGHT,
        keyUp = GLFW_KEY_UP,
        keyDown = GLFW_KEY_DOWN,
        keySpace = GLFW_KEY_SPACE,
        keyEsc = GLFW_KEY_ESCAPE,
        keyShift = GLFW_KEY_LEFT_SHIFT,
        keyEnter = GLFW_KEY_ENTER,
        keyBackspace = GLFW_KEY_BACKSPACE,
        keyTab = GLFW_KEY_TAB
    };

    enum class MouseButton : int
    {
        mouseLeft = GLFW_MOUSE_BUTTON_LEFT,
        mouseRight = GLFW_MOUSE_BUTTON_RIGHT,
        mouseMiddle = GLFW_MOUSE_BUTTON_MIDDLE
    };

    namespace Component
    {
        struct Key
        {
            bool down{false};
            bool pressedThisFrame{false};
            bool releasedThisFrame{false};
            double pressStartedAt{0.0};
            double lastHoldDuration{0.0};
        };

        struct Button
        {
            bool down{false};
            bool pressedThisFrame{false};
            bool releasedThisFrame{false};
            double pressStartedAt{0.0};
            double lastHoldDuration{0.0};
        };

        struct Cursor
        {
            glm::vec2 position{0.0f};
            glm::vec2 deltaPosition{0.0f};
        };

        struct Scroll
        {
            glm::vec2 scroll{0.0f};
            glm::vec2 deltaScroll{0.0f};
        };

        struct InputState
        {
            std::array<Key, GLFW_KEY_LAST + 1> keys{};
            std::array<Button, GLFW_MOUSE_BUTTON_LAST + 1> mouseButtons{};
            Cursor cursor{};
            Scroll scroll{};

            bool isKeyDown(KeyCode key) const
            {
                return keys[static_cast<std::size_t>(key)].down;
            }

            bool wasKeyPressedThisFrame(KeyCode key) const
            {
                return keys[static_cast<std::size_t>(key)].pressedThisFrame;
            }

            bool wasKeyReleasedThisFrame(KeyCode key) const
            {
                return keys[static_cast<std::size_t>(key)].releasedThisFrame;
            }

            double getKeyPressStartedAt(KeyCode key) const
            {
                return keys[static_cast<std::size_t>(key)].pressStartedAt;
            }

            double getKeyLastHoldDuration(KeyCode key) const
            {
                return keys[static_cast<std::size_t>(key)].lastHoldDuration;
            }

            bool isMouseButtonDown(MouseButton button) const
            {
                return mouseButtons[static_cast<std::size_t>(button)].down;
            }

            bool wasMouseButtonPressedThisFrame(MouseButton button) const
            {
                return mouseButtons[static_cast<std::size_t>(button)].pressedThisFrame;
            }

            bool wasMouseButtonReleasedThisFrame(MouseButton button) const
            {
                return mouseButtons[static_cast<std::size_t>(button)].releasedThisFrame;
            }

            double getMouseButtonPressStartedAt(MouseButton button) const
            {
                return mouseButtons[static_cast<std::size_t>(button)].pressStartedAt;
            }

            double getMouseButtonLastHoldDuration(MouseButton button) const
            {
                return mouseButtons[static_cast<std::size_t>(button)].lastHoldDuration;
            }

            glm::vec2 getCursorPosition(){
                return cursor.position;
            }

            glm::vec2 getCursorDeltaPosition(){
                return cursor.deltaPosition;
            }
        };
    }

    namespace Module
    {
        struct InputState
        {
            explicit InputState(flecs::world& world)
            {
                world.component<Component::InputState>().add(flecs::Singleton);
                world.set<Component::InputState>({});

                world.system<Component::InputState>()
                    .kind(flecs::OnLoad)
                    .each([](Component::InputState& input)
                    {
                        for (auto& key : input.keys)
                        {
                            key.pressedThisFrame = false;
                            key.releasedThisFrame = false;
                        }
                        for (auto& button : input.mouseButtons)
                        {
                            button.pressedThisFrame = false;
                            button.releasedThisFrame = false;
                        }
                        input.cursor.deltaPosition = glm::vec2(0.0f);
                        input.scroll.deltaScroll = glm::vec2(0.0f);

                        for (const auto& event : Core::GetInputChannel().drainEvents())
                        {
                            switch (event.type)
                            {
                            case Core::InputEventType::Key:
                                if (event.code < 0 || event.code > GLFW_KEY_LAST)
                                {
                                    break;
                                }
                                if (event.action == Core::InputAction::Press)
                                {
                                    auto& key = input.keys[static_cast<std::size_t>(event.code)];
                                    if (!key.down)
                                    {
                                        key.down = true;
                                        key.pressedThisFrame = true;
                                        key.pressStartedAt = event.timestamp;
                                    }
                                }
                                else if (event.action == Core::InputAction::Release)
                                {
                                    auto& key = input.keys[static_cast<std::size_t>(event.code)];
                                    if (key.down)
                                    {
                                        key.down = false;
                                        key.releasedThisFrame = true;
                                        key.lastHoldDuration = std::max(0.0, event.timestamp - key.pressStartedAt);
                                    }
                                }
                                break;
                            case Core::InputEventType::MouseButton:
                                if (event.code < 0 || event.code > GLFW_MOUSE_BUTTON_LAST)
                                {
                                    break;
                                }
                                if (event.action == Core::InputAction::Press)
                                {
                                    auto& button = input.mouseButtons[static_cast<std::size_t>(event.code)];
                                    if (!button.down)
                                    {
                                        button.down = true;
                                        button.pressedThisFrame = true;
                                        button.pressStartedAt = event.timestamp;
                                    }
                                }
                                else if (event.action == Core::InputAction::Release)
                                {
                                    auto& button = input.mouseButtons[static_cast<std::size_t>(event.code)];
                                    if (button.down)
                                    {
                                        button.down = false;
                                        button.releasedThisFrame = true;
                                        button.lastHoldDuration = std::max(0.0, event.timestamp - button.pressStartedAt);
                                    }
                                }
                                break;
                            case Core::InputEventType::Cursor:
                            {
                                const glm::vec2 position(static_cast<float>(event.x), static_cast<float>(event.y));
                                input.cursor.deltaPosition += position - input.cursor.position;
                                input.cursor.position = position;
                                break;
                            }
                            case Core::InputEventType::Scroll:
                            {
                                const glm::vec2 delta(static_cast<float>(event.x), static_cast<float>(event.y));
                                input.scroll.scroll += delta;
                                input.scroll.deltaScroll += delta;
                                break;
                            }
                            }
                        }
                    });
            }
        };
    }
}