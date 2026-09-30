#include "Core.hpp"
#include "Mutex.hpp"
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
                                    
}
}