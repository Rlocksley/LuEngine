#pragma once

#include "Global.hpp"
#include "Allocator.hpp"

namespace Lu{
    namespace Core{
        struct Image
        {
            uint32_t width{0};
            uint32_t height{0};
            VkImage vkImage{VK_NULL_HANDLE};
            VkImageView vkImageView{VK_NULL_HANDLE};
            VmaAllocation vmaAllocation{VK_NULL_HANDLE};

            Image() = default;
            
            Image(
            uint32_t width, uint32_t height,
            VkFormat format,
            VkImageUsageFlags usage,
            VkSampleCountFlagBits samples,
            VkImageAspectFlags aspect);

            ~Image();
            
            Image(const Image&) = delete;
            Image& operator=(const Image&) = delete;

            Image(Image&& other) noexcept
                : width(other.width)
                , height(other.height)
                , vkImage(other.vkImage)
                , vkImageView(other.vkImageView)
                , vmaAllocation(other.vmaAllocation)
            {
                // Reset the source object handles to prevent double destruction
                other.width = 0;
                other.height = 0;
                other.vkImage = VK_NULL_HANDLE;
                other.vkImageView = VK_NULL_HANDLE;
                other.vmaAllocation = nullptr; 
            }

            Image& operator=(Image&& other) noexcept
            {
                if (this != &other)
                {
                    // 2. Clean up any Vulkan resources currently held by this object
                    destroy();

                    // 3. Transfer resource ownership from the other object
                    width = other.width;
                    height = other.height;
                    vkImage = other.vkImage;
                    vkImageView = other.vkImageView;
                    vmaAllocation = other.vmaAllocation;

                    // 4. Reset the source object so its destructor won't free our new resources
                    other.width = 0;
                    other.height = 0;
                    other.vkImage = VK_NULL_HANDLE;
                    other.vkImageView = VK_NULL_HANDLE;
                    other.vmaAllocation = nullptr; // Or VmaAllocation{} depending on your VMA version
                }
                return *this;
            }



            void destroy();
        };


    }
}