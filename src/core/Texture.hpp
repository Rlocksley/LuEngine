#pragma once

#include "Global.hpp"
#include "Allocator.hpp"

namespace Lu{
    namespace Core{

        struct TextureCpu{
            uint32_t width{ 0 };
            uint32_t height{ 0 };
            std::shared_ptr<uint8_t[]> pixels{ nullptr };
            VkFormat vkFormat{};

            static TextureCpu create(uint32_t height, uint32_t width, std::shared_ptr<uint8_t[]> pixels);

            static TextureCpu load(const std::string& filePath);
        };

        struct Texture{
            uint32_t width;
            uint32_t height;
            VkImage vkImage;
            VkImageUsageFlags usage{0};
            VmaAllocation vmaAllocation;
            VkImageView vkImageView;
            VkSampler vkSampler;

            static Texture create(uint32_t width, uint32_t height, std::shared_ptr<uint8_t[]> pixels);

            static Texture create(const TextureCpu& textureCpu){
                return Texture::create(textureCpu.height, textureCpu.width, textureCpu.pixels);
            }

            void destroy();
        };

    }
}