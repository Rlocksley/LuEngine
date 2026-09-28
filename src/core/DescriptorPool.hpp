#pragma once

#include "Global.hpp"

namespace Lu
{
    namespace Core
    {
        inline uint32_t maxNumberUniformBuffers{20000};
        inline uint32_t maxNumberStorageBuffers{20000};
        inline uint32_t maxNumberCombinedImageSamplers{20000};
        inline uint32_t maxNumberDescriptorSets{20000};
        
        inline VkDescriptorPool vkDescriptorPool;

        void createDescriptorPool();
        void destroyDescriptorPool();
    }
}