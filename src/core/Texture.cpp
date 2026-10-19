#include "Texture.hpp"
#include "PhysicalDevice.hpp"
#include "Device.hpp"
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

            void Texture::recordCopy(VkCommandBuffer commandBuffer, VkBuffer sourceBuffer,
                VkDeviceSize sourceOffset) const {
                VkImageMemoryBarrier transferBarrier{};
                transferBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
                transferBarrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
                transferBarrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
                transferBarrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
                transferBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
                transferBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
                transferBarrier.image = vkImage;
                transferBarrier.subresourceRange = {aspect, 0, 1, 0, 1};

                vkCmdPipelineBarrier(
                    commandBuffer,
                    VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                    VK_PIPELINE_STAGE_TRANSFER_BIT,
                    0, 0, nullptr, 0, nullptr, 1, &transferBarrier);

                VkBufferImageCopy copyRegion{};
                copyRegion.bufferOffset = sourceOffset;
                copyRegion.imageSubresource = {aspect, 0, 0, 1};
                copyRegion.imageExtent = {width, height, 1};
                vkCmdCopyBufferToImage(
                    commandBuffer,
                    sourceBuffer,
                    vkImage,
                    VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                    1,
                    &copyRegion);

                VkImageMemoryBarrier shaderReadBarrier{};
                shaderReadBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
                shaderReadBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
                shaderReadBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
                shaderReadBarrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
                shaderReadBarrier.newLayout = parameters.sampledLayout;
                shaderReadBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
                shaderReadBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
                shaderReadBarrier.image = vkImage;
                shaderReadBarrier.subresourceRange = {aspect, 0, 1, 0, 1};
                vkCmdPipelineBarrier(
                    commandBuffer,
                    VK_PIPELINE_STAGE_TRANSFER_BIT,
                    VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                    0, 0, nullptr, 0, nullptr, 1, &shaderReadBarrier);
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