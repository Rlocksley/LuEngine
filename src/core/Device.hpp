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

        inline PFN_vkCreateIndirectCommandsLayoutEXT vkCreateIndirectCommandsLayoutEXT;
        inline PFN_vkDestroyIndirectCommandsLayoutEXT vkDestroyIndirectCommandsLayoutEXT;
        inline PFN_vkGetGeneratedCommandsMemoryRequirementsEXT vkGetGeneratedCommandsMemoryRequirementsEXT;
        inline PFN_vkCmdExecuteGeneratedCommandsEXT vkCmdExecuteGeneratedCommandsEXT;

        void createDevice();
        void destroyDevice();
    }
}