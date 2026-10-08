#include "Texture.hpp"
#include "PhysicalDevice.hpp"
#include "Device.hpp"
#include "Queue.hpp"
#include "Buffer.hpp"
#define STB_IMAGE_IMPLEMENTATION
#include "stb/stb_image.h"

namespace Lu{
    namespace Core{
/*static*/ TextureCpu create(uint32_t width, uint32_t height, std::shared_ptr<uint8_t[]> pixels){
                return TextureCpu{width, height, pixels, VK_FORMAT_R8G8B8A8_SRGB};
            }

// Load the texture from the specified file path
/*static*/ TextureCpu load(const std::string& filePath){                
                int width, height, numberChannels;

                void* pixels = stbi_load(filePath.c_str(), &width, &height, &numberChannels, STBI_rgb_alpha);
                LU_ASSERT(pixels, "TextureCpu", "load", ("Failed to load texture from file: " + filePath).c_str());

                std::shared_ptr<uint8_t[]> spPixels(
                    static_cast<uint8_t*>(pixels),
                    [](uint8_t* pixels){
                        stbi_image_free(static_cast<void*>(pixels));
                    });

                return TextureCpu::create(width, height, spPixels);
            }

/*static*/ Texture Texture::create(uint32_t width, uint32_t height, std::shared_ptr<uint8_t[]> pixels){
                Texture image;
                image.width = width;
                image.height = height;
                image.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;

                VkImageCreateInfo createInfo;
                createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
                createInfo.flags = 0;
                createInfo.imageType = VK_IMAGE_TYPE_2D;
                createInfo.extent.width = width;
                createInfo.extent.height = height;
                createInfo.extent.depth = 1;
                createInfo.mipLevels = 1;
                createInfo.arrayLayers = 1;
                createInfo.format = VK_FORMAT_R8G8B8A8_SRGB;
                createInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
                createInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
                createInfo.usage = image.usage;
                createInfo.samples = VK_SAMPLE_COUNT_1_BIT;
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
                &image.vkImage,
                &image.vmaAllocation,
                nullptr),
                "createImage",
                "vmaCreateImage")

                VkImageViewCreateInfo ivCreateInfo;
                ivCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
                ivCreateInfo.flags = 0;
                ivCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
                ivCreateInfo.format = VK_FORMAT_R8G8B8A8_SRGB;
                ivCreateInfo.image = image.vkImage;
                ivCreateInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
                ivCreateInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
                ivCreateInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
                ivCreateInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
                ivCreateInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
                ivCreateInfo.subresourceRange.baseMipLevel = 0;
                ivCreateInfo.subresourceRange.levelCount = 1;
                ivCreateInfo.subresourceRange.baseArrayLayer = 0;
                ivCreateInfo.subresourceRange.layerCount = 1;
                ivCreateInfo.pNext = NULL;

                LU_CHECK_VULKAN
                (vkCreateImageView
                (vkDevice,
                &ivCreateInfo, NULL,
                &image.vkImageView),
                "createImage",
                "vkCreateImageView")
                
                VkSamplerCreateInfo samplerCreateInfo;
                samplerCreateInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
                samplerCreateInfo.flags = 0;
                samplerCreateInfo.minFilter = VK_FILTER_LINEAR;
                samplerCreateInfo.magFilter = VK_FILTER_LINEAR;
                samplerCreateInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
                samplerCreateInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
                samplerCreateInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
                samplerCreateInfo.anisotropyEnable = VK_TRUE;
                samplerCreateInfo.maxAnisotropy = vkPhysicalDeviceProperties.limits.maxSamplerAnisotropy;
                samplerCreateInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
                samplerCreateInfo.unnormalizedCoordinates = VK_FALSE;
                samplerCreateInfo.compareEnable = VK_FALSE;
                samplerCreateInfo.compareOp = VK_COMPARE_OP_ALWAYS;
                samplerCreateInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
                samplerCreateInfo.mipLodBias = 0.f;
                samplerCreateInfo.minLod = 0.f;
                samplerCreateInfo.maxLod = 0.f;
                samplerCreateInfo.pNext = NULL;

                LU_CHECK_VULKAN
                (vkCreateSampler
                (vkDevice,
                &samplerCreateInfo, 
                NULL,
                &image.vkSampler),
                "createImage",
                "vkCreateSampler")

                BufferInterfaceDynamic<uint8_t> copyBuffer(
                    width * height * 4,
                    VK_BUFFER_USAGE_TRANSFER_SRC_BIT);
                memcpy(copyBuffer.pMemory, pixels.get(), width * height * 4);

                VkImageMemoryBarrier barrier0;
                barrier0.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
                barrier0.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
                barrier0.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
                barrier0.srcAccessMask = 0;
                barrier0.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
                barrier0.image = image.vkImage;
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
                barrier1.image = image.vkImage;
                barrier1.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
                barrier1.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
                barrier1.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
                barrier1.subresourceRange.baseMipLevel = 0;
                barrier1.subresourceRange.levelCount = 1;
                barrier1.subresourceRange.baseArrayLayer = 0;
                barrier1.subresourceRange.layerCount = 1;
                barrier1.pNext = NULL;


                beginSingleCommand(singleCommand);

                vkCmdPipelineBarrier
                (singleCommand.vkCommandBuffer,
                VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                VK_PIPELINE_STAGE_TRANSFER_BIT,
                0,
                0, NULL,
                0, NULL,
                1, &barrier0);

                vkCmdCopyBufferToImage
                (singleCommand.vkCommandBuffer,
                copyBuffer.vkBuffer,
                image.vkImage,
                VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                1, &copy);

                vkCmdPipelineBarrier
                (singleCommand.vkCommandBuffer,
                VK_PIPELINE_STAGE_TRANSFER_BIT,
                VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                0,
                0, NULL,
                0, NULL,
                1, &barrier1);

                endSingleCommand(singleCommand);
                //TBD add mutext if loading screen is rendering and Texture is uploaded in the backround
                submitSingleCommand(singleCommand);

                return image;

            }

        void Texture::destroy(){
            vkDestroySampler
            (vkDevice, 
            vkSampler, 
            nullptr);
            vkSampler = nullptr;

            vkDestroyImageView
            (vkDevice, 
            vkImageView, 
            nullptr);
            vkImageView = nullptr;

            vmaDestroyImage
            (vmaAllocator, 
            vkImage, 
            vmaAllocation);
            vkImage = nullptr;
        }
    }
}