#pragma once

#include "Global.hpp"
#include "PhysicalDevice.hpp"
#include "Device.hpp"
#include "Queue.hpp"
#include "Swapchain.hpp"
#include "Fence.hpp"

namespace Lu
{
    namespace Core
    {
        struct Command
        {
            VkCommandPool vkCommandPool;
            VkCommandBuffer vkCommandBuffer;
            VkFence vkFence;

            Command(){
                VkCommandPoolCreateInfo createInfo{};
                createInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
                createInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
                createInfo.queueFamilyIndex = queueFamilyIndex;

                LU_CHECK_VULKAN
                (vkCreateCommandPool
                (vkDevice,
                &createInfo,
                nullptr,
                &vkCommandPool),
                "createCommand",
                "vkCreateCommandPool")


                VkCommandBufferAllocateInfo allocInfo{};
                allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
                allocInfo.commandPool = vkCommandPool;
                allocInfo.commandBufferCount = 1;
                allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;

                LU_CHECK_VULKAN
                (vkAllocateCommandBuffers
                (vkDevice,
                &allocInfo,
                &vkCommandBuffer),
                "createCommand",
                "vkAllocateCommandBuffers")

                vkFence = createFence(0);
            }

            ~Command(){
                destroyFence(vkFence);

                vkFreeCommandBuffers
                (vkDevice,
                vkCommandPool,
                1, &vkCommandBuffer);

                vkDestroyCommandPool
                (vkDevice,
                vkCommandPool,
                nullptr);
            }

            void begin() const
            {
                VkCommandBufferBeginInfo beginInfo{};
                beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
                beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

                LU_CHECK_VULKAN
                (vkBeginCommandBuffer
                (vkCommandBuffer,
                &beginInfo),
                "Command::beginCommand",
                "vkBeginCommandBuffer")
            }

            void end() const
            {
                LU_CHECK_VULKAN
                (vkEndCommandBuffer
                (vkCommandBuffer),
                "Command::end",
                "vkEndCommandBuffer")
            }

            void submit() const 
            { 
                std::vector<VkPipelineStageFlags> stageFlags = 
                {
                    VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT
                };

                VkSubmitInfo submitInfo{};
                submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
                submitInfo.pWaitDstStageMask = stageFlags.data();
                submitInfo.waitSemaphoreCount = 0;
                submitInfo.pWaitSemaphores = nullptr;
                submitInfo.commandBufferCount = 1;
                submitInfo.pCommandBuffers = &vkCommandBuffer;
                submitInfo.signalSemaphoreCount = 0;
                submitInfo.pSignalSemaphores = nullptr;

                LU_CHECK_VULKAN
                (vkQueueSubmit
                (Lu::Core::vkQueue,
                1,
                &submitInfo,
                vkFence),
                "Command::submit",
                "vkQueueSubmit")
            }

            void submitGraphics(uint32_t frameIndex) const
            {
                std::vector<VkPipelineStageFlags> stageFlags = 
                {
                    VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT
                };

                VkSubmitInfo submitInfo{};
                submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
                submitInfo.pWaitDstStageMask = stageFlags.data();
                submitInfo.waitSemaphoreCount = 1;
                submitInfo.pWaitSemaphores = &swapchain.imageAvailableSemaphores[frameIndex];
                submitInfo.commandBufferCount = 1;
                submitInfo.pCommandBuffers = &vkCommandBuffer;
                submitInfo.signalSemaphoreCount = 1;
                submitInfo.pSignalSemaphores = &swapchain.renderFinishedSemaphores[swapchain.imageIndex];
                
                LU_CHECK_VULKAN
                (vkQueueSubmit
                (vkQueue,
                1,
                &submitInfo,
                vkFence),
                "Command::submitGraphics",
                "vkQueueSubmit")
            }

            void presentGraphics() const
            {

                VkPresentInfoKHR presentInfo{};
                presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
                presentInfo.waitSemaphoreCount = 1;
                presentInfo.pWaitSemaphores = &swapchain.renderFinishedSemaphores[swapchain.imageIndex];
                presentInfo.swapchainCount = 1;
                presentInfo.pSwapchains = &swapchain.vkSwapchainKHR;
                presentInfo.pImageIndices = &swapchain.imageIndex;

                LU_CHECK_VULKAN
                (vkQueuePresentKHR
                (vkQueue,
                &presentInfo),
                "Command::presentGraphics",
                "vkQueuePresentKHR")
            }


            void waitForFence() const
            {
                LU_CHECK_VULKAN
                (vkWaitForFences
                (vkDevice,
                1, &vkFence,
                VK_TRUE, UINT64_MAX),
                "Command::waitForFence",
                "vkWaitForFences")

                LU_CHECK_VULKAN
                (vkResetFences
                (vkDevice,
                1, &vkFence),
                "Command::waitForFence",
                "vkResetFences")
            }
        };
   }
}