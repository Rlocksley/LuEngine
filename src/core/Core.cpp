#include "Core.hpp"
#include "Image.hpp"

namespace Lu
{
namespace Core
{
    void createCore()
    {
        createWindow();
        Time::createTime();
        Input::createInput();

        createInstance();
        createSurface();
        pickPhysicalDevice();
        createDevice();
        getQueue();
        createAllocator();
        createDescriptorPool();

       
        createSwapchain();
        createFramebuffers();

    }

    void destroyCore()
    {
        vkDeviceWaitIdle(vkDevice);
    
        destroyFramebuffers();
        destroySwapchain();

        destroyDescriptorPool();
        destroyAllocator();
        destroyDevice();
        destroySurface();
        destroyInstance();

        destroyWindow();
    }

    bool updateCore()
    {
        glfwPollEvents();
        
        Time::updateTime();

        return !glfwWindowShouldClose(pGLFWwindow);
    }

    void recreateSwapchain()
    {
        int width = 0;
        int height = 0;
        glfwGetFramebufferSize(pGLFWwindow, &width, &height);
        while(width == 0 || height == 0)
        {
            glfwWaitEvents();
            glfwGetFramebufferSize(pGLFWwindow, &width, &height);
            if(glfwWindowShouldClose(pGLFWwindow))
            {
                return;
            }
        }

        windowWidth = static_cast<uint32_t>(width);
        windowHeight = static_cast<uint32_t>(height);

        LU_CHECK_VULKAN(vkDeviceWaitIdle(vkDevice), "recreateSwapchain", "vkDeviceWaitIdle")
        destroyFramebuffers();
        destroySwapchain();
        refreshSurfaceState();
        createSwapchain();
        createFramebuffers();
    }
                                    
}
}
