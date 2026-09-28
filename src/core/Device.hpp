#pragma once

#include "Global.hpp"

namespace Lu
{
    namespace Core
    {
        inline std::vector<const char*> deviceExtensions
        {
            VK_KHR_SWAPCHAIN_EXTENSION_NAME,
            VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME
        };


        inline VkDevice vkDevice;

        void createDevice();
        void destroyDevice();
    }
}