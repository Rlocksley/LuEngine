#pragma once

#include "Global.hpp"
#include "Buffer.hpp"
#include "CameraPackage.hpp"
#include "Command.hpp"
#include "ComputePipeline.hpp"
#include "DescriptorSet.hpp"
#include "GeometryPackage.hpp"
#include "GraphicsPipeline.hpp"
#include "PipelineConfig.hpp"
#include "Time.hpp"
#include "TransformPackage.hpp"
#include "instance/MaterialInstance.hpp"
#include "instance/TransformInstance.hpp"
#include "component/MultiMesh.hpp"

namespace Lu{
    namespace Core{
        struct MultiMeshTransform{
            Material material{};
            Transform transform{};

            MultiMeshTransform() = default;
            explicit MultiMeshTransform(const Component::MultiMeshInstance& instance)
                : material(instance.material), transform(instance.transform) {}
        };

            struct IndirectDispatch{
                uint32_t multiMeshId{};
                uint32_t transformBufferLength{};
                float deltaTime{};
                VkDispatchIndirectCommand dispatch{};
            };

        static_assert(sizeof(MultiMeshTransform) == 256);
            static_assert(offsetof(IndirectDispatch, deltaTime) == 8);
            static_assert(offsetof(IndirectDispatch, dispatch) == 12);
            static_assert(sizeof(IndirectDispatch) == 24);
        static_assert(sizeof(MultiMesh) == 64);
        static_assert(sizeof(MultiMeshInstance) == 80);

        class MultiMeshPackage{
        private:
            using Entity = flecs::entity_t;
            using Id = uint32_t;
            static constexpr uint32_t AllFramesMask = (1u << MAX_FRAMES_IN_FLIGHT) - 1u;

            struct MultiMeshData{
                Id id{};
                Id computePipeId{};
                Id pipelineId{};
                Id meshInfoId{};
                TransformId parentTransformId{};
                glm::vec4 cullSphere{};
                uint32_t transformCount{};
                bool uploadTransformsNeeded{true};
                std::vector<MultiMeshTransform> transforms;
            };

            struct RetiredTransformBuffer{
                Id id{};
                std::shared_ptr<BufferGpuAddress<MultiMeshTransform>> buffer;
                uint32_t framesRemaining{AllFramesMask};
            };

            static constexpr uint32_t UploadTransformCapacity =
                MAX_MULTI_MESH_TRANSFORMS_PER_UPLOAD * MAX_MULTI_MESH_UPLOADS_PER_FRAME;
            static constexpr uint32_t UploadByteCapacity =
                MAX_INSTANCED_MESHES * sizeof(MultiMeshInstance) +
                UploadTransformCapacity * sizeof(MultiMeshTransform);

            std::array<BufferInterface<uint8_t>, MAX_FRAMES_IN_FLIGHT> uploadInterface;
            BufferGpu<MultiMeshInstance> instanceBuffer;
            BufferGpu<MultiMeshTransform> transformUploadBuffer;
            BufferGpuAddress<MultiMesh> meshBuffer;
            std::array<std::shared_ptr<BufferGpuAddress<MultiMeshTransform>>, MAX_INSTANCED_MESHES> transformBufferArray{};
            std::array<std::shared_ptr<BufferGpuAddress<IndirectDispatch>>, MAX_INSTANCED_MESH_COMPUTE_PIPELINES> indirectDispatchBufferArray{};
            std::array<std::shared_ptr<BufferGpuAddress<VkDrawIndexedIndirectCommand>>, MAX_INSTANCED_MESH_PIPELINES> indirectBufferArray{};
            std::array<std::shared_ptr<IBufferResource>, MAX_INSTANCED_MESH_COMPUTE_PIPELINES> indirectDispatchDescriptorArray{};
            std::array<std::shared_ptr<IBufferResource>, MAX_INSTANCED_MESH_PIPELINES> indirectDescriptorArray{};
            std::array<std::unique_ptr<BufferGpuAddress<uint8_t>>, MAX_INSTANCED_MESH_COMPUTE_PIPELINES> preprocessBufferArray{};
            std::array<VkDeviceSize, MAX_INSTANCED_MESH_COMPUTE_PIPELINES> preprocessBufferSizes{};

            DescriptorSet transferDescrSet;
            ComputePipeline transferPipe;
            DescriptorSet cullDescrSet;
            ComputePipeline cullPipe;
            DescriptorSet computeDescrSet;
            DescriptorSet meshDescrSet;
            uint32_t compPipeArraySize{0};
            std::array<ComputePipeline, MAX_INSTANCED_MESH_COMPUTE_PIPELINES> compPipes{};
            std::array<VkIndirectCommandsLayoutEXT, MAX_INSTANCED_MESH_COMPUTE_PIPELINES> indirectCommandsLayouts{};
            uint32_t meshPipeArraySize{0};
            std::array<GraphicsPipeline<Vertex::Mesh>, MAX_INSTANCED_MESH_PIPELINES> meshPipes{};

            std::unordered_map<Entity, Id> entityToMultiMeshId;
            std::unordered_map<Id, MultiMeshData> multiMeshes;
            std::unordered_map<Entity, Id> entityToComputePipeId;
            std::unordered_map<Entity, Id> entityToMeshPipeId;
            std::vector<Id> freeIds;
            std::unordered_map<Id, uint32_t> dirtyIds;
            std::vector<RetiredTransformBuffer> retiredTransformBuffers;
            std::array<std::vector<MultiMeshInstance>, MAX_FRAMES_IN_FLIGHT> uploadInstances{};
            std::array<std::vector<MultiMeshTransform>, MAX_FRAMES_IN_FLIGHT> uploadTransforms{};
            std::array<uint32_t, MAX_FRAMES_IN_FLIGHT> uploadInstanceCounts{};
            std::array<uint32_t, MAX_FRAMES_IN_FLIGHT> uploadTransformCounts{};
            std::array<uint32_t, MAX_FRAMES_IN_FLIGHT> uploadMaxTransformCounts{};
            Id nextId{0};
            PFN_vkCreateIndirectCommandsLayoutEXT createIndirectCommandsLayoutEXT{};
            PFN_vkDestroyIndirectCommandsLayoutEXT destroyIndirectCommandsLayoutEXT{};
            PFN_vkGetGeneratedCommandsMemoryRequirementsEXT getGeneratedCommandsMemoryRequirementsEXT{};
            PFN_vkCmdExecuteGeneratedCommandsEXT cmdExecuteGeneratedCommandsEXT{};

                        template<size_t... Frames>
                        static std::array<BufferInterface<uint8_t>, MAX_FRAMES_IN_FLIGHT> makeUploadInterfaces(
                                std::index_sequence<Frames...>) {
                                return {{(static_cast<void>(Frames), BufferInterface<uint8_t>(UploadByteCapacity,
                                        VK_BUFFER_USAGE_TRANSFER_SRC_BIT))...}};
                        }

                        static std::array<BufferInterface<uint8_t>, MAX_FRAMES_IN_FLIGHT> makeUploadInterfaces(){
                                return makeUploadInterfaces(std::make_index_sequence<MAX_FRAMES_IN_FLIGHT>{});
                        }

        public:
            MultiMeshPackage(const CameraPackage& camera, const TransformPackage& transforms,
                             const GeometryPackage<Vertex::Mesh>& geometry)
                                : uploadInterface(makeUploadInterfaces()),
                                    instanceBuffer(MAX_MULTI_MESH_UPLOADS_PER_FRAME,
                                        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT),
                                    transformUploadBuffer(UploadTransformCapacity,
                                        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT),
                                    meshBuffer(MAX_INSTANCED_MESHES,
                                        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                                        nullptr, false)
            {
                createIndirectCommandsLayoutEXT = reinterpret_cast<PFN_vkCreateIndirectCommandsLayoutEXT>(
                    vkGetDeviceProcAddr(vkDevice, "vkCreateIndirectCommandsLayoutEXT"));
                destroyIndirectCommandsLayoutEXT = reinterpret_cast<PFN_vkDestroyIndirectCommandsLayoutEXT>(
                    vkGetDeviceProcAddr(vkDevice, "vkDestroyIndirectCommandsLayoutEXT"));
                getGeneratedCommandsMemoryRequirementsEXT = reinterpret_cast<PFN_vkGetGeneratedCommandsMemoryRequirementsEXT>(
                    vkGetDeviceProcAddr(vkDevice, "vkGetGeneratedCommandsMemoryRequirementsEXT"));
                cmdExecuteGeneratedCommandsEXT = reinterpret_cast<PFN_vkCmdExecuteGeneratedCommandsEXT>(
                    vkGetDeviceProcAddr(vkDevice, "vkCmdExecuteGeneratedCommandsEXT"));
                LU_ASSERT(createIndirectCommandsLayoutEXT && destroyIndirectCommandsLayoutEXT &&
                    getGeneratedCommandsMemoryRequirementsEXT && cmdExecuteGeneratedCommandsEXT,
                    "MultiMeshPackage", "MultiMeshPackage", "DGC extension procedures are unavailable.")

                initializeBuffer(meshBuffer);

                auto dummyDispatch = std::make_shared<BufferGpuAddress<IndirectDispatch>>(
                    MAX_INSTANCED_MESHES, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT);
                auto dummyDraw = std::make_shared<BufferGpuAddress<VkDrawIndexedIndirectCommand>>(
                    MAX_INSTANCED_MESHES, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT);
                indirectDispatchDescriptorArray.fill(dummyDispatch);
                indirectDescriptorArray.fill(dummyDraw);

                computeDescrSet
                    .addStorageBuffer(0, meshBuffer, VK_SHADER_STAGE_COMPUTE_BIT)
                    .addPushConstantRange(VK_SHADER_STAGE_COMPUTE_BIT, 0, 2 * sizeof(uint32_t) + sizeof(float))
                    .build();

                transferDescrSet
                    .addStorageBuffer(0, instanceBuffer, VK_SHADER_STAGE_COMPUTE_BIT)
                    .addStorageBuffer(1, transformUploadBuffer, VK_SHADER_STAGE_COMPUTE_BIT)
                    .addStorageBuffer(2, meshBuffer, VK_SHADER_STAGE_COMPUTE_BIT)
                    .addStorageBufferArray(3, indirectDispatchDescriptorArray, VK_SHADER_STAGE_COMPUTE_BIT)
                    .addStorageBufferArray(4, indirectDescriptorArray, VK_SHADER_STAGE_COMPUTE_BIT)
                    .addStorageBuffer(5, geometry.getGeometryInfoBuffer(), VK_SHADER_STAGE_COMPUTE_BIT)
                    .addPushConstantRange(VK_SHADER_STAGE_COMPUTE_BIT, 0, 2 * sizeof(uint32_t) + sizeof(float))
                    .build();

                transferPipe.create(transferDescrSet.getVkPipelineLayout(), ComputePipelineConfig{
                    .name = "multimesh_transfer",
                    .computeShader = "shader/multi_mesh_transfer.comp.spv"
                });

                meshDescrSet
                    .addUniformBuffer(0, camera.getCameraBuffer(), VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT)
                    .addStorageBuffer(1, transforms.getTransformBuffer(), VK_SHADER_STAGE_VERTEX_BIT)
                    .addStorageBuffer(2, meshBuffer, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT)
                    .build();

                cullDescrSet
                    .addUniformBuffer(0, camera.getCameraBuffer(), VK_SHADER_STAGE_COMPUTE_BIT)
                    .addStorageBuffer(1, transforms.getTransformBuffer(), VK_SHADER_STAGE_COMPUTE_BIT)
                    .addStorageBuffer(2, meshBuffer, VK_SHADER_STAGE_COMPUTE_BIT)
                    .addStorageBufferArray(3, indirectDispatchDescriptorArray, VK_SHADER_STAGE_COMPUTE_BIT)
                    .addStorageBufferArray(4, indirectDescriptorArray, VK_SHADER_STAGE_COMPUTE_BIT)
                    .addPushConstantRange(VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(uint32_t) + sizeof(float))
                    .build();

                cullPipe.create(cullDescrSet.getVkPipelineLayout(), ComputePipelineConfig{
                    .name = "multimesh_cull",
                    .computeShader = "shader/multi_mesh_cull.comp.spv"
                });

            }

            ~MultiMeshPackage(){
                for(VkIndirectCommandsLayoutEXT layout : indirectCommandsLayouts){
                    if(layout != VK_NULL_HANDLE){
                        destroyIndirectCommandsLayoutEXT(vkDevice, layout, nullptr);
                    }
                }
            }

            void createComputePipeline(Entity entity, const ComputePipelineConfig& config){
                LU_ASSERT(compPipeArraySize < MAX_INSTANCED_MESH_COMPUTE_PIPELINES,
                    "MultiMeshPackage", "createComputePipeline", "Compute pipeline array capacity exceeded.")
                LU_ASSERT(entityToComputePipeId.find(entity) == entityToComputePipeId.end(),
                    "MultiMeshPackage", "createComputePipeline", "Compute pipeline entity is already registered.")

                const Id pipeId = compPipeArraySize++;
                createIndirectCommandsLayout(pipeId);
                compPipes[pipeId].create(computeDescrSet.getVkPipelineLayout(), config, true);
                entityToComputePipeId.emplace(entity, pipeId);
                indirectDispatchBufferArray[pipeId] = std::make_shared<BufferGpuAddress<IndirectDispatch>>(
                    MAX_INSTANCED_MESHES,
                    VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT |
                        VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                    nullptr,
                    false
                );
                initializeBuffer(*indirectDispatchBufferArray[pipeId]);
                indirectDispatchDescriptorArray[pipeId] = indirectDispatchBufferArray[pipeId];
                transferDescrSet.updateDescriptorSetWrites();
                cullDescrSet.updateDescriptorSetWrites();
                createPreprocessBuffer(pipeId);

                for(const auto& [id, data] : multiMeshes){
                    dirtyIds[id] = AllFramesMask;
                }
            }

            void createGraphicsPipeline(Entity entity, const GraphicsPipelineConfig& config){
                LU_ASSERT(meshPipeArraySize < MAX_INSTANCED_MESH_PIPELINES,
                    "MultiMeshPackage", "createGraphicsPipeline", "Graphics pipeline array capacity exceeded.")
                LU_ASSERT(entityToMeshPipeId.find(entity) == entityToMeshPipeId.end(),
                    "MultiMeshPackage", "createGraphicsPipeline", "Graphics pipeline entity is already registered.")

                const Id pipeId = meshPipeArraySize++;
                indirectBufferArray[pipeId] = std::make_shared<BufferGpuAddress<VkDrawIndexedIndirectCommand>>(
                    MAX_INSTANCED_MESHES,
                    VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT |
                        VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                    nullptr,
                    false
                );
                initializeBuffer(*indirectBufferArray[pipeId]);
                indirectDescriptorArray[pipeId] = indirectBufferArray[pipeId];
                transferDescrSet.updateDescriptorSetWrites();
                cullDescrSet.updateDescriptorSetWrites();

                entityToMeshPipeId.emplace(entity, pipeId);
                meshPipes[pipeId].create(meshDescrSet.getVkPipelineLayout(), false, config);

                for(const auto& [id, data] : multiMeshes){
                    dirtyIds[id] = AllFramesMask;
                }
            }

            void createMultiMesh(Entity entity, Entity geometryEntity, Entity computePipeEntity,
                                 Entity graphicsPipeEntity, TransformId parentTransformId,
                                 const GeometryPackage<Vertex::Mesh>& geometry,
                                 const std::vector<Component::MultiMeshInstance>& instances,
                                 const glm::vec4& cullSphere){
                const auto computeIt = entityToComputePipeId.find(computePipeEntity);
                LU_ASSERT(computeIt != entityToComputePipeId.end(), "MultiMeshPackage", "createMultiMesh", "No compute pipeline registered for entity.")
                const auto graphicsIt = entityToMeshPipeId.find(graphicsPipeEntity);
                LU_ASSERT(graphicsIt != entityToMeshPipeId.end(), "MultiMeshPackage", "createMultiMesh", "No graphics pipeline registered for entity.")

                Id id;
                const auto entityIt = entityToMultiMeshId.find(entity);
                if(entityIt == entityToMultiMeshId.end()){
                    if(freeIds.empty()){
                        LU_ASSERT(nextId < MAX_INSTANCED_MESHES,
                            "MultiMeshPackage", "createMultiMesh", "MultiMesh slot capacity exceeded.")
                        id = nextId++;
                    }else{
                        id = freeIds.back();
                        freeIds.pop_back();
                    }
                    entityToMultiMeshId.emplace(entity, id);
                }else{
                    id = entityIt->second;
                    auto oldIt = multiMeshes.find(id);
                    if(oldIt != multiMeshes.end()){
                        retiredTransformBuffers.push_back({id, std::move(transformBufferArray[id]), AllFramesMask});
                    }
                }

                LU_ASSERT(instances.size() <= MAX_MULTI_MESH_TRANSFORMS_PER_UPLOAD,
                    "MultiMeshPackage", "createMultiMesh", "A multimesh exceeds the per-upload transform limit.")

                MultiMeshData data{};
                data.id = id;
                data.computePipeId = computeIt->second;
                data.pipelineId = graphicsIt->second;
                data.meshInfoId = geometry.getGeometryInfoId(geometryEntity);
                data.parentTransformId = parentTransformId;
                data.cullSphere = cullSphere;
                data.transformCount = static_cast<uint32_t>(instances.size());
                data.transforms.reserve(instances.size());
                for(const auto& instance : instances){
                    data.transforms.emplace_back(instance);
                }
                transformBufferArray[id] = std::make_shared<BufferGpuAddress<MultiMeshTransform>>(
                    std::max(1u, static_cast<uint32_t>(data.transforms.size())),
                    VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                    nullptr, false);

                multiMeshes[id] = std::move(data);
                dirtyIds[id] = AllFramesMask;
            }

            void destroyMultiMesh(Entity entity){
                const auto entityIt = entityToMultiMeshId.find(entity);
                LU_ASSERT(entityIt != entityToMultiMeshId.end(),
                    "MultiMeshPackage", "destroyMultiMesh", "MultiMesh entity does not exist.")

                const Id id = entityIt->second;
                auto dataIt = multiMeshes.find(id);
                if(dataIt != multiMeshes.end()){
                    retiredTransformBuffers.push_back({id, std::move(transformBufferArray[id]), AllFramesMask});
                    multiMeshes.erase(dataIt);
                }
                entityToMultiMeshId.erase(entityIt);
                freeIds.push_back(id);
                dirtyIds[id] = AllFramesMask;
            }

            void collectDirty(uint32_t frameIndex){
                const uint32_t frameMask = 1u << frameIndex;
                uint32_t uploads = 0;
                auto dirtyIt = dirtyIds.begin();
                while(dirtyIt != dirtyIds.end() && uploads < MAX_MULTI_MESH_UPLOADS_PER_FRAME){
                    if((dirtyIt->second & frameMask) == 0){
                        ++dirtyIt;
                        continue;
                    }

                    const Id id = dirtyIt->first;
                    auto dataIt = multiMeshes.find(id);
                    if(dataIt == multiMeshes.end()){
                        uploadInstances[frameIndex].push_back(MultiMeshInstance{MultiMesh{}, id, 0, 0, 0});
                    }else{
                        MultiMeshData& data = dataIt->second;
                        if(data.uploadTransformsNeeded &&
                            uploadTransforms[frameIndex].size() + data.transforms.size() > UploadTransformCapacity){
                            ++dirtyIt;
                            continue;
                        }

                        MultiMesh gpuMesh{};
                        gpuMesh.transformId = data.parentTransformId;
                        gpuMesh.computePipeId = data.computePipeId;
                        gpuMesh.pipelineId = data.pipelineId;
                        gpuMesh.meshInfoId = data.meshInfoId;
                        gpuMesh.size = data.transformCount;
                        gpuMesh.valid = 1;
                        gpuMesh.transformBufferAddress = transformBufferArray[id]->getDeviceAddress(frameIndex);
                        gpuMesh.cullSphere = data.cullSphere;
                        uint32_t rangeBegin = 0;
                        uint32_t rangeEnd = 0;
                        if(data.uploadTransformsNeeded){
                            rangeBegin = static_cast<uint32_t>(uploadTransforms[frameIndex].size());
                            uploadTransforms[frameIndex].insert(uploadTransforms[frameIndex].end(),
                                data.transforms.begin(), data.transforms.end());
                            rangeEnd = static_cast<uint32_t>(uploadTransforms[frameIndex].size());
                            uploadMaxTransformCounts[frameIndex] = std::max(
                                uploadMaxTransformCounts[frameIndex], rangeEnd - rangeBegin);
                        }
                        uploadInstances[frameIndex].push_back(
                            MultiMeshInstance{gpuMesh, id, rangeBegin, rangeEnd, 0});
                    }

                    dirtyIt->second &= ~frameMask;
                    const bool completed = dirtyIt->second == 0;
                    if(dataIt != multiMeshes.end() && completed && dataIt->second.uploadTransformsNeeded){
                        dataIt->second.uploadTransformsNeeded = false;
                        std::vector<MultiMeshTransform>().swap(dataIt->second.transforms);
                    }

                    std::erase_if(retiredTransformBuffers, [&](RetiredTransformBuffer& retired){
                        if(retired.id == id){
                            retired.framesRemaining &= ~frameMask;
                        }
                        return retired.framesRemaining == 0;
                    });

                    ++uploads;
                    if(completed){
                        dirtyIt = dirtyIds.erase(dirtyIt);
                    }else{
                        ++dirtyIt;
                    }
                }
            }

            void copy(uint32_t frameIndex, const Command& command){
                auto& instances = uploadInstances[frameIndex];
                auto& transforms = uploadTransforms[frameIndex];
                uploadInstanceCounts[frameIndex] = static_cast<uint32_t>(instances.size());
                uploadTransformCounts[frameIndex] = static_cast<uint32_t>(transforms.size());
                if(instances.empty()){
                    return;
                }

                LU_ASSERT(transforms.size() <= UploadTransformCapacity,
                    "MultiMeshPackage", "copy", "Transform upload batch exceeds its capacity.")
                const VkDeviceSize instanceBytes = sizeof(MultiMeshInstance) * instances.size();
                const VkDeviceSize transformBytes = sizeof(MultiMeshTransform) * transforms.size();
                const VkDeviceSize transformSourceOffset = instanceBytes;
                auto* mapped = uploadInterface[frameIndex].pMemory;
                std::memcpy(mapped, instances.data(), static_cast<size_t>(instanceBytes));
                if(transformBytes > 0){
                    std::memcpy(reinterpret_cast<uint8_t*>(mapped) + transformSourceOffset,
                        transforms.data(), static_cast<size_t>(transformBytes));
                }
                LU_CHECK_VULKAN(vmaFlushAllocation(vmaAllocator,
                    uploadInterface[frameIndex].vmaAllocation, 0, instanceBytes + transformBytes),
                    "MultiMeshPackage", "vmaFlushAllocation")

                VkBufferCopy instanceCopy{};
                instanceCopy.size = instanceBytes;
                vkCmdCopyBuffer(command.vkCommandBuffer, uploadInterface[frameIndex].vkBuffer,
                    instanceBuffer.getVkBuffer(frameIndex), 1, &instanceCopy);

                if(transformBytes > 0){
                    VkBufferCopy transformCopy{};
                    transformCopy.srcOffset = transformSourceOffset;
                    transformCopy.size = transformBytes;
                    vkCmdCopyBuffer(command.vkCommandBuffer, uploadInterface[frameIndex].vkBuffer,
                        transformUploadBuffer.getVkBuffer(frameIndex), 1, &transformCopy);
                }

                instances.clear();
                transforms.clear();
            }

            void transfer(uint32_t frameIndex, const Command& command){
                const uint32_t instanceCount = uploadInstanceCounts[frameIndex];
                if(instanceCount == 0){
                    return;
                }
                vkCmdBindPipeline(command.vkCommandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE,
                    transferPipe.getVkPipeline());
                vkCmdBindDescriptorSets(command.vkCommandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE,
                    transferDescrSet.getVkPipelineLayout(), 0, 1,
                    &transferDescrSet.getVkDescriptorSet(frameIndex), 0, nullptr);
                const std::array<uint32_t, 2> pushConstants{
                    instanceCount,
                    uploadTransformCounts[frameIndex]
                };
                struct TransferPushConstants{
                    uint32_t instanceCount;
                    uint32_t transformCount;
                    float deltaTime;
                } transferConstants{pushConstants[0], pushConstants[1], Time::deltaTime};
                vkCmdPushConstants(command.vkCommandBuffer, transferDescrSet.getVkPipelineLayout(),
                    VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(transferConstants), &transferConstants);
                const uint32_t transformGroups = std::max(
                    1u, (uploadMaxTransformCounts[frameIndex] + 255u) / 256u);
                vkCmdDispatch(command.vkCommandBuffer, transformGroups, instanceCount, 1);
                uploadMaxTransformCounts[frameIndex] = 0;
            }

            void cull(uint32_t frameIndex, const Command& command) const{
                vkCmdBindPipeline(command.vkCommandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE,
                    cullPipe.getVkPipeline());
                vkCmdBindDescriptorSets(command.vkCommandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE,
                    cullDescrSet.getVkPipelineLayout(), 0, 1,
                    &cullDescrSet.getVkDescriptorSet(frameIndex), 0, nullptr);
                struct CullPushConstants{
                    uint32_t multiMeshCount;
                    float deltaTime;
                } cullConstants{nextId, Time::deltaTime};
                vkCmdPushConstants(command.vkCommandBuffer, cullDescrSet.getVkPipelineLayout(),
                    VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(cullConstants), &cullConstants);
                vkCmdDispatch(command.vkCommandBuffer, (nextId + 255) / 256, 1, 1);
            }

            void executeCompute(uint32_t frameIndex, const Command& command) const{
                for(Id pipeId = 0; pipeId < compPipeArraySize; ++pipeId){
                    vkCmdBindPipeline(command.vkCommandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE,
                        compPipes[pipeId].getVkPipeline());
                    vkCmdBindDescriptorSets(command.vkCommandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE,
                        computeDescrSet.getVkPipelineLayout(), 0, 1,
                        &computeDescrSet.getVkDescriptorSet(frameIndex), 0, nullptr);

                    VkGeneratedCommandsPipelineInfoEXT pipelineInfo{};
                    pipelineInfo.sType = VK_STRUCTURE_TYPE_GENERATED_COMMANDS_PIPELINE_INFO_EXT;
                    pipelineInfo.pipeline = compPipes[pipeId].getVkPipeline();

                    VkGeneratedCommandsInfoEXT generatedInfo{};
                    generatedInfo.sType = VK_STRUCTURE_TYPE_GENERATED_COMMANDS_INFO_EXT;
                    generatedInfo.pNext = &pipelineInfo;
                    generatedInfo.shaderStages = VK_SHADER_STAGE_COMPUTE_BIT;
                    generatedInfo.indirectCommandsLayout = indirectCommandsLayouts[pipeId];
                    generatedInfo.indirectAddress = indirectDispatchBufferArray[pipeId]->getDeviceAddress(frameIndex);
                    generatedInfo.indirectAddressSize = sizeof(IndirectDispatch) * nextId;
                    generatedInfo.preprocessAddress = preprocessBufferArray[pipeId]->getDeviceAddress(frameIndex);
                    generatedInfo.preprocessSize = preprocessBufferSizes[pipeId];
                    generatedInfo.maxSequenceCount = nextId;
                    if(generatedInfo.maxSequenceCount == 0){
                        continue;
                    }
                    cmdExecuteGeneratedCommandsEXT(command.vkCommandBuffer, VK_FALSE, &generatedInfo);
                }
            }

            void draw(uint32_t frameIndex, const GeometryPackage<Vertex::Mesh>& geometry,
                      const Command& command) const{
                const VkBuffer vertexBuffer = geometry.getVertexBuffer();
                const VkBuffer indexBuffer = geometry.getIndexBuffer();
                const VkDeviceSize offset = 0;
                vkCmdBindVertexBuffers(command.vkCommandBuffer, 0, 1, &vertexBuffer, &offset);
                vkCmdBindIndexBuffer(command.vkCommandBuffer, indexBuffer, 0, VK_INDEX_TYPE_UINT32);
                vkCmdBindDescriptorSets(command.vkCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                    meshDescrSet.getVkPipelineLayout(), 0, 1,
                    &meshDescrSet.getVkDescriptorSet(frameIndex), 0, nullptr);

                for(Id pipeId = 0; pipeId < meshPipeArraySize; ++pipeId){
                    if(nextId == 0){
                        continue;
                    }
                    vkCmdBindPipeline(command.vkCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                        meshPipes[pipeId].getVkPipeline());
                    vkCmdDrawIndexedIndirect(command.vkCommandBuffer,
                        indirectBufferArray[pipeId]->getVkBuffer(frameIndex), 0,
                        nextId, sizeof(VkDrawIndexedIndirectCommand));
                }
            }

        private:
            void initializeBuffer(const IBufferResource& buffer){
                Command command;
                command.begin();
                for(uint32_t frameIndex = 0; frameIndex < MAX_FRAMES_IN_FLIGHT; ++frameIndex){
                    vkCmdFillBuffer(command.vkCommandBuffer, buffer.getVkBuffer(frameIndex), 0,
                        VK_WHOLE_SIZE, 0);
                }
                command.end();
                command.submit();
                command.waitForFence();
            }

            void createIndirectCommandsLayout(Id pipeId){
                VkIndirectCommandsPushConstantTokenEXT pushConstantToken{};
                pushConstantToken.updateRange.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
                pushConstantToken.updateRange.offset = 0;
                pushConstantToken.updateRange.size = 2 * sizeof(uint32_t) + sizeof(float);

                std::array<VkIndirectCommandsLayoutTokenEXT, 2> tokens{};
                tokens[0].sType = VK_STRUCTURE_TYPE_INDIRECT_COMMANDS_LAYOUT_TOKEN_EXT;
                tokens[0].type = VK_INDIRECT_COMMANDS_TOKEN_TYPE_PUSH_CONSTANT_EXT;
                tokens[0].data.pPushConstant = &pushConstantToken;
                tokens[0].offset = offsetof(IndirectDispatch, multiMeshId);
                tokens[1].sType = VK_STRUCTURE_TYPE_INDIRECT_COMMANDS_LAYOUT_TOKEN_EXT;
                tokens[1].type = VK_INDIRECT_COMMANDS_TOKEN_TYPE_DISPATCH_EXT;
                tokens[1].offset = offsetof(IndirectDispatch, dispatch);

                VkIndirectCommandsLayoutCreateInfoEXT layoutInfo{};
                layoutInfo.sType = VK_STRUCTURE_TYPE_INDIRECT_COMMANDS_LAYOUT_CREATE_INFO_EXT;
                layoutInfo.shaderStages = VK_SHADER_STAGE_COMPUTE_BIT;
                layoutInfo.indirectStride = sizeof(IndirectDispatch);
                layoutInfo.pipelineLayout = computeDescrSet.getVkPipelineLayout();
                layoutInfo.tokenCount = static_cast<uint32_t>(tokens.size());
                layoutInfo.pTokens = tokens.data();
                LU_CHECK_VULKAN(createIndirectCommandsLayoutEXT(vkDevice, &layoutInfo, nullptr,
                    &indirectCommandsLayouts[pipeId]), "MultiMeshPackage", "vkCreateIndirectCommandsLayoutEXT")
            }

            void createPreprocessBuffer(Id pipeId){
                VkPhysicalDeviceDeviceGeneratedCommandsPropertiesEXT generatedProperties{};
                generatedProperties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DEVICE_GENERATED_COMMANDS_PROPERTIES_EXT;
                VkPhysicalDeviceProperties2 properties{};
                properties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
                properties.pNext = &generatedProperties;
                vkGetPhysicalDeviceProperties2(vkPhysicalDevice, &properties);
                LU_ASSERT(generatedProperties.maxIndirectSequenceCount >= MAX_INSTANCED_MESHES,
                    "MultiMeshPackage", "createPreprocessBuffer", "Device DGC sequence limit is too small.")
                LU_ASSERT((generatedProperties.supportedIndirectCommandsShaderStagesPipelineBinding & VK_SHADER_STAGE_COMPUTE_BIT) != 0,
                    "MultiMeshPackage", "createPreprocessBuffer", "DGC compute pipeline binding is not supported.")
                LU_ASSERT(generatedProperties.maxIndirectCommandsIndirectStride >= sizeof(IndirectDispatch),
                    "MultiMeshPackage", "createPreprocessBuffer", "DGC indirect stride limit is too small.")
                LU_ASSERT(vkPhysicalDeviceProperties.limits.maxDrawIndirectCount >= MAX_INSTANCED_MESHES,
                    "MultiMeshPackage", "createPreprocessBuffer", "Device multi-draw count limit is too small.")

                VkGeneratedCommandsPipelineInfoEXT pipelineInfo{};
                pipelineInfo.sType = VK_STRUCTURE_TYPE_GENERATED_COMMANDS_PIPELINE_INFO_EXT;
                pipelineInfo.pipeline = compPipes[pipeId].getVkPipeline();
                VkGeneratedCommandsMemoryRequirementsInfoEXT memoryInfo{};
                memoryInfo.sType = VK_STRUCTURE_TYPE_GENERATED_COMMANDS_MEMORY_REQUIREMENTS_INFO_EXT;
                memoryInfo.pNext = &pipelineInfo;
                memoryInfo.indirectCommandsLayout = indirectCommandsLayouts[pipeId];
                memoryInfo.maxSequenceCount = MAX_INSTANCED_MESHES;
                VkMemoryRequirements2 requirements{};
                requirements.sType = VK_STRUCTURE_TYPE_MEMORY_REQUIREMENTS_2;
                getGeneratedCommandsMemoryRequirementsEXT(vkDevice, &memoryInfo, &requirements);

                preprocessBufferSizes[pipeId] = requirements.memoryRequirements.size;
                LU_ASSERT(preprocessBufferSizes[pipeId] > 0 && preprocessBufferSizes[pipeId] <= UINT32_MAX,
                    "MultiMeshPackage", "createPreprocessBuffer", "Invalid DGC preprocess buffer size.")

                VkBufferUsageFlags2CreateInfo usageInfo{};
                usageInfo.sType = VK_STRUCTURE_TYPE_BUFFER_USAGE_FLAGS_2_CREATE_INFO;
                usageInfo.usage = VK_BUFFER_USAGE_2_PREPROCESS_BUFFER_BIT_EXT |
                                  VK_BUFFER_USAGE_2_SHADER_DEVICE_ADDRESS_BIT;
                preprocessBufferArray[pipeId] = std::make_unique<BufferGpuAddress<uint8_t>>(
                    static_cast<uint32_t>(preprocessBufferSizes[pipeId]),
                    VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                    &usageInfo
                );
            }

        };
    }
}