#pragma once

#include "Global.hpp"
#include "PhysicalDevice.hpp"
#include "Allocator.hpp"

namespace Lu{
    namespace Core{

        struct TextureCpu{
            uint32_t width{ 0 };
            uint32_t height{ 0 };
            std::shared_ptr<uint8_t[]> pixels{ nullptr };
            VkFormat vkFormat{};

            TextureCpu(uint32_t height, uint32_t width, std::shared_ptr<uint8_t[]> pixels);

            TextureCpu(const std::string& filePath);
        };

        struct TextureParameters {
            VkSampleCountFlagBits samples{VK_SAMPLE_COUNT_1_BIT};
            VkImageLayout sampledLayout{VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};

            VkSamplerCreateFlags samplerFlags{0};
            VkFilter minFilter{VK_FILTER_LINEAR};
            VkFilter magFilter{VK_FILTER_LINEAR};
            VkSamplerMipmapMode mipmapMode{VK_SAMPLER_MIPMAP_MODE_LINEAR};
            float mipLodBias{0.0f};
            VkBool32 anisotropyEnable{VK_TRUE};
            float maxAnisotropy{0.0f};
            VkBool32 compareEnable{VK_FALSE};
            VkCompareOp compareOp{VK_COMPARE_OP_ALWAYS};
            float minLod{0.0f};
            float maxLod{0.0f};
            VkBorderColor borderColor{VK_BORDER_COLOR_INT_OPAQUE_BLACK};
            VkBool32 unnormalizedCoordinates{VK_FALSE};
            VkSamplerAddressMode addressModeU{VK_SAMPLER_ADDRESS_MODE_REPEAT};
            VkSamplerAddressMode addressModeV{VK_SAMPLER_ADDRESS_MODE_REPEAT};
            VkSamplerAddressMode addressModeW{VK_SAMPLER_ADDRESS_MODE_REPEAT};
        };

        struct Texture{
            uint32_t width{0};
            uint32_t height{0};
            VkImage vkImage{VK_NULL_HANDLE};
            VkFormat format{VK_FORMAT_UNDEFINED};
            VkImageUsageFlags usage{0};
            VkImageAspectFlags aspect{VK_IMAGE_ASPECT_NONE};
            VmaAllocation vmaAllocation{nullptr};
            VkImageView vkImageView{VK_NULL_HANDLE};
            VkSampler vkSampler{VK_NULL_HANDLE};
            TextureParameters parameters{};

            Texture() = default;

            Texture(const uint32_t width, const uint32_t height, 
                    const VkFormat format, const VkImageUsageFlags usage, 
                    const VkImageAspectFlags aspect,
                    const TextureParameters& parameters = {}) : 
                    width(width), height(height), 
                    format(format), usage(usage), aspect(aspect),
                    parameters(parameters) {
                
                create();
            }

            Texture(const uint32_t width, const uint32_t height, 
                    const VkFormat format, const VkImageUsageFlags usage, 
                    const VkImageAspectFlags aspect, 
                    const std::shared_ptr<uint8_t[]> pixels,
                    const TextureParameters& parameters = {}) : 
                    width(width), height(height), 
                    format(format), usage(usage), aspect(aspect),
                    parameters(parameters) {
                
                create(pixels);
            }

            ~Texture(){
                destroy();
            }

            Texture(const Texture&) = delete;
            Texture& operator=(const Texture&) = delete;

            Texture(Texture&& other) noexcept
                : width(other.width)
                , height(other.height)
                , vkImage(other.vkImage)
                , format(other.format)
                , usage(other.usage)
                , aspect(other.aspect)
                , vkImageView(other.vkImageView)
                , vmaAllocation(other.vmaAllocation)
                , vkSampler(other.vkSampler)
                , parameters(other.parameters)
            {
                // Reset the source object handles to prevent double destruction
                other.width = 0;
                other.height = 0;
                other.vkImage = VK_NULL_HANDLE;
                other.format = VK_FORMAT_UNDEFINED;
                other.usage = 0;
                other.aspect = VK_IMAGE_ASPECT_NONE;
                other.vkImageView = VK_NULL_HANDLE;
                other.vmaAllocation = nullptr; 
                other.vkSampler = VK_NULL_HANDLE;
                other.parameters = {};
            }

            Texture& operator=(Texture&& other) noexcept
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
                    format = other.format;
                    usage = other.usage;
                    aspect = other.aspect;
                    vkSampler = other.vkSampler;
                    parameters = other.parameters;

                    // 4. Reset the source object so its destructor won't free our new resources
                    other.width = 0;
                    other.height = 0;
                    other.vkImage = VK_NULL_HANDLE;
                    other.vkImageView = VK_NULL_HANDLE;
                    other.vmaAllocation = nullptr; // Or VmaAllocation{} depending on your VMA version
                    other.format = VK_FORMAT_UNDEFINED;
                    other.usage = 0;
                    other.aspect = VK_IMAGE_ASPECT_NONE;
                    other.vkSampler = VK_NULL_HANDLE;
                    other.parameters = {};
                }
                return *this;
            }

            void create(const std::shared_ptr<uint8_t[]> pixels);
            void create();
            void destroy();
        };


        inline Texture depthMap;

        inline void createDepthMap(){
            TextureParameters parameters{};
            parameters.sampledLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
            parameters.minFilter = VK_FILTER_NEAREST;
            parameters.magFilter = VK_FILTER_NEAREST;
            parameters.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
            parameters.anisotropyEnable = VK_FALSE;
            parameters.maxAnisotropy = 1.0f;
            parameters.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
            parameters.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
            parameters.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;

            depthMap = 
            Texture
            (vkExtent2D.width, vkExtent2D.height, 
            depthVkFormat, 
            VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | 
            VK_IMAGE_USAGE_SAMPLED_BIT, 
            VK_IMAGE_ASPECT_DEPTH_BIT,
            parameters);
        }

        inline void destroyDepthMap(){
            depthMap.destroy();
        }
    }
}