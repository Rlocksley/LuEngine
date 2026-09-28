#include "DescriptorPool.hpp"
#include "Device.hpp"

namespace Lu
{
    namespace Core
    {

        void createDescriptorPool()
        {
            uint32_t i = 0;
            std::vector<VkDescriptorPoolSize> sizes(3);

            if(maxNumberUniformBuffers > 0)
            {
                sizes[i].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
                sizes[i].descriptorCount = maxNumberUniformBuffers;
                i++;
            }

            if(maxNumberStorageBuffers > 0)
            {
                sizes[i].type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
                sizes[i].descriptorCount = maxNumberStorageBuffers;
                i++;
            }

            if(maxNumberCombinedImageSamplers > 0)
            {
                sizes[i].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
                sizes[i].descriptorCount = maxNumberCombinedImageSamplers;
                i++;
            }

            VkDescriptorPoolCreateInfo createInfo;
            createInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
            createInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
            createInfo.poolSizeCount = i;
            createInfo.pPoolSizes = sizes.data();
            createInfo.maxSets = maxNumberDescriptorSets;
            createInfo.pNext = NULL;

            LU_CHECK_VULKAN
            (vkCreateDescriptorPool
            (vkDevice,
            &createInfo,
            NULL,
            &vkDescriptorPool),
            "createVkDescriptorPool",
            "vkCreateDescriptorPool")
            
            #ifdef LU_DEBUG
            LU_LOGI("DescriptorPool", "created", "")
            #endif            
        }

        void destroyDescriptorPool()
        {
            vkDestroyDescriptorPool
            (vkDevice,
            vkDescriptorPool,
            nullptr);

            vkDescriptorPool = VK_NULL_HANDLE;
        }

    }
}