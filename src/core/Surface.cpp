#include "Surface.hpp"
#include "Window.hpp"
#include "Instance.hpp"

namespace Lu
{
namespace Core
{
    void createSurface()
    {
        LU_CHECK_VULKAN
        (glfwCreateWindowSurface
        (vkInstance,
        pGLFWwindow,
        nullptr,
        &vkSurfaceKHR),
        "createSurface",
        "glfwCreateWindowSurface")

        #ifdef LU_DEBUG
        LU_LOGI("VkSurfaceKHR" , "created", "")
        #endif
    }

    void destroySurface()
    {
        vkDestroySurfaceKHR
        (vkInstance,
        vkSurfaceKHR,
        nullptr);
    }
}
}