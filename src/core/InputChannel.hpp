#pragma once

#include "Global.hpp"

namespace Lu
{
    namespace Core
    {
        enum class InputEventType
        {
            Key,
            MouseButton,
            Cursor,
            Scroll
        };

        enum class InputAction
        {
            Press,
            Release,
            Repeat
        };

        struct InputEvent
        {
            InputEventType type{};
            InputAction action{};
            int code{};
            double x{};
            double y{};
            double timestamp{};
        };

        class InputChannel
        {
        public:
            void pushKeyEvent(int key, InputAction action, double timestamp)
            {
                pushEvent({InputEventType::Key, action, key, 0.0, 0.0, timestamp});
            }

            void pushMouseButtonEvent(int button, InputAction action, double timestamp)
            {
                pushEvent({InputEventType::MouseButton, action, button, 0.0, 0.0, timestamp});
            }

            void pushCursorEvent(double x, double y, double timestamp)
            {
                pushEvent({InputEventType::Cursor, {}, 0, x, y, timestamp});
            }

            void pushScrollEvent(double xOffset, double yOffset, double timestamp)
            {
                pushEvent({InputEventType::Scroll, {}, 0, xOffset, yOffset, timestamp});
            }

            std::vector<InputEvent> drainEvents()
            {
                std::vector<InputEvent> events;
                {
                    std::lock_guard<std::mutex> lock(inputMutex);
                    events.swap(pendingEvents);
                }
                return events;
            }

        private:
            void pushEvent(const InputEvent& event)
            {
                std::lock_guard<std::mutex> lock(inputMutex);
                pendingEvents.push_back(event);
            }

            std::mutex inputMutex;
            std::vector<InputEvent> pendingEvents;
        };

        inline InputChannel& GetInputChannel()
        {
            static InputChannel inputChannel;
            return inputChannel;
        }
    }
}