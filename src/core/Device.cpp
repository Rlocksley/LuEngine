#include "Device.hpp"
#include "PhysicalDevice.hpp"

namespace Lu
{
    namespace Core
    {

        void createDevice()
        {
            float priority = 1.f;
            VkDeviceQueueCreateInfo queueCreateInfo{};
            queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
            queueCreateInfo.queueFamilyIndex = queueFamilyIndex;
            queueCreateInfo.queueCount = 1;
            queueCreateInfo.pQueuePriorities = &priority;


            VkPhysicalDeviceMaintenance5FeaturesKHR maintenance5Features{};
            maintenance5Features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MAINTENANCE_5_FEATURES_KHR;
            maintenance5Features.maintenance5 = VK_TRUE;

            VkPhysicalDeviceDeviceGeneratedCommandsFeaturesEXT dgcFeatures{};
            dgcFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DEVICE_GENERATED_COMMANDS_FEATURES_EXT;
            dgcFeatures.deviceGeneratedCommands = VK_TRUE;
            dgcFeatures.pNext = &maintenance5Features;

            VkPhysicalDeviceDynamicRenderingFeatures dynamicRenderingFeatures{};
            dynamicRenderingFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_FEATURES;
            dynamicRenderingFeatures.dynamicRendering = VK_TRUE;
            dynamicRenderingFeatures.pNext = &dgcFeatures;

            VkPhysicalDeviceVulkan12Features vulkan12Features{};
            vulkan12Features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
            vulkan12Features.runtimeDescriptorArray = VK_TRUE;
            vulkan12Features.shaderSampledImageArrayNonUniformIndexing = VK_TRUE;
            vulkan12Features.shaderStorageBufferArrayNonUniformIndexing = VK_TRUE;
            vulkan12Features.drawIndirectCount = VK_TRUE;
            vulkan12Features.bufferDeviceAddress = VK_TRUE;
            vulkan12Features.pNext = &dynamicRenderingFeatures;
            
            // Enable Vulkan 1.1 features (multiview is a promoted feature, so enable it here)
            VkPhysicalDeviceVulkan11Features vulkan11Features{};
            vulkan11Features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES;
            vulkan11Features.shaderDrawParameters = VK_TRUE;
            vulkan11Features.multiview = VK_TRUE; // Enable multiview (promoted to core in Vulkan 1.1)
            vulkan11Features.pNext = &vulkan12Features;

            VkDeviceCreateInfo createInfo{};
            createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
            createInfo.enabledLayerCount = 0;
            createInfo.ppEnabledLayerNames = nullptr;
            createInfo.queueCreateInfoCount = 1;
            createInfo.pQueueCreateInfos = &queueCreateInfo;
            createInfo.pEnabledFeatures = nullptr;
            createInfo.enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.size()); 
            createInfo.ppEnabledExtensionNames = deviceExtensions.data();

            VkPhysicalDeviceFeatures2 enabledFeatures2{};
            enabledFeatures2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
            enabledFeatures2.features.samplerAnisotropy = VK_TRUE;
            enabledFeatures2.features.multiDrawIndirect = VK_TRUE;
            enabledFeatures2.features.shaderInt64 = VK_TRUE;
            enabledFeatures2.pNext = &vulkan11Features;
            createInfo.pNext = &enabledFeatures2;

            LU_CHECK_VULKAN
            (vkCreateDevice
            (vkPhysicalDevice,
            &createInfo,
            nullptr,
            &vkDevice),
            "createDevice",
            "vkCreateDevice")

            #ifdef LU_DEBUG
            LU_LOGI("Device", "created", "")
            #endif
        }

        void destroyDevice()
        {
            vkDestroyDevice
            (vkDevice,
            nullptr);

            vkDevice = VK_NULL_HANDLE;
        }
    }
}