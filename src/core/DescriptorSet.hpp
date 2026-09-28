#pragma once

#include "Global.hpp"
#include "Buffer.hpp"
#include "Texture.hpp"

namespace Lu {
    namespace Core {

        class DescriptorSet {
        public:
            DescriptorSet() = default;
            DescriptorSet(const DescriptorSet&) = delete;
            DescriptorSet& operator=(const DescriptorSet&) = delete;
            DescriptorSet(DescriptorSet&&) = delete;
            DescriptorSet& operator=(DescriptorSet&&) = delete;
            ~DescriptorSet();

            DescriptorSet& addUniformBuffer(uint32_t bindingNumber, const IBufferResource& buffer, VkShaderStageFlags stageFlags);
            DescriptorSet& addStorageBuffer(uint32_t bindingNumber, const IBufferResource& buffer, VkShaderStageFlags stageFlags);

            template<size_t Size>
            DescriptorSet& addUniformBufferArray(uint32_t bindingNumber, const std::array<std::shared_ptr<IBufferResource>, Size>& buffers, VkShaderStageFlags stageFlags);

            template<size_t Size>
            DescriptorSet& addStorageBufferArray(uint32_t bindingNumber, const std::array<std::shared_ptr<IBufferResource>, Size>& buffers, VkShaderStageFlags stageFlags);

            DescriptorSet& addTexture(uint32_t bindingNumber, const Texture& texture, VkShaderStageFlags stageFlags);

            template<size_t Size>
            DescriptorSet& addTextureArray(uint32_t bindingNumber, const std::array<std::unique_ptr<Texture>, Size>& textures, VkShaderStageFlags stageFlags);

            DescriptorSet& addPushConstantRange(VkShaderStageFlags stageFlags, uint32_t offset, uint32_t size);

            void build();
            void updateDescriptorSetWrites();

            const VkDescriptorSet& getVkDescriptorSet(uint32_t frameIndex) const { return descriptorSets[frameIndex]; }
            const VkDescriptorSetLayout& getVkDescriptorSetLayout() const { return descriptorSetLayout; }
            const VkPipelineLayout& getVkPipelineLayout() const { return pipelineLayout; }

        private:
            struct DescriptorResource {
                virtual ~DescriptorResource() = default;
                uint32_t binding = 0;
                VkDescriptorType type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
                VkShaderStageFlags stageFlags = 0;
                uint32_t descriptorCount = 1;
                virtual VkBuffer getVkBuffer(uint32_t frameIndex, uint32_t arrayIndex) const = 0;
                virtual VkDescriptorImageInfo getVkDescriptorImageInfo(uint32_t arrayIndex) const = 0;
            };

            template<size_t Size>
            struct BufferDescriptorResource final : DescriptorResource {
                const IBufferResource* buffer = nullptr;
                const std::array<std::shared_ptr<IBufferResource>, Size>* buffers = nullptr;

                explicit BufferDescriptorResource(const IBufferResource& value) : buffer(&value) {}
                explicit BufferDescriptorResource(const std::array<std::shared_ptr<IBufferResource>, Size>& values) : buffers(&values) {}

                VkBuffer getVkBuffer(uint32_t frameIndex, uint32_t arrayIndex) const override {
                    if (buffer != nullptr) {
                        LU_ASSERT(arrayIndex == 0, "DescriptorSetBase", "getVkBuffer", "Single buffer resource indexed as an array.")
                        return buffer->getVkBuffer(frameIndex);
                    }
                    LU_ASSERT(buffers != nullptr && (*buffers)[arrayIndex] != nullptr, "DescriptorSetBase", "getVkBuffer", "Buffer array entry is null.")
                    return (*buffers)[arrayIndex]->getVkBuffer(frameIndex);
                }

                VkDescriptorImageInfo getVkDescriptorImageInfo(uint32_t) const override { 
                    LU_LOGE("BufferDescriptorResource", "getVkDescriptorImageInfo", "Function must not be called");
                    return {}; 
                }
            };

            template<size_t Size>
            struct TextureDescriptorResource final : DescriptorResource {
                const Texture* texture{nullptr};
                const std::array<std::unique_ptr<Texture>, Size>* textures{nullptr};

                explicit TextureDescriptorResource(const Texture& value) : texture(&value) {}
                explicit TextureDescriptorResource(const std::array<std::unique_ptr<Texture>, Size>& values) : textures(&values) {}

                VkBuffer getVkBuffer(uint32_t, uint32_t) const override { 
                    LU_LOGE("TextureDescriptorResource", "getVkBuffer", "Function must not be called");
                    return VK_NULL_HANDLE; 
                }

                VkDescriptorImageInfo getVkDescriptorImageInfo(uint32_t arrayIndex) const override {
                    const Texture* selectedTexture;
                    if(arrayIndex == 0){
                        selectedTexture = texture;
                    }
                    if (selectedTexture == nullptr) {
                        LU_ASSERT((*textures)[arrayIndex] != nullptr, "DescriptorSetBase", "getVkDescriptorImageInfo", "Texture array entry is null.")
                        selectedTexture = (*textures)[arrayIndex].get();
                    }
                    LU_ASSERT(selectedTexture != nullptr, "DescriptorSetBase", "getVkDescriptorImageInfo", "Texture resource is null.")
                    return {selectedTexture->vkSampler, selectedTexture->vkImageView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
                }
            };

            DescriptorSet& addBuffer(uint32_t bindingNumber, VkDescriptorType descriptorType, const IBufferResource& buffer, VkShaderStageFlags stageFlags);

            template<size_t Size>
            DescriptorSet& addBufferArray(uint32_t bindingNumber, VkDescriptorType descriptorType, const std::array<std::shared_ptr<IBufferResource>, Size>& buffers, VkShaderStageFlags stageFlags);

            void resetVulkanObjects();
            void updateDescriptorSetWrites(uint32_t frameIndex);

            std::vector<std::unique_ptr<DescriptorResource>> descriptorResources;
            std::vector<VkPushConstantRange> pushConstantRanges;
            std::array<VkDescriptorSet, MAX_FRAMES_IN_FLIGHT> descriptorSets{};
            VkDescriptorSetLayout descriptorSetLayout{VK_NULL_HANDLE};
            VkPipelineLayout pipelineLayout{VK_NULL_HANDLE};
        };

        class MeshDescriptorSetBase final : public DescriptorSet {
        };

        template<size_t Size>
        DescriptorSet& DescriptorSet::addUniformBufferArray(uint32_t bindingNumber, const std::array<std::shared_ptr<IBufferResource>, Size>& buffers, VkShaderStageFlags stageFlags) {
            static_assert(Size > 0, "A descriptor buffer array cannot be empty.");
            for (const auto& buffer : buffers) {
                LU_ASSERT(buffer != nullptr, "DescriptorSetBase", "addUniformBufferArray", "Buffer array entry must not be null.")
                LU_ASSERT((buffer->getVkBufferUsage() & VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT) != 0, "DescriptorSetBase", "addUniformBufferArray", "Buffer is missing VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT.")
            }
            return addBufferArray(bindingNumber, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, buffers, stageFlags);
        }

        template<size_t Size>
        DescriptorSet& DescriptorSet::addStorageBufferArray(uint32_t bindingNumber, const std::array<std::shared_ptr<IBufferResource>, Size>& buffers, VkShaderStageFlags stageFlags) {
            static_assert(Size > 0, "A descriptor buffer array cannot be empty.");
            for (const auto& buffer : buffers) {
                LU_ASSERT(buffer.get() != nullptr, "DescriptorSetBase", "addStorageBufferArray", "Buffer array entry must not be null.")
                LU_ASSERT((buffer->getVkBufferUsage() & VK_BUFFER_USAGE_STORAGE_BUFFER_BIT) != 0, "DescriptorSetBase", "addStorageBufferArray", "Buffer is missing VK_BUFFER_USAGE_STORAGE_BUFFER_BIT.")
            }
            return addBufferArray(bindingNumber, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, buffers, stageFlags);
        }

        template<size_t Size>
        DescriptorSet& DescriptorSet::addBufferArray(uint32_t bindingNumber, VkDescriptorType descriptorType, const std::array<std::shared_ptr<IBufferResource>, Size>& buffers, VkShaderStageFlags stageFlags) {
            auto resource = std::make_unique<BufferDescriptorResource<Size>>(buffers);
            resource->binding = bindingNumber;
            resource->type = descriptorType;
            resource->stageFlags = stageFlags;
            resource->descriptorCount = static_cast<uint32_t>(Size);
            descriptorResources.push_back(std::move(resource));
            return *this;
        }

        template<size_t Size>
        DescriptorSet& DescriptorSet::addTextureArray(uint32_t bindingNumber, const std::array<std::unique_ptr<Texture>, Size>& textures, VkShaderStageFlags stageFlags) {
            static_assert(Size > 0, "A descriptor texture array cannot be empty.");
            for (const auto& texture : textures) {
                LU_ASSERT(texture != nullptr, "DescriptorSetBase", "addTextureArray", "Texture array entry must not be null.")
                LU_ASSERT((texture->usage & VK_IMAGE_USAGE_SAMPLED_BIT) != 0, "DescriptorSetBase", "addTextureArray", "Texture is missing VK_IMAGE_USAGE_SAMPLED_BIT.")
            }
            auto resource = std::make_unique<TextureDescriptorResource<Size>>(textures);
            resource->binding = bindingNumber;
            resource->type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            resource->stageFlags = stageFlags;
            resource->descriptorCount = static_cast<uint32_t>(Size);
            descriptorResources.push_back(std::move(resource));
            return *this;
        }
    }
}