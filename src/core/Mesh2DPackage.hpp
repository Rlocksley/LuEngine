#pragma once
#include "Global.hpp"
#include "Constant.hpp"
#include "Buffer.hpp"
#include "Command.hpp"
#include "ComputePipeline.hpp"
#include "DescriptorSet.hpp"
#include "DirtyTracker.hpp"
#include "GraphicsPipeline.hpp"
#include "flecs.h"
#include "Vertex.hpp"
#include "instance/Mesh2DInstance.hpp"
#include "Camera2DPackage.hpp"
#include "Transform2DPackage.hpp"
#include "GeometryPackage.hpp"
#include "TexturePackage.hpp"
#include "PipelineConfig.hpp"

namespace Lu{
    namespace Core{

        using Mesh2DPipeId = uint32_t;

        // The 2D counterpart of MeshPackage (GUI meshes: panels, buttons, icons ...), without depth prepass and lights:
        //
        //   Mesh2DInstanceBuffer (elements (Mesh2D, id)) --mesh2d_transfer.comp--> Mesh2DBuffer (element i = Mesh2D with id i)
        //   --mesh2d_cull.comp--> one VkDrawIndexedIndirectCommand per visible mesh in the indirect buffer of its pipeline
        //   --vkCmdDrawIndexedIndirectCount--> per Mesh2D pipeline (GraphicsPipeline<Vertex::Vertex2D>)
        //
        // The geometry (vertices / indices of the registered 2D shapes) lives in the Renderer's mesh2DGeometry.
        // All Mesh2D pipelines share one descriptor set (set 0):
        //   binding 0  uniform Camera2DBuffer      (vertex, fragment)   the Camera2D
        //   binding 1  buffer  Transform2DBuffer   (vertex, fragment)   all mirrored Transform2D
        //   binding 2  buffer  Mesh2DBuffer        (vertex, fragment)   all Mesh2D, element gl_InstanceIndex = this mesh
        //   binding 3  sampler2D textureArray[]    (fragment)           the global texture array
        class Mesh2DPackage{
        private:
            DualBuffer<Mesh2DInstance> instanceBuffer;
            SlotBuffer<Mesh2D> meshBuffer;
            std::array<std::shared_ptr<IBufferResource>, MAX_MESH2D_PIPELINES> indirectBufferArray;
            BufferGpu<uint32_t> drawCountBuffer;
            BufferGpuIndexed<uint32_t> capacityBuffer;

            DescriptorSet transferDescrSet;
            ComputePipeline transferPipe;
            DescriptorSet cullDescrSet;
            ComputePipeline cullPipe;
            DescriptorSet meshDescrSet;
            uint32_t pipeArraySize{0};
            std::array<uint32_t, MAX_MESH2D_PIPELINES> indirectDrawCapacity{};
            std::array<GraphicsPipeline<Vertex::Vertex2D>, MAX_MESH2D_PIPELINES> meshPipes{};

            using Mesh2DEntity = flecs::entity_t;
            using Mesh2DPipeEntity = flecs::entity_t;
            std::unordered_map<Mesh2DEntity, Mesh2DId> entityToMeshId;
            std::unordered_map<Mesh2DPipeEntity, Mesh2DPipeId> entityToPipeId;
            std::vector<Mesh2D> meshes;                                 // CPU mirror of the Mesh2DBuffer, indexed by Mesh2DId
            DirtyTracker<Mesh2DId> dirty{MAX_FRAMES_IN_FLIGHT};

            Command command;

            uint32_t instanceBufferSize{0};

        public:
            Mesh2DPackage(const Camera2DPackage& camera2D, const Transform2DPackage& transforms2D,
                          const TexturePackage& textures, const GeometryPackage<Vertex::Vertex2D>& geometry)
                : instanceBuffer(MAX_ECS_REQUESTS_PROCESSED_PER_FRAME * MAX_FRAMES_IN_FLIGHT, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT),
                meshBuffer(MAX_MESHES_2D, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT),
                indirectBufferArray(initIndirectBufferArray()),
                drawCountBuffer(MAX_MESH2D_PIPELINES, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT),
                capacityBuffer(MAX_MESH2D_PIPELINES, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT),
                command()
            {
                // Zero the Mesh2DBuffer: valid == 0 everywhere, so slots that were never written draw nothing.
                command.begin();
                for (uint32_t frameIndex = 0; frameIndex < MAX_FRAMES_IN_FLIGHT; ++frameIndex) {
                    vkCmdFillBuffer(command.vkCommandBuffer, meshBuffer.getVkBuffer(frameIndex), 0, VK_WHOLE_SIZE, 0);
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
                command.resetFence();

                createTransferPipe();
                createCullPipe(camera2D, transforms2D, geometry);
                createMeshDescriptorSet(camera2D, transforms2D, textures);
            }

            Mesh2DPipeId getMesh2DPipeId(const Mesh2DPipeEntity entity) const {
                const auto it = entityToPipeId.find(entity);
                LU_ASSERT(it != entityToPipeId.end(), "Mesh2DPackage", "getMesh2DPipeId", "No Mesh2DPipeId for given entity. Was it registered with App::registerMesh2DPipe?");
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
                        .name = "mesh2d_transfer",
                        .computeShader = "shader/mesh2d_transfer.comp.spv"
                    }
                );
            }

            void createCullPipe(const Camera2DPackage& camera2D, const Transform2DPackage& transforms2D,
                                const GeometryPackage<Vertex::Vertex2D>& geometry) {
                cullDescrSet
                .addUniformBuffer(0, camera2D.getCameraBuffer(), VK_SHADER_STAGE_COMPUTE_BIT)
                .addStorageBuffer(1, transforms2D.getTransform2DBuffer(), VK_SHADER_STAGE_COMPUTE_BIT)
                .addStorageBuffer(2, geometry.getGeometryInfoBuffer(), VK_SHADER_STAGE_COMPUTE_BIT)
                .addStorageBuffer(3, meshBuffer, VK_SHADER_STAGE_COMPUTE_BIT)
                .addStorageBuffer(4, drawCountBuffer, VK_SHADER_STAGE_COMPUTE_BIT)
                .addStorageBuffer(5, capacityBuffer, VK_SHADER_STAGE_COMPUTE_BIT)
                .addStorageBufferArray(6, indirectBufferArray, VK_SHADER_STAGE_COMPUTE_BIT)
                .build();

                cullPipe.create(
                    cullDescrSet.getVkPipelineLayout(),
                    ComputePipelineConfig{
                        .name = "mesh2d_cull",
                        .computeShader = "shader/mesh2d_cull.comp.spv"
                    }
                );
            }

            void createMeshDescriptorSet(const Camera2DPackage& camera2D, const Transform2DPackage& transforms2D,
                                         const TexturePackage& textures) {
                meshDescrSet
                .addUniformBuffer(0, camera2D.getCameraBuffer(), VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT)
                .addStorageBuffer(1, transforms2D.getTransform2DBuffer(), VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT)
                .addStorageBuffer(2, meshBuffer, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT)
                .addTextureArray(3, textures.getTextureArray(), VK_SHADER_STAGE_FRAGMENT_BIT)
                .build();
            }

            // App::registerMesh2DPipe
            void createMesh2DPipeline(const Mesh2DPipeEntity entity, const GraphicsPipelineConfig& config) {
                LU_ASSERT(pipeArraySize < MAX_MESH2D_PIPELINES, "Mesh2DPackage", "createMesh2DPipeline", "Mesh2DPipelineArray size overflow")
                LU_ASSERT(entityToPipeId.find(entity) == entityToPipeId.end(), "Mesh2DPackage", "createMesh2DPipeline", "Entity already is a Mesh2D pipeline.")
                LU_ASSERT(config.capacity > 0, "Mesh2DPackage", "createMesh2DPipeline", "GraphicsPipelineConfig::capacity must be bigger 0")

                const Mesh2DPipeId pipeId = pipeArraySize++;
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
                command.resetFence();

                entityToPipeId[entity] = pipeId;
                meshPipes[pipeId].create(meshDescrSet.getVkPipelineLayout(), false, config);
            }

            // EcsRequest::CreateMesh2D (upsert)
            void createMesh2D(const Mesh2DEntity entity, const Mesh2DPipeEntity pipeEntity,
                              const Transform2DId transform2DId, const GeometryInfoId meshInfoId,
                              const uint32_t textureId, const glm::vec4& color,
                              const glm::vec4& uvRect, const glm::vec4& params) {
                const Mesh2DPipeId pipeId = getMesh2DPipeId(pipeEntity);

                Mesh2DId id;
                const auto it = entityToMeshId.find(entity);
                if (it == entityToMeshId.end()) {
                    id = meshBuffer.allocate();
                    if (id >= meshes.size()) meshes.resize(std::size_t{id} + 1);
                    entityToMeshId.emplace(entity, id);
                } else {
                    id = it->second;
                }
                meshes[id] = Mesh2D(color, uvRect, params, textureId, transform2DId, pipeId, meshInfoId);
                dirty.mark(id);
            }

            // EcsRequest::DestroyMesh2D
            void destroyMesh2D(const Mesh2DEntity entity) {
                const auto it = entityToMeshId.find(entity);
                if (it == entityToMeshId.end()) return;

                const Mesh2DId id = it->second;
                meshBuffer.free(id);
                meshes[id] = Mesh2D{};          // valid = 0, written to the GPU like any other dirty slot
                dirty.mark(id);
                entityToMeshId.erase(it);
            }

            void collectDirty(const uint32_t frameIndex) {
                dirty.collect([&](Mesh2DId id) { instanceBuffer.push_back(frameIndex, Mesh2DInstance{meshes[id], id}); });
            }

            void copy(const uint32_t frameIndex, const Command& cmd) {
                zeroOut(frameIndex, cmd);

                if ((instanceBufferSize = instanceBuffer.sizeCpu(frameIndex)) == 0) {
                    return;
                }
                instanceBuffer.copyToGpu(frameIndex, cmd);
            }

            void transfer(const uint32_t frameIndex, const Command& cmd) const {
                if (instanceBufferSize == 0) {
                    return;
                }

                vkCmdBindPipeline(cmd.vkCommandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, transferPipe.getVkPipeline());
                vkCmdPushConstants(cmd.vkCommandBuffer, transferDescrSet.getVkPipelineLayout(),
                    VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(uint32_t), &instanceBufferSize);
                vkCmdBindDescriptorSets(cmd.vkCommandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE,
                    transferDescrSet.getVkPipelineLayout(), 0, 1,
                    &transferDescrSet.getVkDescriptorSet(frameIndex), 0, nullptr);
                vkCmdDispatch(cmd.vkCommandBuffer, (instanceBufferSize + 256 - 1) / 256, 1, 1);
            }

            // mesh2d_cull.comp: one thread per Mesh2D slot; visible meshes append a draw command to the indirect buffer of their pipeline
            void cull(const uint32_t frameIndex, const Command& cmd) const {
                if (!hasMeshes()) {
                    return;
                }

                vkCmdBindPipeline(cmd.vkCommandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, cullPipe.getVkPipeline());
                vkCmdBindDescriptorSets(cmd.vkCommandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE,
                    cullDescrSet.getVkPipelineLayout(), 0, 1,
                    &cullDescrSet.getVkDescriptorSet(frameIndex), 0, nullptr);
                vkCmdDispatch(cmd.vkCommandBuffer, (meshBuffer.size() + 256 - 1) / 256, 1, 1);
            }

            // true if any Mesh2D slot is in use (the Renderer clears the depth image for the 2D pass only then)
            bool hasMeshes() const { return meshBuffer.size() > 0 && pipeArraySize > 0; }

            // Must be recorded inside dynamic rendering, after the depth image was cleared.
            void draw(const uint32_t frameIndex, const GeometryPackage<Vertex::Vertex2D>& geometry, const Command& command) const {
                if (!hasMeshes()) {
                    return;
                }

                const VkBuffer vertexBuffer = geometry.getVertexBuffer();
                const VkBuffer indexBuffer = geometry.getIndexBuffer();
                VkDeviceSize offsets[] = {0};

                vkCmdBindVertexBuffers(command.vkCommandBuffer, 0, 1, &vertexBuffer, offsets);
                vkCmdBindIndexBuffer(command.vkCommandBuffer, indexBuffer, 0, VK_INDEX_TYPE_UINT32);

                vkCmdBindDescriptorSets(command.vkCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                    meshDescrSet.getVkPipelineLayout(), 0, 1,
                    &meshDescrSet.getVkDescriptorSet(frameIndex), 0, nullptr);

                for (uint32_t pipeId = 0; pipeId < pipeArraySize; ++pipeId) {
                    vkCmdBindPipeline(command.vkCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                        meshPipes[pipeId].getVkPipeline());

                    vkCmdDrawIndexedIndirectCount(
                        command.vkCommandBuffer,
                        indirectBufferArray[pipeId]->getVkBuffer(frameIndex),
                        0,
                        drawCountBuffer.getVkBuffer(frameIndex),
                        static_cast<VkDeviceSize>(pipeId) * sizeof(uint32_t),
                        indirectDrawCapacity[pipeId],
                        sizeof(VkDrawIndexedIndirectCommand)
                    );
                }
            }

            void updateDescriptorSetWrites(){
                meshDescrSet.updateDescriptorSetWrites();
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

            static std::array<std::shared_ptr<IBufferResource>, MAX_MESH2D_PIPELINES> initIndirectBufferArray() {
                std::array<std::shared_ptr<IBufferResource>, MAX_MESH2D_PIPELINES> buffers;
                buffers.fill(initDummyIndirectBuffer());
                return buffers;
            }

            void zeroOut(const uint32_t frameIndex, const Command& cmd) {
                vkCmdFillBuffer(cmd.vkCommandBuffer, drawCountBuffer.getVkBuffer(frameIndex), 0, VK_WHOLE_SIZE, 0);
            }

            static void recordBarrier(VkCommandBuffer commandBuffer, VkPipelineStageFlags srcStage, VkPipelineStageFlags dstStage,
                                      VkAccessFlags srcAccess, VkAccessFlags dstAccess) {
                VkMemoryBarrier barrier{};
                barrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
                barrier.srcAccessMask = srcAccess;
                barrier.dstAccessMask = dstAccess;
                vkCmdPipelineBarrier(commandBuffer, srcStage, dstStage, 0, 1, &barrier, 0, nullptr, 0, nullptr);
            }
        };
    }
}
