#include "PhysicalDevice.hpp"
#include "Device.hpp"
#include "Window.hpp"
#include "Instance.hpp"
#include "Surface.hpp"

namespace Lu
{
    namespace Core
    {
        bool isComputeGraphicsPresent(VkPhysicalDevice device)
        {
            uint32_t numberPropertiess;
            vkGetPhysicalDeviceQueueFamilyProperties
            (device,&numberPropertiess, nullptr);
            
            if(numberPropertiess == 0)
            {
                return false;
            }

            std::vector<VkQueueFamilyProperties> propertiess(numberPropertiess);
            vkGetPhysicalDeviceQueueFamilyProperties
            (device,
            &numberPropertiess, propertiess.data());

            for(uint32_t i = 0; i < numberPropertiess; i++)
            {
                bool compute = false;
                bool graphics = false;
                bool present = false;
                
                if(propertiess[i].queueFlags & VK_QUEUE_COMPUTE_BIT)
                {
                    compute = true;
                }
                if(propertiess[i].queueFlags & VK_QUEUE_GRAPHICS_BIT)
                {
                    graphics = true;
                }
                
                VkBool32 vkPresent = false;
                vkGetPhysicalDeviceSurfaceSupportKHR
                (device, i,
                vkSurfaceKHR,
                &vkPresent);
                
                if(vkPresent)
                {
                    present = true;
                }

                if(compute&&graphics&&present)
                {
                    return true;
                }
            }

            return false;
        }

        bool hasVkSurfaceFormatKHRSupport(VkPhysicalDevice device)
        {
            uint32_t numberFormats;
            vkGetPhysicalDeviceSurfaceFormatsKHR
            (device,
            vkSurfaceKHR,
            &numberFormats,
            nullptr);

            return (numberFormats > 0);
        }

        bool hasVkPresentModeKHRSupport(VkPhysicalDevice device)
        {
            uint32_t numberModes;
            vkGetPhysicalDeviceSurfacePresentModesKHR
            (device,
            vkSurfaceKHR,
            &numberModes,
            nullptr);

            return (numberModes > 0);
        }

        bool hasRequiredDeviceExtensions(VkPhysicalDevice device)
        {
            uint32_t extensionCount = 0;
            vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, nullptr);

            std::vector<VkExtensionProperties> availableExtensions(extensionCount);
            vkEnumerateDeviceExtensionProperties
            (device, nullptr, &extensionCount, availableExtensions.data());

            for(const char* requiredExtension : deviceExtensions)
            {
                bool found = std::any_of
                (availableExtensions.begin(), availableExtensions.end(),
                [requiredExtension](const VkExtensionProperties& extension)
                {
                    return std::strcmp(extension.extensionName, requiredExtension) == 0;
                });

                if(!found)
                {
                    return false;
                }
            }

            return true;
        }

        bool hasRequiredFeatures(VkPhysicalDevice device)
        {
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

            VkPhysicalDeviceVulkan11Features vulkan11Features{};
            vulkan11Features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES;
            vulkan11Features.shaderDrawParameters = VK_TRUE;
            vulkan11Features.multiview = VK_TRUE;
            vulkan11Features.pNext = &vulkan12Features;

            VkPhysicalDeviceFeatures2 deviceFeatures2{};
            deviceFeatures2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
            deviceFeatures2.features.samplerAnisotropy = VK_TRUE;
            deviceFeatures2.features.multiDrawIndirect = VK_TRUE;
            deviceFeatures2.features.shaderInt64 = VK_TRUE;
            deviceFeatures2.pNext = &vulkan11Features;

            vkGetPhysicalDeviceFeatures2(device, &deviceFeatures2);

            return deviceFeatures2.features.samplerAnisotropy &&
                     deviceFeatures2.features.multiDrawIndirect &&
                     deviceFeatures2.features.shaderInt64 &&
                   vulkan11Features.shaderDrawParameters &&
                   vulkan11Features.multiview &&
                   vulkan12Features.runtimeDescriptorArray &&
                   vulkan12Features.shaderSampledImageArrayNonUniformIndexing &&
                   vulkan12Features.shaderStorageBufferArrayNonUniformIndexing &&
                   vulkan12Features.drawIndirectCount &&
                   vulkan12Features.bufferDeviceAddress &&
                   dynamicRenderingFeatures.dynamicRendering &&
                   dgcFeatures.deviceGeneratedCommands &&
                   maintenance5Features.maintenance5;
        }

        bool fullfillsRequirements(VkPhysicalDevice device)
        {
            return (isComputeGraphicsPresent(device) &&
            hasVkSurfaceFormatKHRSupport(device) &&
            hasVkPresentModeKHRSupport(device) &&
            hasRequiredDeviceExtensions(device) &&
            hasRequiredFeatures(device));
        }

        void pickQueueFamilyIndex()
        {

            uint32_t numberPropertiess;
            vkGetPhysicalDeviceQueueFamilyProperties
            (vkPhysicalDevice,&numberPropertiess, NULL);
            
            if(numberPropertiess == 0)
            {
                LU_LOGE("pickVkPhysicalDevice", "no VkQueueFamilyPropertiess", "found")
            }

            std::vector<VkQueueFamilyProperties> propertiess(numberPropertiess);
            vkGetPhysicalDeviceQueueFamilyProperties
            (vkPhysicalDevice,
            &numberPropertiess, propertiess.data());

            for(uint32_t i = 0; i < numberPropertiess; i++)
            {
                bool compute = false;
                bool graphics = false;
                bool present = false;
                
                if(propertiess[i].queueFlags & VK_QUEUE_COMPUTE_BIT)
                {
                    compute = true;
                }
                if(propertiess[i].queueFlags & VK_QUEUE_GRAPHICS_BIT)
                {
                    graphics = true;
                }
                
                VkBool32 vkPresent = false;
                vkGetPhysicalDeviceSurfaceSupportKHR
                (vkPhysicalDevice, i,
                vkSurfaceKHR,
                &vkPresent);
                
                if(vkPresent)
                {
                    present = true;
                }

                if(compute&&graphics&&present)
                {
                    queueFamilyIndex = i;
                    return;
                }
            }
            
            LU_LOGE("pickQueueFamilyIndex", "no suitable queueFamilyIndex", "found")
        }

        void pickVkSurfaceFormatKHR()
        {   
            uint32_t numberFormats;
            vkGetPhysicalDeviceSurfaceFormatsKHR
            (vkPhysicalDevice, vkSurfaceKHR, &numberFormats, NULL);

            std::vector<VkSurfaceFormatKHR> formats(numberFormats);
            vkGetPhysicalDeviceSurfaceFormatsKHR
            (vkPhysicalDevice, vkSurfaceKHR, &numberFormats, formats.data());

            // Prefer SRGB swapchain so the hardware encodes linear→sRGB on write,
            // matching the sRGB textures sampled in the shaders (VK_FORMAT_R8G8B8A8_SRGB).
            for(uint32_t i = 0; i < numberFormats; i++)
            {
                if(formats[i].format == VK_FORMAT_B8G8R8A8_SRGB &&
                formats[i].colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
                {
                    vkSurfaceFormatKHR = formats[i];
                    return;
                }
            }
            // Fallback: BGRA_UNORM with SRGB colorspace
            for(uint32_t i = 0; i < numberFormats; i++)
            {
                if(formats[i].format == VK_FORMAT_B8G8R8A8_UNORM &&
                formats[i].colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
                {
                    vkSurfaceFormatKHR = formats[i];
                    return;
                }
            }
            vkSurfaceFormatKHR = formats[0];
        }

        void pickVkPresentModeKHR()
        {
            uint32_t numberPresentModes;
            vkGetPhysicalDeviceSurfacePresentModesKHR
            (vkPhysicalDevice, vkSurfaceKHR, &numberPresentModes, NULL);

            if(numberPresentModes == 0)
            {
                LU_LOGE("pickVkPresentModeKHR", "no VkPresentModeKHR", "found")
            }

            std::vector<VkPresentModeKHR> modes(numberPresentModes);
            vkGetPhysicalDeviceSurfacePresentModesKHR
            (vkPhysicalDevice, vkSurfaceKHR, &numberPresentModes, modes.data());

            for(size_t i = 0; i < numberPresentModes; i++)
            {
                if(modes[i] == VK_PRESENT_MODE_MAILBOX_KHR)
                {
                    vkPresentModeKHR =  modes[i];
                    return;
                }
            }
            vkPresentModeKHR = modes[0];
        }

        void getVkPhysicalDeviceProperties()
        {
            vkGetPhysicalDeviceProperties                                     
            (vkPhysicalDevice,
            &vkPhysicalDeviceProperties);
        }

        void getVkPhysicalDeviceMemoryProperties()
        {
            vkGetPhysicalDeviceMemoryProperties
            (vkPhysicalDevice,
            &vkPhysicalDeviceMemoryProperties);
        }

        void getVkSurfaceCapabilitiesKHR()
        {
            vkGetPhysicalDeviceSurfaceCapabilitiesKHR
            (vkPhysicalDevice,
            vkSurfaceKHR,
            &vkSurfaceCapabilitiesKHR);
        }

        uint32_t clamp(uint32_t value, uint32_t minValue, uint32_t maxValue)
        {
            return (value)*(minValue<=value&&value<=maxValue)+
            (minValue)*(value<minValue) + (maxValue)*(maxValue<value);
        }

        void chooseVkExtent2D()
        {

            if(vkSurfaceCapabilitiesKHR.currentExtent.width != UINT32_MAX &&
            vkSurfaceCapabilitiesKHR.currentExtent.height != UINT32_MAX)
            {
                vkExtent2D =  vkSurfaceCapabilitiesKHR.currentExtent;
                return;
            }

            int width, height;
            glfwGetFramebufferSize
            (pGLFWwindow, &width, &height);

            vkExtent2D.width = (uint32_t)width;
            vkExtent2D.height = (uint32_t)height;

            vkExtent2D.width =
            clamp(vkExtent2D.width,
            vkSurfaceCapabilitiesKHR.minImageExtent.width,
            vkSurfaceCapabilitiesKHR.maxImageExtent.width);
            vkExtent2D.height =
            clamp(vkExtent2D.height,
            vkSurfaceCapabilitiesKHR.minImageExtent.height,
            vkSurfaceCapabilitiesKHR.maxImageExtent.height);
        }

        void refreshSurfaceState()
        {
            getVkSurfaceCapabilitiesKHR();
            pickVkSurfaceFormatKHR();
            pickVkPresentModeKHR();
            chooseVkExtent2D();
        }

        void checkVkSampleCountFlagBits()
        {
            VkSampleCountFlags counts = 
            vkPhysicalDeviceProperties.limits.framebufferColorSampleCounts &
            vkPhysicalDeviceProperties.limits.framebufferDepthSampleCounts;

            if(counts & VK_SAMPLE_COUNT_64_BIT){maxVkSampleCountFlagBits = VK_SAMPLE_COUNT_64_BIT;}
            else if(counts & VK_SAMPLE_COUNT_32_BIT){maxVkSampleCountFlagBits = VK_SAMPLE_COUNT_32_BIT;}
            else if(counts & VK_SAMPLE_COUNT_16_BIT){maxVkSampleCountFlagBits = VK_SAMPLE_COUNT_16_BIT;}
            else if(counts & VK_SAMPLE_COUNT_8_BIT){maxVkSampleCountFlagBits = VK_SAMPLE_COUNT_8_BIT;}
            else if(counts & VK_SAMPLE_COUNT_4_BIT){maxVkSampleCountFlagBits = VK_SAMPLE_COUNT_4_BIT;}
            else if(counts & VK_SAMPLE_COUNT_2_BIT){maxVkSampleCountFlagBits = VK_SAMPLE_COUNT_2_BIT;}
            else {maxVkSampleCountFlagBits = VK_SAMPLE_COUNT_1_BIT;}
 
            if(vkSampleCountFlagBits > maxVkSampleCountFlagBits)
            {
                vkSampleCountFlagBits = maxVkSampleCountFlagBits;
            }
        }

        VkFormat findSupportedFormat(std::vector<VkFormat>& vkFormats, const VkFormatFeatureFlags vkFormatFeatureFlags)
        {

            for(size_t i = 0; i < vkFormats.size(); i++)
            {
                VkFormatProperties properties;
                vkGetPhysicalDeviceFormatProperties
                (vkPhysicalDevice,
                vkFormats[i], &properties);

                if((properties.optimalTilingFeatures & vkFormatFeatureFlags) ==
                vkFormatFeatureFlags)
                {
                    return vkFormats[i];
                }    
            }
            LU_LOGE("findSupportedFormat", "no suitable VkFormat found", "")
        }

        void pickPhysicalDevice()
        {
            uint32_t count = 0;
            vkEnumeratePhysicalDevices
            (vkInstance, &count, nullptr);

            if(count == 0)
            {
                LU_LOGE("pickPhysicalDevice", "no vkPhysicalDevices", "found")
            }

            std::vector<VkPhysicalDevice> devices(count);

            vkEnumeratePhysicalDevices
            (vkInstance, &count, devices.data());

            bool found = false;
            for(size_t i = 0; i < devices.size(); i++)
            {
                if(fullfillsRequirements(devices[i]))
                {
                    vkPhysicalDevice = devices[i];
                    found = true;
                    break;
                }
            }

            if(!found)
            {
                LU_LOGE("pickPhysicalDevice", "no VkPhysicalDevice fullfills requirements", "")
            }

            pickQueueFamilyIndex();
            pickVkSurfaceFormatKHR();
            pickVkPresentModeKHR();

            getVkPhysicalDeviceProperties();
            getVkPhysicalDeviceMemoryProperties();
            getVkSurfaceCapabilitiesKHR();

            chooseVkExtent2D();

            checkVkSampleCountFlagBits();
            
            std::vector<VkFormat> depthVkFormats = 
            {
                VK_FORMAT_D32_SFLOAT,
                VK_FORMAT_D32_SFLOAT_S8_UINT,
                VK_FORMAT_D24_UNORM_S8_UINT
            };
            depthVkFormat = findSupportedFormat(
            depthVkFormats,
            VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT);
            
            #ifdef LU_DEBUG
            LU_LOGI("PhysicalDevice", vkPhysicalDeviceProperties.deviceName, "picked")
            #endif
        }

        uint32_t findMemoryType
        (uint32_t typeFilter, VkMemoryPropertyFlags properties,
        VkPhysicalDevice vkPhysicalDevice)
        {

            for(uint32_t i = 0; i < vkPhysicalDeviceMemoryProperties.memoryTypeCount; i++)
            {
                if((typeFilter&(1<<i)) &&
                (vkPhysicalDeviceMemoryProperties.memoryTypes[i].propertyFlags & properties)
                == properties)
                {
                    return i;
                }
            }

            LU_LOGE("findMemoryType", "failed to find suitable memory type", "")
        }
    }
}
