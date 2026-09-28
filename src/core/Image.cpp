#include "Image.hpp"
#include "Device.hpp"

namespace Lu{
    namespace Core{
        Image::Image(
            uint32_t width, uint32_t height,
            VkFormat format,
            VkImageUsageFlags usage,
            VkSampleCountFlagBits samples,
            VkImageAspectFlags aspect) :
            width(width), height(height)
        {
        
            VkImageCreateInfo createInfo{};
            createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
            createInfo.imageType = VK_IMAGE_TYPE_2D;
            createInfo.extent.width = width;
            createInfo.extent.height = height;
            createInfo.extent.depth = 1;
            createInfo.mipLevels = 1;
            createInfo.arrayLayers = 1;
            createInfo.format = format;
            createInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            createInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
            createInfo.usage = usage;
            createInfo.samples = samples;

            VmaAllocationCreateInfo allocInfo{};
            allocInfo.usage = VMA_MEMORY_USAGE_AUTO;
            allocInfo.flags = VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT;
            allocInfo.priority = 1.f;

            LU_CHECK_VULKAN
            (vmaCreateImage
            (vmaAllocator,
            &createInfo,
            &allocInfo,
            &vkImage,
            &vmaAllocation,
            nullptr),
            "Image",
            "vmaCreateImage")

            VkImageViewCreateInfo ivCreateInfo{};
            ivCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
            ivCreateInfo.image = vkImage;
            ivCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
            ivCreateInfo.format = format;
            ivCreateInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
            ivCreateInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
            ivCreateInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
            ivCreateInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
            ivCreateInfo.subresourceRange.aspectMask = aspect;
            ivCreateInfo.subresourceRange.baseMipLevel = 0;
            ivCreateInfo.subresourceRange.levelCount = 1;
            ivCreateInfo.subresourceRange.baseArrayLayer = 0;
            ivCreateInfo.subresourceRange.layerCount = 1;

            LU_CHECK_VULKAN
            (vkCreateImageView
            (vkDevice,
            &ivCreateInfo,
            nullptr,
            &vkImageView),
            "createImage",
            "vkCreateImageView")
        }

        Image::~Image(){
            destroy();
        }

        void Image::destroy()
        {
            if(vkImageView != VK_NULL_HANDLE){
                vkDestroyImageView
                (vkDevice,
                vkImageView,
                nullptr);
                vkImageView = VK_NULL_HANDLE;
            }

            if(vkImage != VK_NULL_HANDLE){
                vmaDestroyImage
                (vmaAllocator,
                vkImage,
                vmaAllocation);
                vkImage = VK_NULL_HANDLE;
            }
        }


    }
}