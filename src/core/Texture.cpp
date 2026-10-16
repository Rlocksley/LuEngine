#include "Texture.hpp"
#include "PhysicalDevice.hpp"
#include "Device.hpp"
#include "Queue.hpp"
#include "Buffer.hpp"
#define STB_IMAGE_IMPLEMENTATION
#include "stb/stb_image.h"

namespace Lu{
    namespace Core{
            TextureCpu::TextureCpu(uint32_t width, uint32_t height, std::shared_ptr<uint8_t[]> pixels): 
            width(width), height(height), pixels(pixels), vkFormat(VK_FORMAT_R8G8B8A8_SRGB) 
            {}
            

            TextureCpu::TextureCpu(const std::string& filePath): 
            width(0), height(0), pixels(nullptr), vkFormat(VK_FORMAT_R8G8B8A8_SRGB) 
            {
                int width, height, numberChannels;

                void* pixels = stbi_load(filePath.c_str(), &width, &height, &numberChannels, STBI_rgb_alpha);
                LU_ASSERT(pixels, "TextureCpu", "load", ("Failed to load texture from file: " + filePath).c_str());

                std::shared_ptr<uint8_t[]> spPixels(
                    static_cast<uint8_t*>(pixels),
                    [](uint8_t* pixels){
                        stbi_image_free(static_cast<void*>(pixels));
                    });

                this->width = static_cast<uint32_t>(width);
                this->height = static_cast<uint32_t>(height);
                this->pixels = spPixels;
            }

            void Texture::create(){
                
                VkImageCreateInfo createInfo{};
                createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
                createInfo.flags = 0;
                createInfo.imageType = VK_IMAGE_TYPE_2D;
                createInfo.extent.width = width;
                createInfo.extent.height = height;
                createInfo.extent.depth = 1;
                createInfo.mipLevels = 1;
                createInfo.arrayLayers = 1;
                createInfo.format = format;
                createInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
                createInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
                createInfo.usage = usage;
                createInfo.samples = parameters.samples;
                createInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
                createInfo.queueFamilyIndexCount = 0;
                createInfo.pQueueFamilyIndices = NULL;
                createInfo.pNext = NULL;

                VmaAllocationCreateInfo allocInfo{};
                allocInfo.usage = VMA_MEMORY_USAGE_AUTO;
                allocInfo.flags = VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT;
                allocInfo.priority = 1.f;

                LU_CHECK_VULKAN
                (vmaCreateImage
                (vmaAllocator,
                &createInfo,
                &allocInfo,
                &this->vkImage,
                &this->vmaAllocation,
                nullptr),
                "Texture",
                "vmaCreateImage")

                VkImageViewCreateInfo ivCreateInfo;
                ivCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
                ivCreateInfo.flags = 0;
                ivCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
                ivCreateInfo.format = format;
                ivCreateInfo.image = this->vkImage;
                ivCreateInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
                ivCreateInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
                ivCreateInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
                ivCreateInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
                ivCreateInfo.subresourceRange.aspectMask = aspect;
                ivCreateInfo.subresourceRange.baseMipLevel = 0;
                ivCreateInfo.subresourceRange.levelCount = 1;
                ivCreateInfo.subresourceRange.baseArrayLayer = 0;
                ivCreateInfo.subresourceRange.layerCount = 1;
                ivCreateInfo.pNext = NULL;

                LU_CHECK_VULKAN
                (vkCreateImageView
                (vkDevice,
                &ivCreateInfo, NULL,
                &this->vkImageView),
                "Texture",
                "vkCreateImageView")
                
                VkSamplerCreateInfo samplerCreateInfo{};
                samplerCreateInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
                samplerCreateInfo.flags = parameters.samplerFlags;
                samplerCreateInfo.minFilter = parameters.minFilter;
                samplerCreateInfo.magFilter = parameters.magFilter;
                samplerCreateInfo.addressModeU = parameters.addressModeU;
                samplerCreateInfo.addressModeV = parameters.addressModeV;
                samplerCreateInfo.addressModeW = parameters.addressModeW;
                samplerCreateInfo.anisotropyEnable = parameters.anisotropyEnable;
                samplerCreateInfo.maxAnisotropy = parameters.maxAnisotropy > 0.0f
                    ? parameters.maxAnisotropy
                    : vkPhysicalDeviceProperties.limits.maxSamplerAnisotropy;
                samplerCreateInfo.borderColor = parameters.borderColor;
                samplerCreateInfo.unnormalizedCoordinates = parameters.unnormalizedCoordinates;
                samplerCreateInfo.compareEnable = parameters.compareEnable;
                samplerCreateInfo.compareOp = parameters.compareOp;
                samplerCreateInfo.mipmapMode = parameters.mipmapMode;
                samplerCreateInfo.mipLodBias = parameters.mipLodBias;
                samplerCreateInfo.minLod = parameters.minLod;
                samplerCreateInfo.maxLod = parameters.maxLod;
                samplerCreateInfo.pNext = nullptr;

                LU_CHECK_VULKAN
                (vkCreateSampler
                (vkDevice,
                &samplerCreateInfo, 
                NULL,
                &this->vkSampler),
                "Texture",
                "vkCreateSampler")
            }

            void Texture::create(std::shared_ptr<uint8_t[]> pixels){
                create();

                BufferInterface<uint8_t> copyBuffer(
                    width * height * 4,
                    VK_BUFFER_USAGE_TRANSFER_SRC_BIT);
                memcpy(copyBuffer.pMemory, pixels.get(), width * height * 4);

                VkImageMemoryBarrier barrier0;
                barrier0.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
                barrier0.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
                barrier0.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
                barrier0.srcAccessMask = 0;
                barrier0.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
                barrier0.image = vkImage;
                barrier0.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
                barrier0.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
                barrier0.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
                barrier0.subresourceRange.baseMipLevel = 0;
                barrier0.subresourceRange.levelCount = 1;
                barrier0.subresourceRange.baseArrayLayer = 0;
                barrier0.subresourceRange.layerCount = 1;
                barrier0.pNext = NULL;

                VkBufferImageCopy copy;
                copy.bufferOffset = 0;
                copy.bufferRowLength = 0;
                copy.bufferImageHeight = 0;
                copy.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
                copy.imageSubresource.mipLevel = 0;
                copy.imageSubresource.baseArrayLayer = 0;
                copy.imageSubresource.layerCount = 1;
                copy.imageOffset.x = 0;
                copy.imageOffset.y = 0;
                copy.imageOffset.z = 0;
                copy.imageExtent.width = width;
                copy.imageExtent.height = height;
                copy.imageExtent.depth = 1;

                VkImageMemoryBarrier barrier1;
                barrier1.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
                barrier1.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
                barrier1.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
                barrier1.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
                barrier1.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
                barrier1.image = vkImage;
                barrier1.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
                barrier1.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
                barrier1.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
                barrier1.subresourceRange.baseMipLevel = 0;
                barrier1.subresourceRange.levelCount = 1;
                barrier1.subresourceRange.baseArrayLayer = 0;
                barrier1.subresourceRange.layerCount = 1;
                barrier1.pNext = NULL;

                Command command;

                command.begin();

                vkCmdPipelineBarrier
                (command.vkCommandBuffer,
                VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                VK_PIPELINE_STAGE_TRANSFER_BIT,
                0,
                0, NULL,
                0, NULL,
                1, &barrier0);

                vkCmdCopyBufferToImage
                (command.vkCommandBuffer,
                copyBuffer.vkBuffer,
                vkImage,
                VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                1, &copy);

                vkCmdPipelineBarrier
                (command.vkCommandBuffer,
                VK_PIPELINE_STAGE_TRANSFER_BIT,
                VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                0,
                0, NULL,
                0, NULL,
                1, &barrier1);

                command.end();
                //TBD add mutext if loading screen is rendering and Texture is uploaded in the backround
                command.submit();
                command.waitForFence();
                command.resetFence();
            }

        void Texture::destroy(){
            if(vkSampler != VK_NULL_HANDLE){
                vkDestroySampler
                (vkDevice, 
                vkSampler, 
                nullptr);
                vkSampler = VK_NULL_HANDLE;
            }

            if(vkImageView != VK_NULL_HANDLE){
                vkDestroyImageView
                (vkDevice, 
                vkImageView, 
                nullptr);
                vkImageView = VK_NULL_HANDLE;
            }

            if(vkImage != VK_NULL_HANDLE && vmaAllocation != nullptr){
                vmaDestroyImage
                (vmaAllocator, 
                vkImage, 
                vmaAllocation);
                vkImage = VK_NULL_HANDLE;
                vmaAllocation = nullptr;
            }
        }
    }
}