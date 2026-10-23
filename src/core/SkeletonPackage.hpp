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
#include "instance/MaterialInstance.hpp"
#include "instance/SkeletonInstance.hpp"
#include "instance/SkeletonMeshInstance.hpp"
#include "CameraPackage.hpp"
#include "TransformPackage.hpp"
#include "GeometryPackage.hpp"
#include "LightPackage.hpp"
#include "TexturePackage.hpp"
#include "PipelineConfig.hpp"
#include "AnimationLibrary.hpp"
#include "TripleBuffers.hpp"
#include "DirtyTracker.hpp"
#include "PagedSparseMap.hpp"
#include <cstddef>
#include <optional>

namespace Lu{
    namespace Core{

        // ============================================================================
        //  SkeletonPackage = MeshPackage for skinned meshes + the skeleton instances
        //
        //  Data flow (ECS thread -> render thread)
        //   Create/Destroy requests  : Channel (mutex queue, rare)
        //   Poses of animated skeletons : TripleBuffer<SkeletonPoseSnapshot> (every frame, lock free)
        //
        //  Per frame on the render thread
        //   updatePoses()  poses of the newest snapshot are memcpy'd into the mapped staging
        //                  buffer of this frame in flight (+ rest poses of new skeletons)
        //   copy()         staging -> device-local copies (uploads, poses, mesh instances)
        //   transfer()     skeleton_pose.comp   : local TRS -> node matrices + skin matrices
        //                                         written into the SkeletonBuffer
        //                  skeleton_mesh_transfer.comp : mesh instances -> SkeletonMesh slots
        //   cull() / depth() / draw()   like MeshPackage, vertex shader reads the SkeletonBuffer
        //
        //  SkeletonBuffer layout per skeleton (SkeletonInstance, MATS_PER_SKELETON mat4):
        //     [0, MAX_BONES)             skin matrix  = node * inverseBind   -> deform
        //     [MAX_BONES, 2*MAX_BONES)   node matrix                         -> attach
        // ============================================================================
        class SkeletonPackage{
        private:
            using SkeletonEntity = flecs::entity_t;
            using SkeletonMeshEntity = flecs::entity_t;
            using PipeEntity = flecs::entity_t;
            using PipeId = uint32_t;

            static uint32_t key(const flecs::entity_t entity) { return static_cast<uint32_t>(entity); }

            // ---- skeleton meshes --------------------------------------------------------------
            DualBuffer<SkeletonMeshInstance> meshInstanceBuffer;
            SlotBuffer<SkeletonMesh> meshBuffer;
            BufferGpu<VkDrawIndexedIndirectCommand> indirectBufferDepth;
            BufferGpu<uint32_t> depthCountBuffer;
            std::array<std::shared_ptr<IBufferResource>, MAX_SKELETON_MESH_PIPELINES> indirectBufferArray;
            BufferGpu<uint32_t> drawCountBuffer;
            BufferGpuIndexed<uint32_t> capacityBuffer;

            // ---- skeleton instances -----------------------------------------------------------
            SlotBuffer<SkeletonInstance> skeletonBuffer;           // double buffered (per frame in flight)
            BufferGpuIndexed<SkeletonNodeGpu> nodeAssetBuffer;     // static: inverse bind + parent per asset node
            StagedBuffer<SkeletonUpload> uploadBuffer;             // headers of the skeletons updated this frame
            StagedBuffer<NodePose> poseBuffer;                     // their local poses (flat)

            DescriptorSet poseDescrSet;
            ComputePipeline posePipe;
            DescriptorSet transferDescrSet;
            ComputePipeline transferPipe;
            DescriptorSet cullDescrSet;
            ComputePipeline cullPipe;
            DescriptorSet depthDescrSet;
            GraphicsPipeline<Vertex::Skeleton> depthPipe;
            DescriptorSet drawDescrSet;
            uint32_t pipeArraySize{0};
            std::array<uint32_t, MAX_SKELETON_MESH_PIPELINES> indirectDrawCapacity{};
            std::array<GraphicsPipeline<Vertex::Skeleton>, MAX_SKELETON_MESH_PIPELINES> pipes{};
            std::unordered_map<PipeEntity, PipeId> entityToPipeId;

            PagedSparseMap<uint32_t, uint32_t> entityToMeshId;
            std::vector<SkeletonMesh> meshes;                      // CPU mirror, indexed by SkeletonMeshId
            std::vector<SkeletonMeshEntity> meshOwner;             // entity per mesh id (0 = free)
            DirtyTracker<SkeletonMeshId> meshDirty{MAX_FRAMES_IN_FLIGHT};

            struct PendingMesh {                                   // parent skeleton not created yet
                SkeletonMeshEntity entity;
                SkeletonEntity parent;
                PipeEntity pipe;
                TransformId transformId;
                GeometryInfoId infoId;
                Material material;
                uint32_t nodeIndex;
            };
            std::vector<PendingMesh> pendingMeshes;

            // ---- skeleton bookkeeping ----------------------------------------------------------
            PagedSparseMap<uint32_t, uint32_t> entityToSkeleton;   // entity -> slot
            std::vector<SkeletonEntity> slotOwner;                 // slot -> entity (0 = free)
            std::vector<SkeletonAssetId> slotAsset;
            std::vector<uint32_t> slotStamp;                       // frame stamp of the last upload
            uint32_t frameStamp{0};

            std::unordered_map<flecs::entity_t, SkeletonAssetId> entityToAsset;
            std::vector<uint32_t> assetNodeCount;
            std::vector<std::vector<NodePose>> assetRest;
            struct PendingAssetUpload { SkeletonAssetId id; std::vector<SkeletonNodeGpu> nodes; };
            std::vector<PendingAssetUpload> pendingAssetUploads;
            std::array<std::optional<BufferInterface<SkeletonNodeGpu>>, MAX_FRAMES_IN_FLIGHT> assetStaging;

            struct RestPending { uint32_t slot; uint32_t framesRemaining; };
            std::vector<RestPending> restPending;                  // new skeletons upload their rest pose
            uint32_t resendRemaining{0};

            Command command;
            uint32_t meshInstanceCount{0};
            uint32_t uploadCount{0};
            uint32_t poseUploadCount{0};

        public:
            SkeletonPackage(const CameraPackage& camera, const LightPackage& lights,
                const TransformPackage& transforms, const TexturePackage& textures,
                const GeometryPackage<Vertex::Skeleton>& geometry)
                : meshInstanceBuffer(MAX_ECS_REQUESTS_PROCESSED_PER_FRAME*MAX_FRAMES_IN_FLIGHT, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT),
                meshBuffer(MAX_SKELETON_MESHES, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT),
                indirectBufferDepth(MAX_SKELETON_MESHES, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT),
                depthCountBuffer(1, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT),
                indirectBufferArray(initIndirectBufferArray()),
                drawCountBuffer(MAX_SKELETON_MESH_PIPELINES, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT),
                capacityBuffer(MAX_SKELETON_MESH_PIPELINES, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT),
                skeletonBuffer(MAX_SKELETONS, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT),
                nodeAssetBuffer(MAX_SKELETON_ASSETS * MAX_BONES_PER_SKELETON, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT),
                uploadBuffer(MAX_SKELETONS, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT),
                poseBuffer(MAX_SKELETON_POSES_PER_FRAME, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT),
                command()
            {
                slotOwner.assign(MAX_SKELETONS, 0);
                slotAsset.assign(MAX_SKELETONS, 0);
                slotStamp.assign(MAX_SKELETONS, 0);

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

                createPosePipe();
                createTransferPipe();
                createCullPipe(camera, transforms, geometry);
                createDepthPipe(camera, transforms);
                createDrawDescriptorSet(camera, lights, transforms, textures);
            }

            // ------------------------------------------------------------------------------------
            //  pipelines
            // ------------------------------------------------------------------------------------
            void createPosePipe() {
                poseDescrSet
                .addStorageBuffer(0, uploadBuffer, VK_SHADER_STAGE_COMPUTE_BIT)
                .addStorageBuffer(1, poseBuffer, VK_SHADER_STAGE_COMPUTE_BIT)
                .addStorageBuffer(2, nodeAssetBuffer, VK_SHADER_STAGE_COMPUTE_BIT)
                .addStorageBuffer(3, skeletonBuffer, VK_SHADER_STAGE_COMPUTE_BIT)
                .addPushConstantRange(VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(uint32_t))
                .build();

                posePipe.create(
                    poseDescrSet.getVkPipelineLayout(),
                    ComputePipelineConfig{
                        .name = "skeleton_pose",
                        .computeShader = "shader/skeleton_pose.comp.spv"
                    }
                );
            }

            void createTransferPipe() {
                transferDescrSet
                .addStorageBuffer(0, meshInstanceBuffer, VK_SHADER_STAGE_COMPUTE_BIT)
                .addStorageBuffer(1, meshBuffer, VK_SHADER_STAGE_COMPUTE_BIT)
                .addPushConstantRange(VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(uint32_t))
                .build();

                transferPipe.create(
                    transferDescrSet.getVkPipelineLayout(),
                    ComputePipelineConfig{
                        .name = "skeleton_mesh_transfer",
                        .computeShader = "shader/skeleton_mesh_transfer.comp.spv"
                    }
                );
            }

            void createCullPipe(const CameraPackage& camera, const TransformPackage& transforms,
                                const GeometryPackage<Vertex::Skeleton>& geometry) {
                cullDescrSet
                .addUniformBuffer(0, camera.getCameraBuffer(), VK_SHADER_STAGE_COMPUTE_BIT)
                .addStorageBuffer(1, transforms.getTransformBuffer(), VK_SHADER_STAGE_COMPUTE_BIT)
                .addStorageBuffer(2, geometry.getGeometryInfoBuffer(), VK_SHADER_STAGE_COMPUTE_BIT)
                .addStorageBuffer(3, meshBuffer, VK_SHADER_STAGE_COMPUTE_BIT)
                .addStorageBuffer(4, drawCountBuffer, VK_SHADER_STAGE_COMPUTE_BIT)
                .addStorageBuffer(5, capacityBuffer, VK_SHADER_STAGE_COMPUTE_BIT)
                .addStorageBufferArray(6, indirectBufferArray, VK_SHADER_STAGE_COMPUTE_BIT)
                .addStorageBuffer(7, indirectBufferDepth, VK_SHADER_STAGE_COMPUTE_BIT)
                .addStorageBuffer(8, depthCountBuffer, VK_SHADER_STAGE_COMPUTE_BIT)
                .addStorageBuffer(9, skeletonBuffer, VK_SHADER_STAGE_COMPUTE_BIT)
                .build();

                cullPipe.create(
                    cullDescrSet.getVkPipelineLayout(),
                    ComputePipelineConfig{
                        .name = "skeleton_cull",
                        .computeShader = "shader/skeleton_cull.comp.spv"
                    }
                );
            }

            void createDepthPipe(const CameraPackage& camera, const TransformPackage& transforms) {
                depthDescrSet
                .addUniformBuffer(0, camera.getCameraBuffer(), VK_SHADER_STAGE_VERTEX_BIT)
                .addStorageBuffer(1, transforms.getTransformBuffer(), VK_SHADER_STAGE_VERTEX_BIT)
                .addStorageBuffer(2, meshBuffer, VK_SHADER_STAGE_VERTEX_BIT)
                .addStorageBuffer(3, skeletonBuffer, VK_SHADER_STAGE_VERTEX_BIT)
                .build();

                depthPipe.create(
                    depthDescrSet.getVkPipelineLayout(),
                    true/*depthVertexMode*/,
                    GraphicsPipelineConfig{
                        .name = "skeleton_depth",
                        .capacity = MAX_SKELETON_MESHES/*not used*/,
                        .vertexShader = "shader/skeleton_depth.vert.spv",
                        .depthBiasConstantFactor = 1.25f,
                        .depthBiasClamp = 0.0f,
                        .depthBiasSlopeFactor = 1.75f,
                        .sampleCount = VK_SAMPLE_COUNT_1_BIT,
                        .minSampleShading = 1.0f,
                        .depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL,
                        .minDepthBounds = 0.0f,
                        .maxDepthBounds = 1.0f,
                        .hasColorAttachments = false,
                        .depthAttachmentFormat = depthVkFormat
                    }
                );
            }

            void createDrawDescriptorSet(const CameraPackage& camera, const LightPackage& lights,
                                         const TransformPackage& transforms, const TexturePackage& textures){
                drawDescrSet
                .addUniformBuffer(0, camera.getCameraBuffer(), VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT)
                .addUniformBuffer(1, lights.getLightCountBuffer(), VK_SHADER_STAGE_FRAGMENT_BIT)
                .addStorageBuffer(2, lights.getLightBuffer(), VK_SHADER_STAGE_FRAGMENT_BIT)
                .addStorageBuffer(3, lights.getLightIndicesBuffer(), VK_SHADER_STAGE_FRAGMENT_BIT)
                .addStorageBuffer(4, transforms.getTransformBuffer(), VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT)
                .addStorageBuffer(5, meshBuffer, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT)
                .addTextureArray(6, textures.getTextureArray(), VK_SHADER_STAGE_FRAGMENT_BIT)
                .addStorageBuffer(7, skeletonBuffer, VK_SHADER_STAGE_VERTEX_BIT)
                .build();
            }

            void createSkeletonPipeline(const PipeEntity entity, const GraphicsPipelineConfig& config) {
                LU_ASSERT(pipeArraySize < MAX_SKELETON_MESH_PIPELINES, "SkeletonPackage", "createSkeletonPipeline",
                    "Skeleton pipeline array size overflow")
                const PipeId pipeId = pipeArraySize++;
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
                pipes[pipeId].create(drawDescrSet.getVkPipelineLayout(), false, config);
            }

            // ------------------------------------------------------------------------------------
            //  skeleton assets (static data: inverse bind matrices, hierarchy, rest pose)
            // ------------------------------------------------------------------------------------
            void createSkeletonAsset(const flecs::entity_t entity, const SkeletonAsset& asset) {
                LU_ASSERT(entityToAsset.size() < MAX_SKELETON_ASSETS, "SkeletonPackage", "createSkeletonAsset",
                    "Too many skeleton assets (MAX_SKELETON_ASSETS)")
                LU_ASSERT(!entityToAsset.contains(entity), "SkeletonPackage", "createSkeletonAsset",
                    "Skeleton asset entity already registered")
                LU_ASSERT(asset.nodeCount() > 0 && asset.nodeCount() <= MAX_BONES_PER_SKELETON,
                    "SkeletonPackage", "createSkeletonAsset",
                    "Skeleton '" + asset.name + "' has " + std::to_string(asset.nodeCount()) +
                    " nodes, MAX_BONES_PER_SKELETON is " + std::to_string(MAX_BONES_PER_SKELETON))

                const SkeletonAssetId id = static_cast<SkeletonAssetId>(entityToAsset.size());
                entityToAsset[entity] = id;
                assetNodeCount.push_back(asset.nodeCount());

                std::vector<NodePose> rest(asset.nodeCount());
                std::vector<SkeletonNodeGpu> gpu(asset.nodeCount());
                for(uint32_t i = 0; i < asset.nodeCount(); ++i) {
                    const SkeletonNode& node = asset.nodes[i];
                    rest[i] = node.rest;
                    gpu[i].inverseBind = node.inverseBind;
                    gpu[i].parent = node.parent;
                    gpu[i].isBone = node.isBone ? 1u : 0u;
                }
                assetRest.push_back(std::move(rest));
                pendingAssetUploads.push_back({id, std::move(gpu)});
            }

            SkeletonAssetId getSkeletonAssetId(const flecs::entity_t entity) const {
                const auto it = entityToAsset.find(entity);
                LU_ASSERT(it != entityToAsset.end(), "SkeletonPackage", "getSkeletonAssetId",
                    "No skeleton asset for given entity")
                return it->second;
            }

            // ------------------------------------------------------------------------------------
            //  skeleton instances (create = upsert)
            // ------------------------------------------------------------------------------------
            void createSkeleton(const SkeletonEntity entity, const flecs::entity_t assetEntity) {
                const SkeletonAssetId assetId = getSkeletonAssetId(assetEntity);

                uint32_t slot = entityToSkeleton.get(key(entity));
                if(slot == entityToSkeleton.kInvalid) {
                    slot = skeletonBuffer.allocate();
                    entityToSkeleton.set(key(entity), slot);
                }
                slotOwner[slot] = entity;
                slotAsset[slot] = assetId;
                restPending.push_back({slot, MAX_FRAMES_IN_FLIGHT});
            }

            void destroySkeleton(const SkeletonEntity entity) {
                const uint32_t slot = entityToSkeleton.get(key(entity));
                if(slot == entityToSkeleton.kInvalid) return;
                skeletonBuffer.free(slot);
                entityToSkeleton.erase(key(entity));
                slotOwner[slot] = 0;
            }

            uint32_t getSkeletonSlot(const SkeletonEntity entity) const {
                return entityToSkeleton.get(key(entity));
            }

            // ------------------------------------------------------------------------------------
            //  skeleton meshes (create = upsert)
            // ------------------------------------------------------------------------------------
            void createSkeletonMesh(const SkeletonMeshEntity entity, const SkeletonEntity parent,
                                    const PipeEntity pipe, const TransformId transformId,
                                    const GeometryInfoId infoId, const Material& material,
                                    const uint32_t nodeIndex) {
                LU_ASSERT(entityToPipeId.contains(pipe), "SkeletonPackage", "createSkeletonMesh",
                    "No PipelineId for given skeleton pipeline entity")

                const uint32_t slot = entityToSkeleton.get(key(parent));
                if(slot == entityToSkeleton.kInvalid) {
                    // the parent's CreateSkeleton request has not been processed yet: retry next frame
                    for(PendingMesh& pending : pendingMeshes) {
                        if(pending.entity == entity) {
                            pending = {entity, parent, pipe, transformId, infoId, material, nodeIndex};
                            return;
                        }
                    }
                    pendingMeshes.push_back({entity, parent, pipe, transformId, infoId, material, nodeIndex});
                    return;
                }

                SkeletonMeshId id = entityToMeshId.get(key(entity));
                if(id == entityToMeshId.kInvalid) {
                    id = meshBuffer.allocate();
                    if(id >= meshes.size()) {
                        meshes.resize(std::size_t{id} + 1);
                        meshOwner.resize(std::size_t{id} + 1, 0);
                    }
                    entityToMeshId.set(key(entity), id);
                }
                meshOwner[id] = entity;
                meshes[id] = SkeletonMesh(material, transformId, entityToPipeId.at(pipe), infoId, slot, nodeIndex);
                meshDirty.mark(id);
            }

            void destroySkeletonMesh(const SkeletonMeshEntity entity) {
                std::erase_if(pendingMeshes, [&](const PendingMesh& p){ return p.entity == entity; });

                const SkeletonMeshId id = entityToMeshId.get(key(entity));
                if(id == entityToMeshId.kInvalid) return;
                meshBuffer.free(id);
                entityToMeshId.erase(key(entity));
                meshOwner[id] = 0;
                meshes[id] = SkeletonMesh{};      // valid = 0, pushed to the GPU for all frames in flight
                meshDirty.mark(id);
            }

            // ------------------------------------------------------------------------------------
            //  per frame
            // ------------------------------------------------------------------------------------

            // Called on the render thread after the ECS requests were processed.
            // Fills the mapped staging buffers of `frameIndex`.
            void updatePoses(const uint32_t frameIndex) {
                ++frameStamp;
                uploadCount = 0;
                poseUploadCount = 0;

                SkeletonUpload* headers = uploadBuffer.map(frameIndex);
                NodePose* poses = poseBuffer.map(frameIndex);

                // A snapshot must reach EVERY frame-in-flight buffer, otherwise the buffers
                // of the other frames would show an older pose (flicker). So after a new
                // snapshot arrived it is uploaded again for MAX_FRAMES_IN_FLIGHT-1 frames
                // (the reader slot of the triple buffer stays valid until the next update()).
                auto& tripleBuffer = GetSkeletonPoseTripleBuffer();
                bool useSnapshot = false;
                if(tripleBuffer.update()) {
                    resendRemaining = MAX_FRAMES_IN_FLIGHT - 1;
                    useSnapshot = true;
                } else if(resendRemaining > 0) {
                    --resendRemaining;
                    useSnapshot = true;
                }

                if(useSnapshot) {
                    const SkeletonPoseSnapshot& snapshot = tripleBuffer.read_slot();
                    const uint32_t poseCount = std::min(snapshot.poseCount, MAX_SKELETON_POSES_PER_FRAME);
                    if(poseCount > 0) {
                        std::memcpy(poses, snapshot.poses.data(), sizeof(NodePose) * poseCount);
                    }
                    for(uint32_t i = 0; i < snapshot.headerCount && uploadCount < MAX_SKELETONS; ++i) {
                        const EcsRequest::UpdateSkeletonPose& header = snapshot.headers[i];
                        const uint32_t slot = entityToSkeleton.get(key(header.entity));
                        if(slot == entityToSkeleton.kInvalid || slotOwner[slot] != header.entity) continue; // destroyed / not created yet
                        const SkeletonAssetId assetId = slotAsset[slot];
                        if(header.nodeCount != assetNodeCount[assetId] ||
                           header.firstPose + header.nodeCount > poseCount) continue;                     // skeleton / graph mismatch
                        headers[uploadCount++] = SkeletonUpload{slot, assetId, header.nodeCount, header.firstPose};
                        slotStamp[slot] = frameStamp;
                    }
                    poseUploadCount = poseCount;
                }

                // rest pose of freshly created skeletons (until their first animated pose arrives)
                for(size_t i = 0; i < restPending.size();) {
                    RestPending& rest = restPending[i];
                    const bool dead = slotOwner[rest.slot] == 0;
                    const bool covered = slotStamp[rest.slot] == frameStamp;
                    if(dead || covered || rest.framesRemaining == 0) {
                        restPending[i] = restPending.back();
                        restPending.pop_back();
                        continue;
                    }
                    const SkeletonAssetId assetId = slotAsset[rest.slot];
                    const uint32_t n = assetNodeCount[assetId];
                    if(poseUploadCount + n <= MAX_SKELETON_POSES_PER_FRAME && uploadCount < MAX_SKELETONS) {
                        std::memcpy(poses + poseUploadCount, assetRest[assetId].data(), sizeof(NodePose) * n);
                        headers[uploadCount++] = SkeletonUpload{rest.slot, assetId, n, poseUploadCount};
                        poseUploadCount += n;
                        slotStamp[rest.slot] = frameStamp;
                        --rest.framesRemaining;
                    }
                    ++i;
                }
            }

            void collectDirty(const uint32_t frameIndex) {
                // meshes whose parent skeleton was not available yet
                if(!pendingMeshes.empty()) {
                    std::vector<PendingMesh> retry;
                    retry.swap(pendingMeshes);
                    for(const PendingMesh& p : retry) {
                        createSkeletonMesh(p.entity, p.parent, p.pipe, p.transformId, p.infoId, p.material, p.nodeIndex);
                    }
                }
                meshDirty.collect([&](SkeletonMeshId id) {
                    meshInstanceBuffer.push_back(frameIndex, SkeletonMeshInstance{meshes[id], id});
                });
            }

            void copy(const uint32_t frameIndex, const Command& cmd) {
                zeroOut(frameIndex, cmd);

                // static node table of newly registered skeleton assets
                assetStaging[frameIndex].reset();
                if(!pendingAssetUploads.empty()) {
                    uint32_t total = 0;
                    for(const auto& upload : pendingAssetUploads) total += static_cast<uint32_t>(upload.nodes.size());
                    assetStaging[frameIndex].emplace(total, VK_BUFFER_USAGE_TRANSFER_SRC_BIT);
                    BufferInterface<SkeletonNodeGpu>& staging = *assetStaging[frameIndex];
                    uint32_t offset = 0;
                    for(const auto& upload : pendingAssetUploads) {
                        const uint32_t count = static_cast<uint32_t>(upload.nodes.size());
                        std::memcpy(staging.pMemory + offset, upload.nodes.data(), sizeof(SkeletonNodeGpu) * count);
                        nodeAssetBuffer.recordCopyFrom(staging, offset, upload.id * MAX_BONES_PER_SKELETON, count, cmd);
                        offset += count;
                    }
                    pendingAssetUploads.clear();
                }

                uploadBuffer.copyToGpu(frameIndex, uploadCount, cmd);
                poseBuffer.copyToGpu(frameIndex, poseUploadCount, cmd);

                if ((meshInstanceCount = meshInstanceBuffer.sizeCpu(frameIndex)) == 0) {
                    return;
                }
                meshInstanceBuffer.copyToGpu(frameIndex, cmd);
            }

            void transfer(const uint32_t frameIndex, const Command& cmd) const {
                if(uploadCount > 0) {
                    vkCmdBindPipeline(cmd.vkCommandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, posePipe.getVkPipeline());
                    vkCmdPushConstants(cmd.vkCommandBuffer, poseDescrSet.getVkPipelineLayout(),
                        VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(uint32_t), &uploadCount);
                    vkCmdBindDescriptorSets(cmd.vkCommandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE,
                        poseDescrSet.getVkPipelineLayout(), 0, 1, &poseDescrSet.getVkDescriptorSet(frameIndex), 0, nullptr);
                    // one workgroup (MAX_BONES_PER_SKELETON threads) per skeleton
                    vkCmdDispatch(cmd.vkCommandBuffer, uploadCount, 1, 1);
                }

                if(meshInstanceCount > 0) {
                    vkCmdBindPipeline(cmd.vkCommandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, transferPipe.getVkPipeline());
                    vkCmdPushConstants(cmd.vkCommandBuffer, transferDescrSet.getVkPipelineLayout(),
                        VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(uint32_t), &meshInstanceCount);
                    vkCmdBindDescriptorSets(cmd.vkCommandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE,
                        transferDescrSet.getVkPipelineLayout(), 0, 1, &transferDescrSet.getVkDescriptorSet(frameIndex), 0, nullptr);
                    vkCmdDispatch(cmd.vkCommandBuffer, (meshInstanceCount + 256-1)/256, 1, 1);
                }
            }

            void cull(const uint32_t frameIndex, const Command& cmd) const {
                if(meshBuffer.size() == 0) return;
                vkCmdBindPipeline(cmd.vkCommandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, cullPipe.getVkPipeline());
                vkCmdBindDescriptorSets(cmd.vkCommandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE,
                    cullDescrSet.getVkPipelineLayout(), 0, 1, &cullDescrSet.getVkDescriptorSet(frameIndex), 0, nullptr);
                vkCmdDispatch(cmd.vkCommandBuffer, (meshBuffer.size() + 256-1)/256, 1, 1);
            }

            void depth(const uint32_t frameIndex, const GeometryPackage<Vertex::Skeleton>& geometry, const Command& cmd) const {
                if(meshBuffer.size() == 0) return;
                vkCmdBindPipeline(cmd.vkCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, depthPipe.getVkPipeline());
                vkCmdBindDescriptorSets(cmd.vkCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                    depthDescrSet.getVkPipelineLayout(), 0, 1, &depthDescrSet.getVkDescriptorSet(frameIndex), 0, nullptr);

                const VkBuffer vertexBuffer = geometry.getVertexBuffer();
                const VkBuffer indexBuffer = geometry.getIndexBuffer();
                VkDeviceSize offsets[] = {0};
                vkCmdBindVertexBuffers(cmd.vkCommandBuffer, 0, 1, &vertexBuffer, offsets);
                vkCmdBindIndexBuffer(cmd.vkCommandBuffer, indexBuffer, 0, VK_INDEX_TYPE_UINT32);

                vkCmdDrawIndexedIndirectCount(
                    cmd.vkCommandBuffer,
                    indirectBufferDepth.getVkBuffer(frameIndex), 0,
                    depthCountBuffer.getVkBuffer(frameIndex), 0,
                    meshBuffer.size(), sizeof(VkDrawIndexedIndirectCommand));
            }

            void draw(const uint32_t frameIndex, const GeometryPackage<Vertex::Skeleton>& geometry, const Command& cmd) const {
                if(meshBuffer.size() == 0) return;
                const VkBuffer vertexBuffer = geometry.getVertexBuffer();
                const VkBuffer indexBuffer = geometry.getIndexBuffer();
                VkDeviceSize offsets[] = {0};
                vkCmdBindVertexBuffers(cmd.vkCommandBuffer, 0, 1, &vertexBuffer, offsets);
                vkCmdBindIndexBuffer(cmd.vkCommandBuffer, indexBuffer, 0, VK_INDEX_TYPE_UINT32);
                vkCmdBindDescriptorSets(cmd.vkCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                    drawDescrSet.getVkPipelineLayout(), 0, 1, &drawDescrSet.getVkDescriptorSet(frameIndex), 0, nullptr);

                for (uint32_t pipeId = 0; pipeId < pipeArraySize; ++pipeId) {
                    vkCmdBindPipeline(cmd.vkCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipes[pipeId].getVkPipeline());
                    vkCmdDrawIndexedIndirectCount(
                        cmd.vkCommandBuffer,
                        indirectBufferArray[pipeId]->getVkBuffer(frameIndex), 0,
                        drawCountBuffer.getVkBuffer(frameIndex), static_cast<VkDeviceSize>(pipeId) * sizeof(uint32_t),
                        indirectDrawCapacity[pipeId], sizeof(VkDrawIndexedIndirectCommand));
                }
            }

            void updateDescriptorSetWrites(){
                drawDescrSet.updateDescriptorSetWrites();
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

            static std::array<std::shared_ptr<IBufferResource>, MAX_SKELETON_MESH_PIPELINES> initIndirectBufferArray() {
                std::array<std::shared_ptr<IBufferResource>, MAX_SKELETON_MESH_PIPELINES> buffers;
                buffers.fill(initDummyIndirectBuffer());
                return buffers;
            }

            void zeroOut(const uint32_t frameIndex, const Command& cmd){
                vkCmdFillBuffer(cmd.vkCommandBuffer, drawCountBuffer.getVkBuffer(frameIndex), 0, VK_WHOLE_SIZE, 0);
                vkCmdFillBuffer(cmd.vkCommandBuffer, depthCountBuffer.getVkBuffer(frameIndex), 0, VK_WHOLE_SIZE, 0);
            }

            static void recordBarrier(
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
                vkCmdPipelineBarrier(commandBuffer, srcStage, dstStage, 0, 1, &barrier, 0, nullptr, 0, nullptr);
            }
        };
    }
}
