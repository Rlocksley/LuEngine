#include "Swapchain.hpp"
#include "Surface.hpp"
#include "PhysicalDevice.hpp"
#include "Device.hpp"
#include "Semaphore.hpp"
#include "Constant.hpp"

namespace Lu
{
    namespace Core
    {
        void createSwapchain()
        {
            const uint32_t minImageCount = 
            vkSurfaceCapabilitiesKHR.maxImageCount > 0 ?
            vkSurfaceCapabilitiesKHR.maxImageCount : 
            vkSurfaceCapabilitiesKHR.minImageCount + 1;

            VkSwapchainCreateInfoKHR createInfo{};
            createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
            createInfo.surface = vkSurfaceKHR;
            createInfo.minImageCount = minImageCount;
            createInfo.imageFormat = vkSurfaceFormatKHR.format;
            createInfo.imageColorSpace = vkSurfaceFormatKHR.colorSpace;
            createInfo.imageExtent = vkExtent2D;
            createInfo.imageArrayLayers = 1;
            createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
            createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
            createInfo.preTransform = vkSurfaceCapabilitiesKHR.currentTransform;
            createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
            createInfo.presentMode = vkPresentModeKHR;
            createInfo.clipped = VK_TRUE;
            createInfo.oldSwapchain = nullptr;

            LU_CHECK_VULKAN
            (vkCreateSwapchainKHR
            (vkDevice,
            &createInfo,
            nullptr,
            &swapchain.vkSwapchainKHR),
            "createSwapchain",
            "vkCreateSwapchainKHR")

            uint32_t numberImages;

            LU_CHECK_VULKAN
            (vkGetSwapchainImagesKHR
            (vkDevice,
            swapchain.vkSwapchainKHR,
            &numberImages,
            nullptr),
            "createSwapchain",
            "vkGetSwapchainImageKHR")

            if(numberImages == 0)
            {
                LU_LOGE("createSwapchain", "number VkSwapchainKHR images", "zero")
            }

            swapchain.vkImages.resize(numberImages);

            LU_CHECK_VULKAN
            (vkGetSwapchainImagesKHR
            (vkDevice,
            swapchain.vkSwapchainKHR,
            &numberImages,
            swapchain.vkImages.data()),
            "createSwapchain",
            "vkGetSwapchainImagesKHR")

            swapchain.vkImageViews.resize(numberImages);

            for(uint32_t i = 0; i < numberImages; i++)
            {
                VkImageViewCreateInfo ivCreateInfo{};
                ivCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
                ivCreateInfo.image = swapchain.vkImages[i];
                ivCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
                ivCreateInfo.format = vkSurfaceFormatKHR.format;
                ivCreateInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
                ivCreateInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
                ivCreateInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
                ivCreateInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
                ivCreateInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
                ivCreateInfo.subresourceRange.baseMipLevel = 0;
                ivCreateInfo.subresourceRange.levelCount = 1;
                ivCreateInfo.subresourceRange.baseArrayLayer = 0;
                ivCreateInfo.subresourceRange.layerCount = 1;

                LU_CHECK_VULKAN
                (vkCreateImageView
                (vkDevice,
                &ivCreateInfo,
                nullptr,
                &swapchain.vkImageViews[i]),
                "createSwapchain",
                "vkCreateImageView")
            }

            for(uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
            {
                swapchain.imageAvailableSemaphores[i] = createSemaphore();
            }

            swapchain.renderFinishedSemaphores.resize(swapchain.vkImages.size());
            for(size_t i = 0; i < swapchain.vkImages.size(); i++)
            {
                swapchain.renderFinishedSemaphores[i] = createSemaphore();
            }

            #ifdef LU_DEBUG
            LU_LOGI("Swapchain", "created", "")
            #endif        
        }

        void destroySwapchain()
        {
            for(size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
            {
                destroySemaphore(swapchain.imageAvailableSemaphores[i]);
            }
            for(size_t i = 0; i < swapchain.renderFinishedSemaphores.size(); i++)
            {
                destroySemaphore(swapchain.renderFinishedSemaphores[i]);
            }
            swapchain.renderFinishedSemaphores.clear();
            
            for(size_t i = 0; i < swapchain.vkImageViews.size(); i++)
            {
                vkDestroyImageView
                (vkDevice,
                swapchain.vkImageViews[i],
                nullptr);
            }

            swapchain.vkImageViews.clear();
            swapchain.vkImages.clear();

            vkDestroySwapchainKHR
            (vkDevice,
            swapchain.vkSwapchainKHR,
            nullptr);
        }

        void getSwapchainImageIndex(uint32_t frameIndex)
        {
            LU_CHECK_VULKAN
            (vkAcquireNextImageKHR
            (vkDevice,
            swapchain.vkSwapchainKHR,
            UINT64_MAX,
            swapchain.imageAvailableSemaphores[frameIndex],
            nullptr,
            &swapchain.imageIndex),
            "getSwapchainImageIndex",
            "vkAcquireNextImageKHR")
        }

    }
}