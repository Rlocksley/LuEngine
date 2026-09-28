#include "Framebuffers.hpp"
#include "PhysicalDevice.hpp"
#include "Device.hpp"
#include "Swapchain.hpp"
#include "RenderPass.hpp"

namespace Lu
{
    namespace Core
    {

        void createFramebuffers()
        {
            //framebuffers.vkFramebuffers.resize(swapchain.vkImages.size());
            framebuffers.colorImages.resize(swapchain.vkImages.size());
            framebuffers.depthImages.resize(swapchain.vkImages.size());

            for(size_t i = 0; i < swapchain.vkImages.size(); i++)
            {
                framebuffers.colorImages[i] = 
                Image
                (vkExtent2D.width, vkExtent2D.height,
                vkSurfaceFormatKHR.format,
                VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT |
                VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
                vkSampleCountFlagBits,
                VK_IMAGE_ASPECT_COLOR_BIT);


                framebuffers.depthImages[i] = 
                Image
                (vkExtent2D.width, vkExtent2D.height,
                depthVkFormat,
                VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
                vkSampleCountFlagBits,
                VK_IMAGE_ASPECT_DEPTH_BIT);

                // std::vector<VkImageView> attachments = 
                // {
                //     framebuffers.colorImages[i].vkImageView,
                //     framebuffers.depthImages[i].vkImageView,
                //     swapchain.vkImageViews[i]
                // };

                // VkFramebufferCreateInfo createInfo{};
                // createInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
                // createInfo.renderPass = vkRenderPass;
                // createInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
                // createInfo.pAttachments = attachments.data();
                // createInfo.width = vkExtent2D.width;
                // createInfo.height = vkExtent2D.height;
                // createInfo.layers = 1;

                // LU_CHECK_VULKAN
                // (vkCreateFramebuffer
                // (vkDevice,
                // &createInfo,
                // nullptr,
                // &framebuffers.vkFramebuffers[i]),
                // "createFramebuffers",
                // "vkCreateFramebuffer")
            }

            #ifdef LU_DEBUG
            LU_LOGI("Framebuffers", "created", "")
            #endif
        }
        
        void destroyFramebuffers()
        {
            // for(size_t i = 0; i < framebuffers.vkFramebuffers.size(); i++)
            // {
            //     vkDestroyFramebuffer
            //     (vkDevice,
            //     framebuffers.vkFramebuffers[i],
            //     nullptr);
            // }

            // framebuffers.vkFramebuffers.clear();
            
            for(size_t i = 0; i < framebuffers.depthImages.size(); i++)
            {
                framebuffers.depthImages[i].destroy();
                framebuffers.colorImages[i].destroy();
            }

            framebuffers.depthImages.clear();
            framebuffers.colorImages.clear();
        }
    }
}