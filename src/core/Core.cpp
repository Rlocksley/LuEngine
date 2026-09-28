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
        static int frameCounter = 1;
        int framerate = static_cast<int>(1.f / Time::deltaTime);
        if(frameCounter++%1000 == 0 || framerate < 100){
            std::cout << "Framerate: " << framerate << " FPS\n";
        };

        glfwPollEvents();
        
        Time::updateTime();

        return !glfwWindowShouldClose(pGLFWwindow);
    }

}
}