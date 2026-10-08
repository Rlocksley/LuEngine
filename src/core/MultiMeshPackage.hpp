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
        struct MultiMeshMeshInstance{
            Material material{};
            Transform transform{};

            MultiMeshMeshInstance() = default;
            explicit MultiMeshMeshInstance(const Component::MultiMeshInstance& instance)
                : material(instance.material), transform(instance.transform) {}
        };

            struct IndirectDispatch{
                uint32_t multiMeshId{};
                uint32_t transformBufferLength{};
                float deltaTime{};
                VkDispatchIndirectCommand dispatch{};
            };

        static_assert(sizeof(MultiMeshMeshInstance) == 256);
            static_assert(offsetof(IndirectDispatch, deltaTime) == 8);
            static_assert(offsetof(IndirectDispatch, dispatch) == 12);
            static_assert(sizeof(IndirectDispatch) == 24);
        static_assert(sizeof(MultiMesh) == 64);
        static_assert(sizeof(MultiMeshInstance) == 80);

        class MultiMeshPackage{
        private:
            using MultiMeshId = uint32_t;
            using MultiMeshComputePipeId = uint32_t;
            using MultiMeshPipeId = uint32_t;
            using MeshInfoId = uint32_t;

            struct MultiMeshData{
                MultiMeshId id{};
                MultiMeshComputePipeId computePipeId{};
                MultiMeshPipeId pipelineId{};
                MeshInfoId meshInfoId{};
                TransformId parentTransformId{};
                glm::vec4 cullSphere{};
                uint32_t transformCount{};
                std::vector<MultiMeshMeshInstance> transforms;
            };

            struct RetiredMeshInstanceBuffer{
                MultiMeshId id{};
                std::shared_ptr<BufferGpu<MultiMeshMeshInstance>> buffer;
                uint32_t framesRemaining{MAX_FRAMES_IN_FLIGHT};
            };

            DualBuffer<MultiMeshInstance> instanceBuffer;
            DualBuffer<MultiMeshMeshInstance> meshInstanceUploadBuffer;
            SlotBuffer<MultiMesh> multiMeshBuffer;
            std::array<std::shared_ptr<BufferGpu<MultiMeshMeshInstance>>, MAX_MULTI_MESHES> meshInstanceBufferArray{};
            std::array<std::shared_ptr<BufferGpu<IndirectDispatch>>, MAX_MULTI_MESH_COMPUTE_PIPELINES> indirectDispatchBufferArray{};
            std::array<std::shared_ptr<IBufferResource>, MAX_MULTI_MESH_COMPUTE_PIPELINES> indirectDispatchBufferDescriptorArray{};
            std::array<std::shared_ptr<BufferGpu<VkDrawIndexedIndirectCommand>>, MAX_MULTI_MESH_PIPELINES> indirectBufferArray{};
            std::array<std::shared_ptr<IBufferResource>, MAX_MULTI_MESH_PIPELINES> indirectBufferDescriptorArray{};
            std::array<std::unique_ptr<BufferGpu<uint8_t>>, MAX_MULTI_MESH_COMPUTE_PIPELINES> preprocessBufferArray{};
            std::array<VkDeviceSize, MAX_MULTI_MESH_COMPUTE_PIPELINES> preprocessBufferSizes{};

            DescriptorSet transferDescrSet;
            ComputePipeline transferPipe;
            DescriptorSet cullDescrSet;
            ComputePipeline cullPipe;
            DescriptorSet computeDescrSet;
            DescriptorSet meshDescrSet;
            uint32_t compPipeArraySize{0};
            std::array<ComputePipeline, MAX_MULTI_MESH_COMPUTE_PIPELINES> compPipes{};
            std::array<VkIndirectCommandsLayoutEXT, MAX_MULTI_MESH_COMPUTE_PIPELINES> indirectCommandsLayouts{};
            uint32_t meshPipeArraySize{0};
            std::array<GraphicsPipeline<Vertex::Mesh>, MAX_MULTI_MESH_PIPELINES> meshPipes{};

            using MultiMeshEntity = flecs::entity_t;
            using MultiMeshComputePipeEntity = flecs::entity_t;
            using MultiMeshPipeEntity = flecs::entity_t;
            std::unordered_map<MultiMeshEntity, MultiMeshId> entityToMultiMeshId;
            std::unordered_map<MultiMeshId, MultiMeshData> multiMeshes;
            std::unordered_map<MultiMeshComputePipeEntity, MultiMeshComputePipeId> entityToComputePipeId;
            std::unordered_map<MultiMeshPipeEntity, MultiMeshPipeId> entityToMeshPipeId;
            std::unordered_map<MultiMeshId, uint32_t> dirtyIds;
            std::vector<MultiMeshMeshInstance> multiMeshInstancesToUpload;
            std::vector<RetiredMeshInstanceBuffer> retiredMeshInstanceBuffers;
            std::array<uint32_t, MAX_FRAMES_IN_FLIGHT> uploadInstanceCounts{};
            std::array<uint32_t, MAX_FRAMES_IN_FLIGHT> uploadMeshInstanceCounts{};
            std::array<uint32_t, MAX_FRAMES_IN_FLIGHT> uploadMaxMeshInstanceCounts{};

            Command command;
                
        public:
            MultiMeshPackage(const CameraPackage& camera, const TransformPackage& transforms,
                             const GeometryPackage<Vertex::Mesh>& geometry) :
                            instanceBuffer(MAX_ECS_REQUESTS_PROCESSED_PER_FRAME*MAX_FRAMES_IN_FLIGHT,
                                VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT),
                            meshInstanceUploadBuffer(MAX_MULTI_MESH_UPLOAD_INSTANCES,
                                VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | 
                                VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT),
                            multiMeshBuffer(MAX_MULTI_MESHES,
                                VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT),
                            indirectDispatchBufferDescriptorArray(initIndirectDispatchBufferDescriptorArray()),
                            indirectBufferDescriptorArray(initIndirectBufferDescriptorArray()),
                            command()
            {
                initializeBuffer(multiMeshBuffer);

                computeDescrSet
                    .addStorageBuffer(0, multiMeshBuffer, VK_SHADER_STAGE_COMPUTE_BIT)
                    .addPushConstantRange(VK_SHADER_STAGE_COMPUTE_BIT, 0, 2 * sizeof(uint32_t) + sizeof(float))
                    .build();

                transferDescrSet
                    .addStorageBuffer(0, instanceBuffer, VK_SHADER_STAGE_COMPUTE_BIT)
                    .addStorageBuffer(1, meshInstanceUploadBuffer, VK_SHADER_STAGE_COMPUTE_BIT)
                    .addStorageBuffer(2, multiMeshBuffer, VK_SHADER_STAGE_COMPUTE_BIT)
                    .addStorageBufferArray(3, indirectDispatchBufferDescriptorArray, VK_SHADER_STAGE_COMPUTE_BIT)
                    .addStorageBufferArray(4, indirectBufferDescriptorArray, VK_SHADER_STAGE_COMPUTE_BIT)
                    .addStorageBuffer(5, geometry.getGeometryInfoBuffer(), VK_SHADER_STAGE_COMPUTE_BIT)
                    .addPushConstantRange(VK_SHADER_STAGE_COMPUTE_BIT, 0, 2 * sizeof(uint32_t) + sizeof(float))
                    .build();

                transferPipe.create(transferDescrSet.getVkPipelineLayout(), ComputePipelineConfig{
                    .name = "multimesh_transfer",
                    .computeShader = "shader/multi_mesh_transfer.comp.spv"
                });

                cullDescrSet
                    .addUniformBuffer(0, camera.getCameraBuffer(), VK_SHADER_STAGE_COMPUTE_BIT)
                    .addStorageBuffer(1, transforms.getTransformBuffer(), VK_SHADER_STAGE_COMPUTE_BIT)
                    .addStorageBuffer(2, multiMeshBuffer, VK_SHADER_STAGE_COMPUTE_BIT)
                    .addStorageBufferArray(3, indirectDispatchBufferDescriptorArray, VK_SHADER_STAGE_COMPUTE_BIT)
                    .addStorageBufferArray(4, indirectBufferDescriptorArray, VK_SHADER_STAGE_COMPUTE_BIT)
                    .addPushConstantRange(VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(uint32_t) + sizeof(float))
                    .build();

                cullPipe.create(cullDescrSet.getVkPipelineLayout(), ComputePipelineConfig{
                    .name = "multimesh_cull",
                    .computeShader = "shader/multi_mesh_cull.comp.spv"
                });

                meshDescrSet
                    .addUniformBuffer(0, camera.getCameraBuffer(), VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT)
                    .addStorageBuffer(1, transforms.getTransformBuffer(), VK_SHADER_STAGE_VERTEX_BIT)
                    .addStorageBuffer(2, multiMeshBuffer, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT)
                    .build();
            }

            ~MultiMeshPackage(){
                for(VkIndirectCommandsLayoutEXT layout : indirectCommandsLayouts){
                    if(layout != VK_NULL_HANDLE){
                        vkDestroyIndirectCommandsLayoutEXT(vkDevice, layout, nullptr);
                    }
                }
            }

            void createComputePipeline(MultiMeshComputePipeEntity entity, const ComputePipelineConfig& config){
                LU_ASSERT(compPipeArraySize < MAX_MULTI_MESH_COMPUTE_PIPELINES,
                    "MultiMeshPackage", "createComputePipeline", "Compute pipeline array capacity exceeded.")
                LU_ASSERT(entityToComputePipeId.find(entity) == entityToComputePipeId.end(),
                    "MultiMeshPackage", "createComputePipeline", "Compute pipeline entity is already registered.")

                const MultiMeshComputePipeId pipeId = compPipeArraySize++;
                createIndirectCommandsLayout(pipeId);
                compPipes[pipeId].create(computeDescrSet.getVkPipelineLayout(), config, true);
                entityToComputePipeId.emplace(entity, pipeId);
                indirectDispatchBufferArray[pipeId] = std::make_shared<BufferGpu<IndirectDispatch>>(
                    MAX_MULTI_MESHES,
                    VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT |
                    VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT
                );
                initializeBuffer(*indirectDispatchBufferArray[pipeId]);
                indirectDispatchBufferDescriptorArray[pipeId] = indirectDispatchBufferArray[pipeId];
                transferDescrSet.updateDescriptorSetWrites();
                cullDescrSet.updateDescriptorSetWrites();
                createPreprocessBuffer(pipeId);
            }

            void createGraphicsPipeline(MultiMeshPipeEntity entity, const GraphicsPipelineConfig& config){
                LU_ASSERT(meshPipeArraySize < MAX_MULTI_MESH_PIPELINES,
                    "MultiMeshPackage", "createGraphicsPipeline", "Graphics pipeline array capacity exceeded.")
                LU_ASSERT(entityToMeshPipeId.find(entity) == entityToMeshPipeId.end(),
                    "MultiMeshPackage", "createGraphicsPipeline", "Graphics pipeline entity is already registered.")

                const MultiMeshPipeId pipeId = meshPipeArraySize++;
                indirectBufferArray[pipeId] = std::make_shared<BufferGpu<VkDrawIndexedIndirectCommand>>(
                    MAX_MULTI_MESHES,
                    VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT |
                    VK_BUFFER_USAGE_TRANSFER_DST_BIT
                );
                initializeBuffer(*indirectBufferArray[pipeId]);
                indirectBufferDescriptorArray[pipeId] = indirectBufferArray[pipeId];
                transferDescrSet.updateDescriptorSetWrites();
                cullDescrSet.updateDescriptorSetWrites();

                entityToMeshPipeId.emplace(entity, pipeId);
                meshPipes[pipeId].create(meshDescrSet.getVkPipelineLayout(), false, config);
            }

            void createMultiMesh(const MultiMeshEntity entity, const MultiMeshComputePipeEntity computePipeEntity,
                                 const MultiMeshPipeEntity graphicsPipeEntity, const TransformId parentTransformId,
                                 const GeometryInfoId meshInfoId,
                                 const std::vector<Component::MultiMeshInstance>& instances,
                                 const glm::vec4& cullSphere){
                const auto computeIt = entityToComputePipeId.find(computePipeEntity);
                LU_ASSERT(computeIt != entityToComputePipeId.end(), "MultiMeshPackage", "createMultiMesh", "No compute pipeline registered for entity.")
                const auto graphicsIt = entityToMeshPipeId.find(graphicsPipeEntity);
                LU_ASSERT(graphicsIt != entityToMeshPipeId.end(), "MultiMeshPackage", "createMultiMesh", "No graphics pipeline registered for entity.")
                LU_ASSERT(instances.size() > 0,
                    "MultiMeshPackage", "createMultiMesh", "A multi mesh has no instances.")
                LU_ASSERT(instances.size() <= MAX_MULTI_MESH_INSTANCES,
                    "MultiMeshPackage", "createMultiMesh", "A multi mesh exceeds the multi mesh mesh instance limit.")

                MultiMeshId id;
                const auto entityIt = entityToMultiMeshId.find(entity);
                if(entityIt == entityToMultiMeshId.end()){
                    id = multiMeshBuffer.allocate();
                    entityToMultiMeshId.emplace(entity, id);
                }else{
                    id = entityIt->second;
                    auto oldIt = multiMeshes.find(id);
                    if((oldIt != multiMeshes.end()) && (instances.size() != meshInstanceBufferArray[id]->getSize())){
                        retiredMeshInstanceBuffers.push_back({id, std::move(meshInstanceBufferArray[id])});
                    }
                }


                MultiMeshData data{};
                data.id = id;
                data.computePipeId = computeIt->second;
                data.pipelineId = graphicsIt->second;
                data.meshInfoId = meshInfoId;
                data.parentTransformId = parentTransformId;
                data.cullSphere = cullSphere;
                data.transformCount = static_cast<uint32_t>(instances.size());
                data.transforms.reserve(instances.size());
                for(const auto& instance : instances){
                    data.transforms.emplace_back(instance);
                }

                if(!meshInstanceBufferArray[id] ||
                   instances.size() != meshInstanceBufferArray[id]->getSize()){
                    meshInstanceBufferArray[id] = std::make_shared<BufferGpu<MultiMeshMeshInstance>>(
                        static_cast<uint32_t>(data.transforms.size()),
                        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | 
                        VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT);
                }

                multiMeshes[id] = std::move(data);
                dirtyIds[id] = MAX_FRAMES_IN_FLIGHT;
            }

            void destroyMultiMesh(MultiMeshEntity entity){
                const auto entityIt = entityToMultiMeshId.find(entity);
                LU_ASSERT(entityIt != entityToMultiMeshId.end(),
                    "MultiMeshPackage", "destroyMultiMesh", "MultiMesh entity does not exist.")

                const MultiMeshId id = entityIt->second;
                auto dataIt = multiMeshes.find(id);
                if(dataIt != multiMeshes.end()){
                    retiredMeshInstanceBuffers.push_back({id, std::move(meshInstanceBufferArray[id]), MAX_FRAMES_IN_FLIGHT});
                }else{
                    LU_LOGE("MultiMeshPackage", "destroyMultiMesh", "multi mesh mesh instance data for entity does not exist.")
                }
                multiMeshBuffer.free(id);
                multiMeshes.erase(dataIt);
                entityToMultiMeshId.erase(entityIt);
                dirtyIds[id] = MAX_FRAMES_IN_FLIGHT;
            }

            void collectDirty(uint32_t frameIndex){
                multiMeshInstancesToUpload.clear();
                uploadInstanceCounts[frameIndex] = 0;
                uploadMeshInstanceCounts[frameIndex] = 0;
                uploadMaxMeshInstanceCounts[frameIndex] = 0;

                std::erase_if(dirtyIds, [&](auto& pair){
                    auto& [id, framesRemaining] = pair;

                    auto dataIt = multiMeshes.find(id);
                    if(dataIt != multiMeshes.end()){
                        MultiMeshData& data = dataIt->second;

                        MultiMesh multiMesh{};
                        multiMesh.computePipeId = data.computePipeId;
                        multiMesh.pipelineId = data.pipelineId;
                        multiMesh.meshInfoId = data.meshInfoId;
                        multiMesh.transformId = data.parentTransformId;
                        multiMesh.cullSphere = data.cullSphere;
                        multiMesh.size = static_cast<uint32_t>(data.transforms.size());
                        multiMesh.meshInstanceBufferAddress[0] = meshInstanceBufferArray[id]->getVkDeviceAddress(frameIndex%MAX_FRAMES_IN_FLIGHT);
                        multiMesh.meshInstanceBufferAddress[1] = meshInstanceBufferArray[id]->getVkDeviceAddress((frameIndex+1)%MAX_FRAMES_IN_FLIGHT);
                        multiMesh.valid = 1;

                        uint32_t rangeBegin = meshInstanceUploadBuffer.sizeCpu(frameIndex);
                        uint32_t size = static_cast<uint32_t>(data.transforms.size());

                        uploadInstanceCounts[frameIndex] += 1;
                        uploadMeshInstanceCounts[frameIndex] += size;
                        uploadMaxMeshInstanceCounts[frameIndex] = std::max(uploadMaxMeshInstanceCounts[frameIndex], size); 

                        instanceBuffer.push_back
                        (frameIndex,
                        MultiMeshInstance{
                            multiMesh,
                            id,
                            rangeBegin
                        });

                        meshInstanceUploadBuffer.push_range_back(frameIndex, data.transforms);
                    }else{
                        instanceBuffer.push_back
                        (frameIndex, 
                        MultiMeshInstance{
                            MultiMesh{}, 
                            id
                        });
                    }

                    --framesRemaining;
                    return framesRemaining <= 0;
                });


                std::erase_if(retiredMeshInstanceBuffers, [&](RetiredMeshInstanceBuffer& retired){
                    retired.framesRemaining--;
                    return retired.framesRemaining <= 0;
                });
            }

            void copy(uint32_t frameIndex, const Command& command){
                if(uploadInstanceCounts[frameIndex] == 0){
                    return;
                }

                instanceBuffer.copyToGpu(frameIndex, command);
                meshInstanceUploadBuffer.copyToGpu(frameIndex, command);
            }

            void transfer(uint32_t frameIndex, const Command& command){
                const uint32_t uploadInstanceCount = uploadInstanceCounts[frameIndex];
                if(uploadInstanceCount == 0){
                    return;
                }
                vkCmdBindPipeline
                (command.vkCommandBuffer, 
                VK_PIPELINE_BIND_POINT_COMPUTE,
                transferPipe.getVkPipeline());

                vkCmdBindDescriptorSets
                (command.vkCommandBuffer, 
                VK_PIPELINE_BIND_POINT_COMPUTE,
                transferDescrSet.getVkPipelineLayout(), 
                0, 1,
                &transferDescrSet.getVkDescriptorSet(frameIndex), 
                0, nullptr);

                struct TransferPushConstants{
                    uint32_t instanceCount;
                    uint32_t meshInstanceCount;
                    float deltaTime;
                } transferConstants {                    
                    uploadInstanceCount,
                    uploadMeshInstanceCounts[frameIndex],
                    Time::deltaTime
                };
                
                vkCmdPushConstants
                (command.vkCommandBuffer, 
                transferDescrSet.getVkPipelineLayout(),
                VK_SHADER_STAGE_COMPUTE_BIT, 
                0, sizeof(transferConstants), 
                &transferConstants);

                vkCmdDispatch
                (command.vkCommandBuffer,
                std::max(1u, (uploadMaxMeshInstanceCounts[frameIndex] + 255u) / 256u),
                uploadInstanceCount,
                1);
            }

            void cull(uint32_t frameIndex, const Command& command) const{
                vkCmdBindPipeline
                (command.vkCommandBuffer, 
                VK_PIPELINE_BIND_POINT_COMPUTE,
                cullPipe.getVkPipeline());

                vkCmdBindDescriptorSets
                (command.vkCommandBuffer, 
                VK_PIPELINE_BIND_POINT_COMPUTE,
                cullDescrSet.getVkPipelineLayout(), 
                0, 1,
                &cullDescrSet.getVkDescriptorSet(frameIndex), 
                0, nullptr);
                
                struct CullPushConstants{
                    uint32_t multiMeshCount;
                    float deltaTime;
                } cullConstants{
                    multiMeshBuffer.size(), 
                    Time::deltaTime
                };

                vkCmdPushConstants
                (command.vkCommandBuffer, 
                cullDescrSet.getVkPipelineLayout(),
                VK_SHADER_STAGE_COMPUTE_BIT, 
                0, sizeof(cullConstants), 
                &cullConstants);

                vkCmdDispatch
                (command.vkCommandBuffer, 
                (multiMeshBuffer.size() + 255) / 256, 
                1, 
                1);
            }

            void executeCompute(uint32_t frameIndex, const Command& command) const{
                for(MultiMeshComputePipeId pipeId = 0; pipeId < compPipeArraySize; ++pipeId){
                    vkCmdBindPipeline
                    (command.vkCommandBuffer, 
                    VK_PIPELINE_BIND_POINT_COMPUTE,
                    compPipes[pipeId].getVkPipeline());

                    vkCmdBindDescriptorSets
                    (command.vkCommandBuffer, 
                    VK_PIPELINE_BIND_POINT_COMPUTE,
                    computeDescrSet.getVkPipelineLayout(), 
                    0, 1,
                    &computeDescrSet.getVkDescriptorSet(frameIndex), 
                    0, nullptr);

                    VkGeneratedCommandsPipelineInfoEXT pipelineInfo{};
                    pipelineInfo.sType = VK_STRUCTURE_TYPE_GENERATED_COMMANDS_PIPELINE_INFO_EXT;
                    pipelineInfo.pipeline = compPipes[pipeId].getVkPipeline();

                    VkGeneratedCommandsInfoEXT generatedInfo{};
                    generatedInfo.sType = VK_STRUCTURE_TYPE_GENERATED_COMMANDS_INFO_EXT;
                    generatedInfo.pNext = &pipelineInfo;
                    generatedInfo.shaderStages = VK_SHADER_STAGE_COMPUTE_BIT;
                    generatedInfo.indirectCommandsLayout = indirectCommandsLayouts[pipeId];
                    generatedInfo.indirectAddress = indirectDispatchBufferArray[pipeId]->getVkDeviceAddress(frameIndex);
                    generatedInfo.indirectAddressSize = sizeof(IndirectDispatch) * multiMeshBuffer.size();
                    generatedInfo.preprocessAddress = preprocessBufferArray[pipeId]->getVkDeviceAddress(frameIndex);
                    generatedInfo.preprocessSize = preprocessBufferSizes[pipeId];
                    generatedInfo.maxSequenceCount = multiMeshBuffer.size();
                    if(generatedInfo.maxSequenceCount > 0){
                        vkCmdExecuteGeneratedCommandsEXT(command.vkCommandBuffer, VK_FALSE, &generatedInfo);
                    }
                }
            }

            void draw(uint32_t frameIndex, const GeometryPackage<Vertex::Mesh>& geometry,
                      const Command& command) const{
                
                const VkBuffer vertexBuffer = geometry.getVertexBuffer();
                const VkBuffer indexBuffer = geometry.getIndexBuffer();
                const VkDeviceSize offset = 0;
                
                vkCmdBindVertexBuffers
                (command.vkCommandBuffer, 
                0, 1, 
                &vertexBuffer, 
                &offset);

                vkCmdBindIndexBuffer
                (command.vkCommandBuffer, 
                indexBuffer, 
                0, 
                VK_INDEX_TYPE_UINT32);

                vkCmdBindDescriptorSets
                (command.vkCommandBuffer, 
                VK_PIPELINE_BIND_POINT_GRAPHICS,
                meshDescrSet.getVkPipelineLayout(), 
                0, 1,
                &meshDescrSet.getVkDescriptorSet(frameIndex), 
                0, nullptr);

                for(MultiMeshPipeId pipeId = 0; pipeId < meshPipeArraySize; ++pipeId){
                    vkCmdBindPipeline
                    (command.vkCommandBuffer, 
                    VK_PIPELINE_BIND_POINT_GRAPHICS,
                    meshPipes[pipeId].getVkPipeline());

                    vkCmdDrawIndexedIndirect
                    (command.vkCommandBuffer,
                    indirectBufferArray[pipeId]->getVkBuffer(frameIndex), 
                    0,
                    multiMeshBuffer.size(), 
                    sizeof(VkDrawIndexedIndirectCommand));
                }
            }

        private:
            void initializeBuffer(const IBufferResource& buffer){
                command.begin();
                for(uint32_t frameIndex = 0; frameIndex < MAX_FRAMES_IN_FLIGHT; ++frameIndex){
                    vkCmdFillBuffer(command.vkCommandBuffer, buffer.getVkBuffer(frameIndex), 0,
                        VK_WHOLE_SIZE, 0);
                }
                command.end();
                command.submit();
                command.waitForFence();
            }

            void createIndirectCommandsLayout(MultiMeshComputePipeId pipeId){
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
                
                LU_CHECK_VULKAN
                (vkCreateIndirectCommandsLayoutEXT
                (vkDevice, &layoutInfo, nullptr,
                &indirectCommandsLayouts[pipeId]),
                "MultiMeshPackage", 
                "vkCreateIndirectCommandsLayoutEXT")
            }

            void createPreprocessBuffer(MultiMeshComputePipeId pipeId){
                VkPhysicalDeviceDeviceGeneratedCommandsPropertiesEXT generatedProperties{};
                generatedProperties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DEVICE_GENERATED_COMMANDS_PROPERTIES_EXT;
                VkPhysicalDeviceProperties2 properties{};
                properties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
                properties.pNext = &generatedProperties;
                vkGetPhysicalDeviceProperties2(vkPhysicalDevice, &properties);
                LU_ASSERT(generatedProperties.maxIndirectSequenceCount >= MAX_MULTI_MESHES,
                    "MultiMeshPackage", "createPreprocessBuffer", "Device DGC sequence limit is too small.")
                LU_ASSERT((generatedProperties.supportedIndirectCommandsShaderStagesPipelineBinding & VK_SHADER_STAGE_COMPUTE_BIT) != 0,
                    "MultiMeshPackage", "createPreprocessBuffer", "DGC compute pipeline binding is not supported.")
                LU_ASSERT(generatedProperties.maxIndirectCommandsIndirectStride >= sizeof(IndirectDispatch),
                    "MultiMeshPackage", "createPreprocessBuffer", "DGC indirect stride limit is too small.")
                LU_ASSERT(vkPhysicalDeviceProperties.limits.maxDrawIndirectCount >= MAX_MULTI_MESHES,
                    "MultiMeshPackage", "createPreprocessBuffer", "Device multi-draw count limit is too small.")

                VkGeneratedCommandsPipelineInfoEXT pipelineInfo{};
                pipelineInfo.sType = VK_STRUCTURE_TYPE_GENERATED_COMMANDS_PIPELINE_INFO_EXT;
                pipelineInfo.pipeline = compPipes[pipeId].getVkPipeline();
                VkGeneratedCommandsMemoryRequirementsInfoEXT memoryInfo{};
                memoryInfo.sType = VK_STRUCTURE_TYPE_GENERATED_COMMANDS_MEMORY_REQUIREMENTS_INFO_EXT;
                memoryInfo.pNext = &pipelineInfo;
                memoryInfo.indirectCommandsLayout = indirectCommandsLayouts[pipeId];
                memoryInfo.maxSequenceCount = MAX_MULTI_MESH_INSTANCES;
                VkMemoryRequirements2 requirements{};
                requirements.sType = VK_STRUCTURE_TYPE_MEMORY_REQUIREMENTS_2;
                vkGetGeneratedCommandsMemoryRequirementsEXT(vkDevice, &memoryInfo, &requirements);

                preprocessBufferSizes[pipeId] = requirements.memoryRequirements.size;
                LU_ASSERT(preprocessBufferSizes[pipeId] > 0 && preprocessBufferSizes[pipeId] <= UINT32_MAX,
                    "MultiMeshPackage", "createPreprocessBuffer", "Invalid DGC preprocess buffer size.")

                VkBufferUsageFlags2CreateInfo usageInfo{};
                usageInfo.sType = VK_STRUCTURE_TYPE_BUFFER_USAGE_FLAGS_2_CREATE_INFO;
                usageInfo.usage = VK_BUFFER_USAGE_2_PREPROCESS_BUFFER_BIT_EXT |
                                  VK_BUFFER_USAGE_2_SHADER_DEVICE_ADDRESS_BIT;
                preprocessBufferArray[pipeId] = std::make_unique<BufferGpu<uint8_t>>(
                    static_cast<uint32_t>(preprocessBufferSizes[pipeId]),
                    VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                    &usageInfo
                );
            }

            static std::shared_ptr<IBufferResource> initDummyIndirectDispatchBuffer() {
                return std::make_shared<BufferGpu<IndirectDispatch>>(
                    1,
                    VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                    VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT |
                    VK_BUFFER_USAGE_TRANSFER_DST_BIT
                );
            }

            static std::array<std::shared_ptr<IBufferResource>, MAX_MULTI_MESH_PIPELINES> initIndirectDispatchBufferDescriptorArray() {
                std::array<std::shared_ptr<IBufferResource>, MAX_MULTI_MESH_PIPELINES> buffers;
                buffers.fill(initDummyIndirectDispatchBuffer());
                return buffers;
            }

            static std::shared_ptr<IBufferResource> initDummyIndirectBuffer() {
                return std::make_shared<BufferGpu<VkDrawIndexedIndirectCommand>>(
                    1,
                    VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                    VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT |
                    VK_BUFFER_USAGE_TRANSFER_DST_BIT
                );
            }

            static std::array<std::shared_ptr<IBufferResource>, MAX_MULTI_MESH_PIPELINES> initIndirectBufferDescriptorArray() {
                std::array<std::shared_ptr<IBufferResource>, MAX_MULTI_MESH_PIPELINES> buffers;
                buffers.fill(initDummyIndirectBuffer());
                return buffers;
            }
        };
    }
}