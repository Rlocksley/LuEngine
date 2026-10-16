#pragma once

#include "Global.hpp"
#include "Constant.hpp"
#include "PhysicalDevice.hpp"
#include "Buffer.hpp"
#include "Image.hpp"
#include "flecs.h"
#include "component/Light.hpp"
#include "instance/LightInstance.hpp"
#include "DescriptorSet.hpp"
#include "ComputePipeline.hpp"
#include "CameraPackage.hpp"
#include "TransformPackage.hpp"
#include "Texture.hpp"

namespace Lu{
    namespace Core{
        class LightPackage{
        private:
            using LightEntity = flecs::entity_t;
            std::vector<Light> lights;
            std::vector<LightEntity> lightEntities;
            DualBuffer<Light> lightBuffer;
            DualBuffer<uint32_t> lightCountBuffer;
            BufferGpu<uint32_t> lightIndicesBuffer;
            uint64_t revision{1};
            std::array<uint64_t, MAX_FRAMES_IN_FLIGHT> uploadedRevision{};

            DescriptorSet cullDescrSet;
            ComputePipeline cullPipe;


        public:
            LightPackage(const CameraPackage& camera, const TransformPackage& transforms)
                : lightBuffer(MAX_LIGHTS, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT),
                  lightCountBuffer(1, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT),
                  lightIndicesBuffer((
                                     ((vkExtent2D.width+TILE_WIDTH-1u)/TILE_WIDTH) *
                                     ((vkExtent2D.height+TILE_HEIGHT-1u)/TILE_HEIGHT) *
                                     (MAX_LIGHTS_PER_TILE+1u)
                                    ),
                                    VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT)
                  {
                    createCullPipe(camera, transforms);
                  }


            void createCullPipe(const CameraPackage& camera, const TransformPackage& transforms){
                cullDescrSet
                .addUniformBuffer(0, camera.getCameraBuffer(), VK_SHADER_STAGE_COMPUTE_BIT)
                .addUniformBuffer(1, lightCountBuffer, VK_SHADER_STAGE_COMPUTE_BIT)
                .addStorageBuffer(2, lightBuffer, VK_SHADER_STAGE_COMPUTE_BIT)
                .addStorageBuffer(3, lightIndicesBuffer, VK_SHADER_STAGE_COMPUTE_BIT)
                .addTexture(4, depthMap, VK_SHADER_STAGE_COMPUTE_BIT)
                .addStorageBuffer(5, transforms.getTransformBuffer(), VK_SHADER_STAGE_COMPUTE_BIT)
                .build();

                cullPipe.create(
                    cullDescrSet.getVkPipelineLayout(),
                    ComputePipelineConfig{
                        .name = "light_cull",
                        .computeShader = "shader/light_cull.comp.spv"
                    }
                );
            }

            void createLight(LightEntity entity, const Component::Light& light, const uint32_t transformId) {
                const auto it = std::find(lightEntities.begin(), lightEntities.end(), entity);
                if(it == lightEntities.end()) {
                    LU_ASSERT(lights.size() < MAX_LIGHTS,
                        "LightPackage", "createLight", "Light capacity exceeded.")
                    lights.push_back(Light(light, transformId));
                    lightEntities.push_back(entity);
                } else {
                    lights[static_cast<size_t>(it - lightEntities.begin())] = Light(light, transformId);
                }
                ++revision;
            }

            void destroyLight(LightEntity entity) {
                const auto it = std::find(lightEntities.begin(), lightEntities.end(), entity);
                LU_ASSERT(it != lightEntities.end(),
                    "LightPackage", "destroyLight", "Light entity does not exist.")
                const size_t index = static_cast<size_t>(it - lightEntities.begin());
                lights.erase(lights.begin() + static_cast<ptrdiff_t>(index));
                lightEntities.erase(it);
                ++revision;
            }

            void copy(uint32_t frameIndex, const Command& command) {
                LU_ASSERT(frameIndex < MAX_FRAMES_IN_FLIGHT,
                    "LightPackage", "copy", "Frame index is out of range.")
                if(uploadedRevision[frameIndex] == revision) return;

                lightBuffer.clearCpu(frameIndex);
                if(!lights.empty()) {
                    lightBuffer.push_range_back(frameIndex, lights);
                    lightBuffer.copyToGpu(frameIndex, command);
                }

                lightCountBuffer.clearCpu(frameIndex);
                lightCountBuffer.push_back(frameIndex, static_cast<uint32_t>(lights.size()));
                lightCountBuffer.copyToGpu(frameIndex, command);
                uploadedRevision[frameIndex] = revision;

                vkCmdFillBuffer(
                    command.vkCommandBuffer, 
                    lightIndicesBuffer.getVkBuffer(frameIndex), 
                    0, 
                    VK_WHOLE_SIZE, 
                    0
                );
            }

            void cull(uint32_t frameIndex, const Command& command){
                vkCmdBindPipeline(
                    command.vkCommandBuffer,
                    VK_PIPELINE_BIND_POINT_COMPUTE,
                    cullPipe.getVkPipeline()
                );

                vkCmdBindDescriptorSets(
                    command.vkCommandBuffer,
                    VK_PIPELINE_BIND_POINT_COMPUTE,
                    cullDescrSet.getVkPipelineLayout(),
                    0,
                    1,
                    &cullDescrSet.getVkDescriptorSet(frameIndex),
                    0,
                    nullptr
                );

                vkCmdDispatch(
                    command.vkCommandBuffer,
                    (vkExtent2D.width + TILE_WIDTH - 1u) / TILE_WIDTH,
                    (vkExtent2D.height + TILE_HEIGHT - 1u) / TILE_HEIGHT,
                    1
                );
            }

            void updateDepthMapDescriptorSet() {
                const uint32_t tileCountX = (vkExtent2D.width + TILE_WIDTH - 1u) / TILE_WIDTH;
                const uint32_t tileCountY = (vkExtent2D.height + TILE_HEIGHT - 1u) / TILE_HEIGHT;
                const uint32_t requiredIndexCount =
                    tileCountX * tileCountY * (MAX_LIGHTS_PER_TILE + 1u);
                if (lightIndicesBuffer.getSize() < requiredIndexCount) {
                    lightIndicesBuffer = BufferGpu<uint32_t>(
                        requiredIndexCount,
                        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT
                    );
                }
                cullDescrSet.updateDescriptorSetWrites();
            }

            const DualBuffer<Light>& getLightBuffer() const { return lightBuffer; }
            const DualBuffer<uint32_t>& getLightCountBuffer() const { return lightCountBuffer; }
            const BufferGpu<uint32_t>& getLightIndicesBuffer() const { return lightIndicesBuffer; }
        };
    }
}