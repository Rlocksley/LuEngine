#include "Input.hpp"
#include "Window.hpp"
#include "InputChannel.hpp"

namespace Lu
{
    namespace Input
    {
        namespace
        {
            Core::InputAction toInputAction(int action)
            {
                switch (action)
                {
                case GLFW_PRESS:
                    return Core::InputAction::Press;
                case GLFW_RELEASE:
                    return Core::InputAction::Release;
                default:
                    return Core::InputAction::Repeat;
                }
            }

            void keyCallback(GLFWwindow*, int key, int, int action, int)
            {
                Core::GetInputChannel().pushKeyEvent(key, toInputAction(action), glfwGetTime());
            }

            void mouseButtonCallback(GLFWwindow*, int button, int action, int)
            {
                Core::GetInputChannel().pushMouseButtonEvent(button, toInputAction(action), glfwGetTime());
            }

            void cursorPositionCallback(GLFWwindow*, double x, double y)
            {
                Core::GetInputChannel().pushCursorEvent(x, y, glfwGetTime());
            }

            void scrollCallback(GLFWwindow*, double xOffset, double yOffset)
            {
                Core::GetInputChannel().pushScrollEvent(xOffset, yOffset, glfwGetTime());
            }
        }

        void createInput()
        {
            glfwSetKeyCallback(Core::pGLFWwindow, keyCallback);
            glfwSetMouseButtonCallback(Core::pGLFWwindow, mouseButtonCallback);
            glfwSetCursorPosCallback(Core::pGLFWwindow, cursorPositionCallback);
            glfwSetScrollCallback(Core::pGLFWwindow, scrollCallback);
        }
    }
}