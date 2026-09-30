#pragma once
#include "Global.hpp"
#include "Buffer.hpp"
#include "flecs.h"
#include "instance/TransformInstance.hpp"
#include "component/Transform.hpp"
#include "DescriptorSet.hpp"
#include "ComputePipeline.hpp"
#include "TripleBuffers.hpp"
#include "DirtyTracker.hpp"
#include "PagedSparseMap.hpp"

#include <cstddef>

namespace Lu{
    namespace Core{        


        static_assert(sizeof(Transform) == 176);
        static_assert(offsetof(TransformInstance, id) == 176);
        static_assert(sizeof(TransformInstance) == 192);

        class TransformPackage{
        private:
            DualBuffer<TransformInstance> instanceBuffer;
            SlotBuffer<Transform> transformBuffer;

            DescriptorSet transferDescrSet;
            ComputePipeline transferPipe;

            PagedSparseMap<TransformEntity, TransformId> entityToId;
            std::vector<Transform> transforms;                       // indexed by TransformId
            DirtyTracker<TransformId> dirty{MAX_FRAMES_IN_FLIGHT};
 

            uint32_t instanceBufferSize{0};

        public:
            TransformPackage()
                : instanceBuffer(MAX_TRANSFORMS, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT),
                transformBuffer(MAX_TRANSFORMS, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT)
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
                        .name = "transform_transfer",
                        .computeShader = "shader/transform_transfer.comp.spv"
                    }
                );
            }

            const SlotBuffer<Transform>& getTransformBuffer() const { return transformBuffer; }

            const TransformId getTransformId(const TransformEntity entity){
                const TransformId id = entityToId.get(entity);
                LU_ASSERT(id != entityToId.kInvalid, "TransformPackage", "getTransformId", "No TransformId for given TransformEntity");
                return id;    
            }

            void createTransform(TransformEntity e, const Transform& t) {
                    TransformId id = entityToId.get(e);
                    if (id == entityToId.kInvalid) {
                        id = transformBuffer.allocate();
                        if (id >= transforms.size()) transforms.resize(std::size_t{id} + 1);
                        entityToId.set(e, id);
                    }
                    transforms[id] = t;
                    dirty.mark(id);
            }

            void updateTransforms() {
                auto& tripleBuffer = GetTransformTripleBuffer();
                
                if(!tripleBuffer.update()){
                    return;
                }

                for (const auto& u : tripleBuffer.read_slot()) {
                    const TransformId id = entityToId.get(u.entity);
                    if (id == entityToId.kInvalid) continue;
                    transforms[id] = u.transform;
                    dirty.mark(id);
                }
            }

            void destroyTransform(TransformEntity e) {
                const TransformId id = entityToId.get(e);
                transformBuffer.free(id); entityToId.erase(e); dirty.cancel(id);
            }

            void collectDirty(uint32_t f) {
                dirty.collect([&](TransformId id) { instanceBuffer.push_back(f, {transforms[id], id}); });
            }


            void copy(uint32_t frameIndex, const Command& cmd) {
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
                    (instanceBufferSize + 256-1)/256,
                    1,
                    1
                );
            }
        };
    }
}
