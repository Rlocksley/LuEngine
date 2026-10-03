#include "Window.hpp"

namespace Lu
{
    namespace Core
    {

        void createWindow()
        {
             glfwInit();
        
            glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
            
            if(fullscreen)
            {
                GLFWmonitor* monitor = glfwGetPrimaryMonitor();
                const GLFWvidmode* mode = monitor ? glfwGetVideoMode(monitor) : nullptr;

                if(mode == nullptr)
                {
                    LU_LOGE("createWindow", "glfwGetVideoMode", "failed to get primary monitor video mode")
                }

                windowWidth = static_cast<uint32_t>(mode->width);
                windowHeight = static_cast<uint32_t>(mode->height);

                if((pGLFWwindow = glfwCreateWindow
                (mode->width, mode->height, windowTitle.data(), monitor, nullptr)) == nullptr)
                {
                    LU_LOGE("createWindow", "glfwCreateWindow", "failed")
                }
            }
            else
            {
                glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

                if((pGLFWwindow = glfwCreateWindow
                (static_cast<int>(windowWidth), static_cast<int>(windowHeight), windowTitle.data(), nullptr, nullptr)) == nullptr)
                {
                    LU_LOGE("createWindow", "glfwCreateWindow", "failed")
                }
            }

            glfwSetInputMode(pGLFWwindow, GLFW_STICKY_KEYS, GLFW_TRUE);

            #ifdef LU_DEBUG
            LU_LOGI("Window", "created", "")
            #endif
        }
    

        void destroyWindow()
        {
            glfwDestroyWindow(pGLFWwindow);
        }

    }
}
