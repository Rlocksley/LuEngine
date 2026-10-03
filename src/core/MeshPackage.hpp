#pragma once
#include "Global.hpp"
#include "Constant.hpp"
#include "Buffer.hpp"
#include "Command.hpp"
#include "ComputePipeline.hpp"
#include "DescriptorSet.hpp"
#include "GraphicsPipeline.hpp"
#include "flecs.h"
#include "Vertex.hpp"
#include "instance/MeshInstance.hpp"
#include "CameraPackage.hpp"
#include "TransformPackage.hpp"
#include "GeometryPackage.hpp"
#include "PipelineConfig.hpp"
#include <cstddef>

namespace Lu{
    namespace Core{

    class MeshPackage{
        private:
            
            
            DualBuffer<MeshInstance> instanceBuffer;
            SlotBuffer<Mesh> meshBuffer;
            std::array<std::shared_ptr<IBufferResource>, MAX_MESH_PIPELINES> indirectBufferArray;
            BufferGpu<uint32_t> drawCountBuffer;
            BufferGpuIndexed<uint32_t> capacityBuffer;

            DescriptorSet transferDescrSet;
            ComputePipeline transferPipe;
            DescriptorSet cullDescrSet;
            ComputePipeline cullPipe;
            DescriptorSet meshDescrSet;
            uint32_t pipeArraySize{0};
            std::array<uint32_t, MAX_MESH_PIPELINES> indirectDrawCapacity{};
            std::array<GraphicsPipeline<Vertex::Mesh>, MAX_MESH_PIPELINES> meshPipes{};

            using MeshEntity = flecs::entity_t;
            std::unordered_map<MeshEntity, MeshId> entityToMeshId;
            std::unordered_map<MeshId, Mesh> meshIdToMesh;
            std::unordered_map<MeshId, uint32_t> dirtyIds;

            using MeshPipeEntity = flecs::entity_t;
            using MeshPipeId = uint32_t;
            std::unordered_map<MeshPipeEntity, MeshPipeId> entityToPipeId; 
    
            using MeshInfoEntity = flecs::entity_t;
            using TransformEntity = flecs::entity_t;

            Command command;

            uint32_t instanceBufferSize{0};

        public:
        MeshPackage(const CameraPackage& camera, const TransformPackage& transforms, const GeometryPackage<Vertex::Mesh>& geometry)
            : instanceBuffer(MAX_ECS_REQUESTS_PROCESSED_PER_FRAME*MAX_FRAMES_IN_FLIGHT, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT),
            meshBuffer(MAX_MESHES, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT),
            indirectBufferArray(initIndirectBufferArray()),
            drawCountBuffer(MAX_MESH_PIPELINES, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT),
            capacityBuffer(MAX_MESH_PIPELINES, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT),
            command()
        {
            command.begin();
            for (uint32_t frameIndex = 0; frameIndex < MAX_FRAMES_IN_FLIGHT; ++frameIndex) {
                vkCmdFillBuffer(
                    command.vkCommandBuffer,
                    meshBuffer.getVkBuffer(frameIndex),
                    0,
                    VK_WHOLE_SIZE,
                    0
                );
            }
            recordBarrier(
                command.vkCommandBuffer,
                VK_PIPELINE_STAGE_TRANSFER_BIT,
                VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_VERTEX_SHADER_BIT,
                VK_ACCESS_TRANSFER_WRITE_BIT,
                VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT
            );
            command.end();
            command.submit();
            command.waitForFence();

            createTransferPipe();
            createCullPipe(camera, transforms, geometry);
            createMeshDescriptorSet(camera, transforms);
        }

        ~MeshPackage(){}

        MeshPipeId getMeshPipeId(const MeshPipeEntity entity){
            auto it = entityToPipeId.find(entity);
            LU_ASSERT(it != entityToPipeId.end(), "MeshPackage", "getMeshPipeId", "No PipeId for given MeshPipeEntity");
            return it->second;
        }


        void createTransferPipe() {
            transferDescrSet
            .addStorageBuffer(0, instanceBuffer, VK_SHADER_STAGE_COMPUTE_BIT)
            .addStorageBuffer(1, meshBuffer, VK_SHADER_STAGE_COMPUTE_BIT)
            .addPushConstantRange(VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(uint32_t))
            .build();


            transferPipe.create(
                transferDescrSet.getVkPipelineLayout(), 
                ComputePipelineConfig{
                    .name = "mesh_transfer",
                    .computeShader = "shader/mesh_transfer.comp.spv"
                }
            );
        }

        void createCullPipe(const CameraPackage& camera, const TransformPackage& transforms, const GeometryPackage<Vertex::Mesh>& geometry) {
            cullDescrSet
            .addUniformBuffer(0, camera.getCameraBuffer(), VK_SHADER_STAGE_COMPUTE_BIT)
            .addStorageBuffer(1, transforms.getTransformBuffer(), VK_SHADER_STAGE_COMPUTE_BIT)
            .addStorageBuffer(2, geometry.getGeometryInfoBuffer(), VK_SHADER_STAGE_COMPUTE_BIT)
            .addStorageBuffer(3, meshBuffer, VK_SHADER_STAGE_COMPUTE_BIT)
            .addStorageBuffer(4, drawCountBuffer, VK_SHADER_STAGE_COMPUTE_BIT)
            .addStorageBuffer(5, capacityBuffer, VK_SHADER_STAGE_COMPUTE_BIT)
            .addStorageBufferArray(6, indirectBufferArray, VK_SHADER_STAGE_COMPUTE_BIT)
            .build();

            cullPipe.create(
                cullDescrSet.getVkPipelineLayout(),
                ComputePipelineConfig{
                    .name = "mesh_cull",
                    .computeShader = "shader/mesh_cull.comp.spv"
                }
            );
        }

        void createMeshDescriptorSet(const CameraPackage& camera, const TransformPackage& transforms){
            meshDescrSet
            .addUniformBuffer(0, camera.getCameraBuffer(), VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT)
            .addStorageBuffer(1, transforms.getTransformBuffer(), VK_SHADER_STAGE_VERTEX_BIT)
            .addStorageBuffer(2, meshBuffer, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT)
            .build();
        }

        void createTransferPipeline(const ComputePipelineConfig& config) {
            transferPipe.create(transferDescrSet.getVkPipelineLayout(), config);
        }

        void createCullPipeline(const ComputePipelineConfig& config) {
            cullPipe.create(cullDescrSet.getVkPipelineLayout(), config);
        }

        void createMeshPipeline(const MeshPipeEntity entity, const GraphicsPipelineConfig& config) {
            LU_ASSERT(
                pipeArraySize < MAX_MESH_PIPELINES,
                "MeshPackage",
                "createMeshPipeline",
                "MeshPipelineArray size overflow"
            );
            const MeshPipeId pipeId = pipeArraySize++;
            indirectBufferArray[pipeId] = std::make_shared<BufferGpu<VkDrawIndexedIndirectCommand>>(
                config.capacity,
                VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT |
                VK_BUFFER_USAGE_TRANSFER_DST_BIT
            );
            indirectDrawCapacity[pipeId] = config.capacity;

            cullDescrSet.updateDescriptorSetWrites();

            command.begin();
            capacityBuffer.upload(pipeId, config.capacity, command);
            command.end();
            command.submit();
            command.waitForFence();

            entityToPipeId[entity] = pipeId;
            meshPipes[pipeId].create(
                meshDescrSet.getVkPipelineLayout(),
                false,
                config
            );
        }

    
        void createMesh(const MeshEntity meshEntity, const MeshPipeEntity meshPipeEntity, 
                        const TransformId transformId, const GeometryInfoId meshInfoId,
                        const Component::Material& material) {
            
            auto pipeIt = entityToPipeId.find(meshPipeEntity);
            LU_ASSERT(pipeIt != entityToPipeId.end(), "MeshPackage", "createMesh", "No PipelineId for given MeshPipeline-Entity");

            Mesh mesh(material, transformId, pipeIt->second, meshInfoId);

            const auto it = entityToMeshId.find(meshEntity);
            if (it == entityToMeshId.end()) {
                //create
                const MeshId id = meshBuffer.allocate();
                entityToMeshId[meshEntity] = id;
                meshIdToMesh[id] = mesh;
                dirtyIds[id] = MAX_FRAMES_IN_FLIGHT;
            }else{
                //update
                meshIdToMesh[it->second] = mesh;
                dirtyIds[it->second] = MAX_FRAMES_IN_FLIGHT;
            }
        }

        void destroyMesh(const MeshEntity entity) {
            const auto it = entityToMeshId.find(entity);

            LU_ASSERT(
                it != entityToMeshId.end(),
                "MeshPackage",
                "destroyMesh",
                "MeshEntity does not exist."
            );

            const MeshId id = it->second;
            meshBuffer.free(id);
            entityToMeshId.erase(entity);
            meshIdToMesh.erase(id);
            dirtyIds[id] = MAX_FRAMES_IN_FLIGHT;
        }

        void collectDirty(uint32_t frameIndex) {
            std::erase_if(dirtyIds, [&](auto& pair) {
                auto& [id, framesRemaining] = pair;

                const auto it = meshIdToMesh.find(id);
                if (it != meshIdToMesh.end()) {
                    instanceBuffer.push_back(
                        frameIndex,
                        MeshInstance{it->second, id}
                    );
                } else {
                    instanceBuffer.push_back(
                        frameIndex,
                        MeshInstance{Mesh{}, id}
                    );
                }

                --framesRemaining;
                return framesRemaining <= 0;
            });
        }

        void zeroOut(uint32_t frameIndex, const Command& cmd){
            vkCmdFillBuffer(
                cmd.vkCommandBuffer,
                drawCountBuffer.getVkBuffer(frameIndex),
                0,
                VK_WHOLE_SIZE,
                0
            );
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

        void cull(const uint32_t frameIndex, const Command& cmd) const {
            vkCmdBindPipeline(
                cmd.vkCommandBuffer,
                VK_PIPELINE_BIND_POINT_COMPUTE,
                cullPipe.getVkPipeline()
            );
            vkCmdBindDescriptorSets(
                cmd.vkCommandBuffer,
                VK_PIPELINE_BIND_POINT_COMPUTE,
                cullDescrSet.getVkPipelineLayout(),
                0,
                1,
                &cullDescrSet.getVkDescriptorSet(frameIndex),
                0,
                nullptr
            );

            vkCmdDispatch(
                cmd.vkCommandBuffer,
                (meshBuffer.size() + 256-1)/256,
                1,
                1
            );
        }


        void draw(const uint32_t frameIndex, const GeometryPackage<Vertex::Mesh>& geometry, const Command& command) const {
            const VkBuffer vertexBuffer = geometry.getVertexBuffer();
            const VkBuffer indexBuffer = geometry.getIndexBuffer();
            VkDeviceSize offsets[] = {0};

            vkCmdBindVertexBuffers(command.vkCommandBuffer, 0, 1, &vertexBuffer, offsets);
            vkCmdBindIndexBuffer(command.vkCommandBuffer, indexBuffer, 0, VK_INDEX_TYPE_UINT32);

            vkCmdBindDescriptorSets(
                command.vkCommandBuffer,
                VK_PIPELINE_BIND_POINT_GRAPHICS,
                meshDescrSet.getVkPipelineLayout(),
                0,
                1,
                &meshDescrSet.getVkDescriptorSet(frameIndex),
                0,
                nullptr
            );

            for (uint32_t pipeId = 0; pipeId < pipeArraySize; ++pipeId) {
                const auto& indirectBuffer = indirectBufferArray[pipeId];
                
                vkCmdBindPipeline(
                    command.vkCommandBuffer,
                    VK_PIPELINE_BIND_POINT_GRAPHICS,
                    meshPipes[pipeId].getVkPipeline()
                );

                vkCmdDrawIndexedIndirectCount(
                    command.vkCommandBuffer,
                    indirectBuffer->getVkBuffer(frameIndex),
                    0,
                    drawCountBuffer.getVkBuffer(frameIndex),
                    static_cast<VkDeviceSize>(pipeId) * sizeof(uint32_t),
                    indirectDrawCapacity[pipeId],
                    sizeof(VkDrawIndexedIndirectCommand)
                );
            }
        }


        private:
        static std::shared_ptr<IBufferResource> initDummyIndirectBuffer() {
            return std::make_shared<BufferGpu<VkDrawIndexedIndirectCommand>>(
                1,
                VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT |
                VK_BUFFER_USAGE_TRANSFER_DST_BIT
            );
        }

        static std::array<std::shared_ptr<IBufferResource>, MAX_MESH_PIPELINES> initIndirectBufferArray() {
            std::array<std::shared_ptr<IBufferResource>, MAX_MESH_PIPELINES> buffers;
            buffers.fill(initDummyIndirectBuffer());
            return buffers;
        }

        void recordBarrier(
            VkCommandBuffer commandBuffer,
            VkPipelineStageFlags srcStage,
            VkPipelineStageFlags dstStage,
            VkAccessFlags srcAccess,
            VkAccessFlags dstAccess
        ) {
            VkMemoryBarrier barrier{};
            barrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
            barrier.srcAccessMask = srcAccess;
            barrier.dstAccessMask = dstAccess;
            vkCmdPipelineBarrier(
                commandBuffer,
                srcStage,
                dstStage,
                0,
                1,
                &barrier,
                0,
                nullptr,
                0,
                nullptr
            );
        }
   };
}
}
