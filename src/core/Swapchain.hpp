#pragma once

#include "Global.hpp"
#include "Constant.hpp"

namespace Lu
{
    namespace Core
    {
        struct Swapchain
        {
            std::vector<VkImage> vkImages{};
            std::vector<VkImageView> vkImageViews{};
            VkSwapchainKHR vkSwapchainKHR;
            std::array<VkSemaphore, MAX_FRAMES_IN_FLIGHT> imageAvailableSemaphores{};  // One Per Frame in flight
            std::vector<VkSemaphore> renderFinishedSemaphores{};  // One per swapchain image
            uint32_t imageIndex;
        };

        inline Swapchain swapchain;

        void createSwapchain();
        void destroySwapchain();
        void getSwapchainImageIndex(uint32_t frameIndex);
    }
}