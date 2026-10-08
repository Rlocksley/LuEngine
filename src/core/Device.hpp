#pragma once

#include "Global.hpp"

namespace Lu
{
    namespace Core
    {
        inline std::vector<const char*> deviceExtensions
        {
            VK_KHR_SWAPCHAIN_EXTENSION_NAME,
            VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME,
            VK_EXT_DEVICE_GENERATED_COMMANDS_EXTENSION_NAME, 
            VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME,     
            VK_KHR_MAINTENANCE_5_EXTENSION_NAME              
        };


        inline VkDevice vkDevice;

        PFN_vkCreateIndirectCommandsLayoutEXT vkCreateIndirectCommandsLayoutEXT;
        PFN_vkDestroyIndirectCommandsLayoutEXT vkDestroyIndirectCommandsLayoutEXT;
        PFN_vkGetGeneratedCommandsMemoryRequirementsEXT vkGetGeneratedCommandsMemoryRequirementsEXT;
        PFN_vkCmdExecuteGeneratedCommandsEXT vkCmdExecuteGeneratedCommandsEXT;

        void createDevice();
        void destroyDevice();
    }
}