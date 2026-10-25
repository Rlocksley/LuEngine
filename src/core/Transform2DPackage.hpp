#pragma once

#include "Global.hpp"
#include "Buffer.hpp"
#include "flecs.h"
#include "instance/Transform2DInstance.hpp"
#include "DescriptorSet.hpp"
#include "ComputePipeline.hpp"
#include "TripleBuffers.hpp"
#include "DirtyTracker.hpp"

namespace Lu{
    namespace Core{

        // Same job as TransformPackage, for Component::Transform2D:
        //   Transform2DInstanceBuffer (elements (Transform2D, id), uploaded per frame)
        //     --transform2d_transfer.comp--> Transform2DBuffer (element i = Transform2D with id i)
        class Transform2DPackage{
        private:
            DualBuffer<Transform2DInstance> instanceBuffer;
            SlotBuffer<Transform2D> transformBuffer;

            DescriptorSet transferDescrSet;
            ComputePipeline transferPipe;

            std::unordered_map<Transform2DEntity, Transform2DId> entityToId;
            std::vector<Transform2D> transforms;                       // CPU mirror, indexed by Transform2DId
            DirtyTracker<Transform2DId> dirty{MAX_FRAMES_IN_FLIGHT};

            uint32_t instanceBufferSize{0};

        public:
            Transform2DPackage()
                : instanceBuffer(MAX_TRANSFORMS_2D, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT),
                transformBuffer(MAX_TRANSFORMS_2D, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT)
            {
                createTransferPipe();
            }

            void createTransferPipe() {
                transferDescrSet
                .addStorageBuffer(0, instanceBuffer, VK_SHADER_STAGE_COMPUTE_BIT)
                .addStorageBuffer(1, transformBuffer, VK_SHADER_STAGE_COMPUTE_BIT)
                .addPushConstantRange(VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(uint32_t))
                .build();

                transferPipe.create(
                    transferDescrSet.getVkPipelineLayout(),
                    ComputePipelineConfig{
                        .name = "transform2d_transfer",
                        .computeShader = "shader/transform2d_transfer.comp.spv"
                    }
                );
            }

            const SlotBuffer<Transform2D>& getTransform2DBuffer() const { return transformBuffer; }

            Transform2DId getTransform2DId(const Transform2DEntity entity) const {
                const auto it = entityToId.find(entity);
                LU_ASSERT(it != entityToId.end(), "Transform2DPackage", "getTransform2DId", "No Transform2DId for given Transform2DEntity");
                return it->second;
            }

            // EcsRequest::CreateTransform2D (upsert)
            void createTransform2D(const Transform2DEntity entity, const Component::Transform2D& transform) {
                Transform2DId id;
                const auto it = entityToId.find(entity);
                if (it == entityToId.end()) {
                    id = transformBuffer.allocate();
                    if (id >= transforms.size()) transforms.resize(std::size_t{id} + 1);
                    entityToId.emplace(entity, id);
                } else {
                    id = it->second;
                }
                transforms[id] = Transform2D(transform);
                dirty.mark(id);
            }

            // EcsRequest::UpdateTransform2D: only changes existing transforms
            void updateTransform2D(const Transform2DEntity entity, const Component::Transform2D& transform) {
                const auto it = entityToId.find(entity);
                if (it == entityToId.end()) return;
                transforms[it->second] = Transform2D(transform);
                dirty.mark(it->second);
            }

            // EcsRequest::DestroyTransform2D
            void destroyTransform2D(const Transform2DEntity entity) {
                const auto it = entityToId.find(entity);
                if (it == entityToId.end()) return;
                const Transform2DId id = it->second;
                transformBuffer.free(id);
                dirty.cancel(id);
                entityToId.erase(it);
            }

            // Latest per-frame values written by the ECS thread (Transform2D triple buffer)
            void updateTransforms() {
                auto& tripleBuffer = GetTransform2DTripleBuffer();
                if (tripleBuffer.update()) {
                    for (const auto& u : tripleBuffer.read_slot()) {
                        const auto it = entityToId.find(u.entity);
                        if (it == entityToId.end()) continue;
                        const Transform2D value(u.transform);
                        // only upload what really changed
                        if (std::memcmp(&transforms[it->second], &value, sizeof(Transform2D)) == 0) continue;
                        transforms[it->second] = value;
                        dirty.mark(it->second);
                    }
                }
            }

            void collectDirty(const uint32_t frameIndex) {
                dirty.collect(
                    [&](Transform2DId id) { 
                        instanceBuffer.push_back(frameIndex, {transforms[id], id}); 
                    }
                );
            }

            void copy(const uint32_t frameIndex, const Command& cmd) {
                if ((instanceBufferSize = instanceBuffer.sizeCpu(frameIndex)) == 0) {
                    return;
                }
                instanceBuffer.copyToGpu(frameIndex, cmd);
            }

            void transfer(const uint32_t frameIndex, const Command& cmd) const {
                if (instanceBufferSize == 0) {
                    return;
                }

                vkCmdBindPipeline(
                    cmd.vkCommandBuffer, 
                    VK_PIPELINE_BIND_POINT_COMPUTE, 
                    transferPipe.getVkPipeline()
                );

                vkCmdPushConstants(
                    cmd.vkCommandBuffer, 
                    transferDescrSet.getVkPipelineLayout(),
                    VK_SHADER_STAGE_COMPUTE_BIT, 
                    0, 
                    sizeof(uint32_t), 
                    &instanceBufferSize
                );

                vkCmdBindDescriptorSets(
                    cmd.vkCommandBuffer, 
                    VK_PIPELINE_BIND_POINT_COMPUTE,
                    transferDescrSet.getVkPipelineLayout(), 
                    0, 
                    1,
                    &transferDescrSet.getVkDescriptorSet(frameIndex),
                    0, 
                    nullptr
                );

                vkCmdDispatch(
                    cmd.vkCommandBuffer, 
                    (instanceBufferSize + 256 - 1) / 256, 
                    1, 
                    1
                );
            }
        };
    }
}
