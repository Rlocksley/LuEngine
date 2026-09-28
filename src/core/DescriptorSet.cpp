#include "DescriptorSet.hpp"

#include "DescriptorPool.hpp"
#include "Device.hpp"

namespace Lu {
    namespace Core {
        DescriptorSet::~DescriptorSet() { resetVulkanObjects(); }

        DescriptorSet& DescriptorSet::addBuffer(uint32_t bindingNumber, VkDescriptorType descriptorType, const IBufferResource& buffer, VkShaderStageFlags stageFlags) {
            auto resource = std::make_unique<BufferDescriptorResource<0>>(buffer);
            resource->binding = bindingNumber;
            resource->type = descriptorType;
            resource->stageFlags = stageFlags;
            descriptorResources.push_back(std::move(resource));
            return *this;
        }

        DescriptorSet& DescriptorSet::addUniformBuffer(uint32_t bindingNumber, const IBufferResource& buffer, VkShaderStageFlags stageFlags) { 
            LU_ASSERT((buffer.getVkBufferUsage() & VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT) != 0, "DescriptorSet", "addUniformBuffer", "Buffer usage does not match descriptor type.")
            return addBuffer(bindingNumber, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, buffer, stageFlags); }
            
        DescriptorSet& DescriptorSet::addStorageBuffer(uint32_t bindingNumber, const IBufferResource& buffer, VkShaderStageFlags stageFlags) { 
            LU_ASSERT((buffer.getVkBufferUsage() & VK_BUFFER_USAGE_STORAGE_BUFFER_BIT) != 0, "DescriptorSet", "addStorageBuffer", "Buffer usage does not match descriptor type.")
            return addBuffer(bindingNumber, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, buffer, stageFlags); }

        DescriptorSet& DescriptorSet::addTexture(uint32_t bindingNumber, const Texture& texture, VkShaderStageFlags stageFlags) {
            LU_ASSERT((texture.usage & VK_IMAGE_USAGE_SAMPLED_BIT) != 0, "DescriptorSet", "addTexture", "Texture is missing VK_IMAGE_USAGE_SAMPLED_BIT.")
            auto resource = std::make_unique<TextureDescriptorResource<0>>(texture);
            resource->binding = bindingNumber;
            resource->type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            resource->stageFlags = stageFlags;
            descriptorResources.push_back(std::move(resource));
            return *this;
        }

        DescriptorSet& DescriptorSet::addPushConstantRange(VkShaderStageFlags stageFlags, uint32_t offset, uint32_t size) {
            pushConstantRanges.push_back({stageFlags, offset, size});
            return *this;
        }

        void DescriptorSet::resetVulkanObjects() {
            for (auto& descriptorSet : descriptorSets) {
                if (descriptorSet != VK_NULL_HANDLE) {
                    LU_CHECK_VULKAN(vkFreeDescriptorSets(vkDevice, vkDescriptorPool, 1, &descriptorSet), "DescriptorSet::resetVulkanObjects", "vkFreeDescriptorSets")
                    descriptorSet = VK_NULL_HANDLE;
                }
            }
            if (pipelineLayout != VK_NULL_HANDLE) {
                vkDestroyPipelineLayout(vkDevice, pipelineLayout, nullptr);
                pipelineLayout = VK_NULL_HANDLE;
            }
            if (descriptorSetLayout != VK_NULL_HANDLE) {
                vkDestroyDescriptorSetLayout(vkDevice, descriptorSetLayout, nullptr);
                descriptorSetLayout = VK_NULL_HANDLE;
            }
        }

        void DescriptorSet::build() {
            resetVulkanObjects();
            std::vector<VkDescriptorSetLayoutBinding> bindings;
            bindings.reserve(descriptorResources.size());
            for (const auto& resource : descriptorResources) {
                bindings.push_back({resource->binding, resource->type, resource->descriptorCount, resource->stageFlags, nullptr});
            }

            VkDescriptorSetLayoutCreateInfo layoutInfo{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
            layoutInfo.bindingCount = static_cast<uint32_t>(bindings.size());
            layoutInfo.pBindings = bindings.empty() ? nullptr : bindings.data();
            LU_CHECK_VULKAN(vkCreateDescriptorSetLayout(vkDevice, &layoutInfo, nullptr, &descriptorSetLayout), "DescriptorSet::build", "vkCreateDescriptorSetLayout")

            std::array<VkDescriptorSetLayout, MAX_FRAMES_IN_FLIGHT> layouts{};
            layouts.fill(descriptorSetLayout);
            VkDescriptorSetAllocateInfo allocateInfo{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
            allocateInfo.descriptorPool = vkDescriptorPool;
            allocateInfo.descriptorSetCount = MAX_FRAMES_IN_FLIGHT;
            allocateInfo.pSetLayouts = layouts.data();
            LU_CHECK_VULKAN(vkAllocateDescriptorSets(vkDevice, &allocateInfo, descriptorSets.data()), "DescriptorSet::build", "vkAllocateDescriptorSets")

            VkPipelineLayoutCreateInfo pipelineLayoutInfo{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
            pipelineLayoutInfo.setLayoutCount = 1;
            pipelineLayoutInfo.pSetLayouts = &descriptorSetLayout;
            pipelineLayoutInfo.pushConstantRangeCount = static_cast<uint32_t>(pushConstantRanges.size());
            pipelineLayoutInfo.pPushConstantRanges = pushConstantRanges.empty() ? nullptr : pushConstantRanges.data();
            LU_CHECK_VULKAN(vkCreatePipelineLayout(vkDevice, &pipelineLayoutInfo, nullptr, &pipelineLayout), "DescriptorSet::build", "vkCreatePipelineLayout")
            updateDescriptorSetWrites();
        }

        void DescriptorSet::updateDescriptorSetWrites() {
            for (uint32_t frameIndex = 0; frameIndex < MAX_FRAMES_IN_FLIGHT; ++frameIndex) 
            {
                updateDescriptorSetWrites(frameIndex);
            }
        }

        void DescriptorSet::updateDescriptorSetWrites(uint32_t frameIndex) {
            std::vector<VkDescriptorBufferInfo> bufferInfos;
            std::vector<std::vector<VkDescriptorBufferInfo>> bufferArrays;
            std::vector<std::vector<VkDescriptorImageInfo>> imageArrays;
            std::vector<VkWriteDescriptorSet> writes;
            bufferInfos.reserve(descriptorResources.size());
            bufferArrays.reserve(descriptorResources.size());
            imageArrays.reserve(descriptorResources.size());
            writes.reserve(descriptorResources.size());

            for (const auto& resource : descriptorResources) {
                VkWriteDescriptorSet write{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
                write.dstSet = descriptorSets[frameIndex];
                write.dstBinding = resource->binding;
                write.descriptorType = resource->type;
                write.descriptorCount = resource->descriptorCount;
                if (resource->type == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER || resource->type == VK_DESCRIPTOR_TYPE_STORAGE_BUFFER) {
                    bufferArrays.emplace_back(resource->descriptorCount);
                    for (uint32_t i = 0; i < resource->descriptorCount; ++i) bufferArrays.back()[i] = {resource->getVkBuffer(frameIndex, i), 0, VK_WHOLE_SIZE};
                    write.pBufferInfo = bufferArrays.back().data();
                } else if (resource->type == VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER) {
                    imageArrays.emplace_back(resource->descriptorCount);
                    for (uint32_t i = 0; i < resource->descriptorCount; ++i) imageArrays.back()[i] = resource->getVkDescriptorImageInfo(i);
                    write.pImageInfo = imageArrays.back().data();
                } else {
                    LU_LOGE("DescriptorSet", "updateDescriptorSetWrites", "Unsupported descriptor type in resources.")
                }
                writes.push_back(write);
            }
            if (!writes.empty()) vkUpdateDescriptorSets(vkDevice, static_cast<uint32_t>(writes.size()), writes.data(), 0, nullptr);
        }
    }
}