#pragma once
#include "Global.hpp"
#include "Constant.hpp"
#include "Buffer.hpp"
#include "Command.hpp"
#include "ComputePipeline.hpp"
#include "DescriptorSet.hpp"
#include "DirtyTracker.hpp"
#include "flecs.h"
#include "instance/Clip2DInstance.hpp"

namespace Lu::Core {
    class Clip2DPackage {
        DualBuffer<Clip2DInstance> clip2DInstanceArray;
        SlotBuffer<Clip2D> clip2Darray;
        DescriptorSet transferSet;
        ComputePipeline transferPipe;
        std::unordered_map<flecs::entity_t, Clip2DId> entityToId;
        std::vector<Clip2D> clips;
        DirtyTracker<Clip2DId> dirty{MAX_FRAMES_IN_FLIGHT};
        uint32_t instanceCount{0};
    public:
        Clip2DPackage()
            : clip2DInstanceArray(MAX_CLIPS_2D, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT),
              clip2Darray(MAX_CLIPS_2D, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT) {
            transferSet.addStorageBuffer(0, clip2DInstanceArray, VK_SHADER_STAGE_COMPUTE_BIT)
                .addStorageBuffer(1, clip2Darray, VK_SHADER_STAGE_COMPUTE_BIT)
                .addPushConstantRange(VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(uint32_t))
                .build();
            transferPipe.create(transferSet.getVkPipelineLayout(), ComputePipelineConfig{
                .name = "clip2d_transfer",
                .computeShader = "shader/clip2d_transfer.comp.spv"
            });
        }

        const SlotBuffer<Clip2D>& getClip2DBuffer() const { return clip2Darray; }

        uint32_t getClip2DId(flecs::entity_t e) const {
            const auto it = entityToId.find(e);
            LU_ASSERT(it != entityToId.end(), "Clip2DPackage", "getClip2DId", "No Clip2D slot for entity")
            return it->second;
        }

        void createClip2D(flecs::entity_t e, const Component::Clip2D& c) {
            Clip2DId id;
            const auto it = entityToId.find(e);
            if (it == entityToId.end()) {
                id = clip2Darray.allocate();
                if (id >= clips.size()) clips.resize(std::size_t{id} + 1);
                entityToId.emplace(e, id);
            } else {
                id = it->second;
            }
            clips[id] = Clip2D(c);
            dirty.mark(id);
        }

        void destroyClip2D(flecs::entity_t e) {
            const auto it = entityToId.find(e);
            if (it == entityToId.end()) return;
            const Clip2DId id = it->second;
            clip2Darray.free(id);
            dirty.cancel(id);
            entityToId.erase(it);
        }

        void collectDirty(uint32_t frame) {
            dirty.collect([&](Clip2DId id) {
                clip2DInstanceArray.push_back(frame, Clip2DInstance{clips[id], id});
            });
        }

        void copy(uint32_t frame, const Command& cmd) {
            instanceCount = clip2DInstanceArray.sizeCpu(frame);
            if (instanceCount) clip2DInstanceArray.copyToGpu(frame, cmd);
        }

        void transfer(uint32_t frame, const Command& cmd) const {
            if (!instanceCount) return;
            vkCmdBindPipeline(cmd.vkCommandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, transferPipe.getVkPipeline());
            vkCmdPushConstants(cmd.vkCommandBuffer, transferSet.getVkPipelineLayout(), VK_SHADER_STAGE_COMPUTE_BIT,
                0, sizeof(uint32_t), &instanceCount);
            vkCmdBindDescriptorSets(cmd.vkCommandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE,
                transferSet.getVkPipelineLayout(), 0, 1, &transferSet.getVkDescriptorSet(frame), 0, nullptr);
            vkCmdDispatch(cmd.vkCommandBuffer, (instanceCount + 255) / 256, 1, 1);
        }
    };
}
