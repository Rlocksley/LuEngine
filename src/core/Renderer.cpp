#include "Renderer.hpp"
#include "Core.hpp"
#include "Constant.hpp"
#include "Channel.hpp"
#include "Swapchain.hpp"
#include "Framebuffers.hpp"
#include "Exit.hpp"
#include "Texture.hpp"
#include "instance/MaterialInstance.hpp"
namespace Lu{
    namespace Core{
        Renderer::Renderer()
            : command{},
            camera(),
            transform(),
            lights(camera, transform),
            meshGeometry(
                MAX_MESH_VERTEX_BUFFER_SIZE,
                MAX_MESH_INDEX_BUFFER_SIZE,
                MAX_VERTICES_PER_MESH,
                MAX_INDICES_PER_MESH,
                MAX_MESH_INFOS
            ),
            texture(),
            mesh(camera, lights, transform, texture, meshGeometry),
            multiMesh(camera, transform, meshGeometry),
            skeletonGeometry(
                MAX_SKELETON_VERTEX_BUFFER_SIZE,
                MAX_SKELETON_INDEX_BUFFER_SIZE,
                MAX_VERTICES_PER_MESH,
                MAX_INDICES_PER_MESH,
                MAX_SKELETON_MESH_INFOS
            ),
            skeleton(camera, lights, transform, texture, skeletonGeometry) {
        }

        Renderer::~Renderer() {
            // run() may leave up to MAX_FRAMES_IN_FLIGHT submissions pending.
            LU_CHECK_VULKAN(vkDeviceWaitIdle(vkDevice), "Renderer::~Renderer", "vkDeviceWaitIdle")
        }

        void Renderer::run(FramerateMonitor& framerateMonitor) {
            while (Core::isRunning.load(std::memory_order_relaxed) && Core::updateCore()) {
                // Reuse the command buffer and frame-indexed resources only
                // after this frame slot's previous submission has completed.
                command[frameIndex].waitForFence();
                const VkResult acquireResult = Core::getSwapchainImageIndex(frameIndex);
                if(acquireResult == VK_ERROR_OUT_OF_DATE_KHR)
                {
                    Core::recreateSwapchain();
                    continue;
                }
                processEcsRequests();
                record();
                const VkResult presentResult = submit();
                framerateMonitor.recordRendererFrameAndPrint();
                frameIndex = (frameIndex + 1) % MAX_FRAMES_IN_FLIGHT;

                if(acquireResult == VK_SUBOPTIMAL_KHR || presentResult == VK_SUBOPTIMAL_KHR ||
                    presentResult == VK_ERROR_OUT_OF_DATE_KHR)
                {
                    Core::recreateSwapchain();
                }
            }

            if(Core::isRunning.load(std::memory_order_relaxed)){
                Core::isRunning.store(false, std::memory_order_relaxed);
            }
        }

        void Renderer::createMeshPipe(const flecs::entity_t entity, const GraphicsPipelineConfig& config){
            mesh.createMeshPipeline(entity, config);
        }
        
        void Renderer::createMeshGeometry(const flecs::entity_t entity, const std::vector<Vertex::Mesh>& vb, const std::vector<uint32_t> ib){
            meshGeometry.addGeometry(entity, vb, ib);
        }

        void Renderer::createSkeletonPipe(const flecs::entity_t entity, const GraphicsPipelineConfig& config){
            skeleton.createSkeletonPipeline(entity, config);
        }

        void Renderer::createSkeletonGeometry(const flecs::entity_t entity, const std::vector<Vertex::Skeleton>& vb, const std::vector<uint32_t>& ib){
            skeletonGeometry.addGeometry(entity, vb, ib);
        }

        void Renderer::createSkeletonAsset(const flecs::entity_t entity, const SkeletonAsset& asset){
            skeleton.createSkeletonAsset(entity, asset);
        }

        void Renderer::createMultiMeshPipe(const flecs::entity_t entity, const GraphicsPipelineConfig& config){
            multiMesh.createGraphicsPipeline(entity, config);
        }

        void Renderer::createMultiMeshComputePipe(const flecs::entity_t entity, const ComputePipelineConfig& config){
            multiMesh.createComputePipeline(entity, config);
        }

        void Renderer::createTexture(const TextureEntity entity, const std::string& filePath){
            texture.createTexture(entity, filePath);
            mesh.updateDescriptorSetWrites();
            skeleton.updateDescriptorSetWrites();
        }

        void Renderer::createTexture(const TextureEntity entity, TextureCpu&& textureCpu){
            texture.createTexture(entity, std::move(textureCpu));
            mesh.updateDescriptorSetWrites();
            skeleton.updateDescriptorSetWrites();
        }
        
        void Renderer::processEcsRequests(){
            auto vecEcsRequests = GetChannel().drain(MAX_ECS_REQUESTS_PROCESSED_PER_FRAME);
            for(auto& request : vecEcsRequests){
                std::visit(variant_match{
                     
                        [&](const EcsRequest::CreateTransform& req){
                            transform.createTransform(req.entity, req.transform);
                        },

                        [&](const EcsRequest::DestroyTransform& req){
                            transform.destroyTransform(req.entity);    
                        },

                        [&](const EcsRequest::CreateMeshTransform& req){
                            transform.createLocalTransform(req.entity, req.transform);
                        },

                        [&](const EcsRequest::DestroyMeshTransform& req){
                            transform.destroyLocalTransform(req.entity);
                        },
                        
                        [&](const EcsRequest::CreateMesh& req){
                            uint32_t textureIds[5]{UINT32_MAX,UINT32_MAX,UINT32_MAX,UINT32_MAX,UINT32_MAX};

                            for(uint32_t i = 0; i < 5; i++){
                                if(req.material.textures[i] != 0){
                                    textureIds[i] = texture.getTextureId(req.material.textures[i]);
                                }
                            }

                            Material material(
                                textureIds, 
                                req.material.albedo, 
                                req.material.ambient, 
                                req.material.emission, 
                                req.material.roughness, 
                                req.material.metallic
                            );
                     
                            mesh.createMesh(
                                req.entity, req.pipe,
                                transform.getTransformId(req.parent),
                                transform.getLocalTransformId(req.parent),
                                meshGeometry.getGeometryInfoId(req.mesh),
                                material
                            );
                        },
                        
                        [&](const EcsRequest::DestroyMesh& req){
                            mesh.destroyMesh(req.entity);    
                        },

                        [&](EcsRequest::CreateMultiMesh& req){
                            pendingMultiMeshCreates.push_back(std::move(req));
                        },

                        [&](const EcsRequest::DestroyMultiMesh& req){
                            multiMesh.destroyMultiMesh(req.entity);
                        },

                        [&](const EcsRequest::CreateLight& req){
                            lights.createLight(
                                req.entity, 
                                req.light, 
                                transform.getTransformId(req.parent)
                            );
                        },

                        [&](const EcsRequest::DestroyLight& req){
                            lights.destroyLight(req.entity);
                        },
                        
                        [&](const EcsRequest::CreateSkeleton& req){
                            skeleton.createSkeleton(req.entity, req.asset);
                        },

                        [&](const EcsRequest::DestroySkeleton& req){
                            skeleton.destroySkeleton(req.entity);
                        },

                        [&](const EcsRequest::CreateSkeletonMesh& req){
                            uint32_t textureIds[5]{UINT32_MAX,UINT32_MAX,UINT32_MAX,UINT32_MAX,UINT32_MAX};
                            for(uint32_t i = 0; i < 5; i++){
                                if(req.material.textures[i] != 0){
                                    textureIds[i] = texture.getTextureId(req.material.textures[i]);
                                }
                            }
                            Material material(textureIds, req.material.albedo, req.material.ambient,
                                req.material.emission, req.material.roughness, req.material.metallic);
                            skeleton.createSkeletonMesh(
                                req.entity, req.parent, req.pipe,
                                transform.getTransformId(req.parent),
                                transform.getLocalTransformId(req.parent),
                                skeletonGeometry.getGeometryInfoId(req.mesh),
                                material, req.nodeIndex
                            );
                        },

                        [&](const EcsRequest::DestroySkeletonMesh& req){
                            skeleton.destroySkeletonMesh(req.entity);
                        },

                        [&](const EcsRequest::UpdateCamera& req){
                            camera.update(req);
                        }

                    }, 
                    request
                );
            }

            if(!pendingMultiMeshCreates.empty()){
                EcsRequest::CreateMultiMesh request = std::move(pendingMultiMeshCreates.front());
                pendingMultiMeshCreates.pop_front();
                multiMesh.createMultiMesh(
                    request.entity,
                    request.computePipe,
                    request.pipe,
                    transform.getTransformId(request.parent),
                    meshGeometry.getGeometryInfoId(request.mesh),
                    request.instances,
                    request.cullSphere
                );
            }
            
            // Apply the latest dynamic transforms after lifecycle requests have
            // established or removed entity-to-transform ID mappings.
            transform.updateTransforms();

            // Latest animated skeleton poses (triple buffer) -> staging buffer of this frame
            skeleton.updatePoses(frameIndex);

            //Writes the Dirty Elements to the InstanceBuffer
            transform.collectDirty(frameIndex);
            mesh.collectDirty(frameIndex);
            multiMesh.collectDirty(frameIndex);
            skeleton.collectDirty(frameIndex);
        }

        void Renderer::record(){
            if (depthMapView != depthMap.vkImageView ||
                depthMapExtent.width != depthMap.width ||
                depthMapExtent.height != depthMap.height) {
                lights.updateDepthMapDescriptorSet();
                depthMapView = depthMap.vkImageView;
                depthMapExtent = {depthMap.width, depthMap.height};
                depthMapInitialized = false;
            }
            
            const auto& cmd = command[frameIndex];

            //begin command buffer recording
            cmd.begin();

            
            //Copy To GPU
            meshGeometry.copy(frameIndex, cmd);
            texture.copy(frameIndex, cmd);
            camera.copy(frameIndex, cmd);
            transform.copy(frameIndex, cmd);
            lights.copy(frameIndex, cmd);
            mesh.copy(frameIndex, cmd);
            multiMesh.copy(frameIndex, cmd);
            skeletonGeometry.copy(frameIndex, cmd);
            skeleton.copy(frameIndex, cmd);
            
            //Pipeline Barrier
            copyBarrier(cmd);

            //Transfer 
            //InstanceBuffer Elements (Type,Id) 
            //to Buffer Elements (Type) 
            //via compute shader
            transform.transfer(frameIndex, cmd);
            mesh.transfer(frameIndex, cmd);
            multiMesh.transfer(frameIndex, cmd);
            skeleton.transfer(frameIndex, cmd);   // poses -> SkeletonBuffer, mesh instances

            //Pipeline Barrier
            transferBarrier(cmd);

            //Mesh Culling
            mesh.cull(frameIndex, cmd);
            multiMesh.cull(frameIndex, cmd);
            skeleton.cull(frameIndex, cmd);

            //Pipeline Barrier
            cullBarrier(cmd);

            //move transforms/change materials of 
            //multimesh meshinstances via compute shader
            multiMesh.move(frameIndex, cmd);

            //Pipeline Barrier
            cullBarrier(cmd);

            // Render mesh depth for tile light culling.
            beginDepthRendering(cmd);
            mesh.depth(frameIndex, meshGeometry, cmd);
            skeleton.depth(frameIndex, skeletonGeometry, cmd);
            endDepthRendering(cmd);

            // Cull lights against the depth prepass.
            lights.cull(frameIndex, cmd);

            recordBarrier(
                cmd.vkCommandBuffer,
                VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                VK_ACCESS_SHADER_WRITE_BIT,
                VK_ACCESS_SHADER_READ_BIT
            );

            //begin dynamic rendering
            beginRendering(cmd);

            //Indirect Draws of all MeshPipelines
            mesh.draw(frameIndex, meshGeometry, cmd);
            multiMesh.draw(frameIndex, meshGeometry, cmd);
            skeleton.draw(frameIndex, skeletonGeometry, cmd);

            //end dynamic rendering
            endRendering(cmd);

            //end command buffer recording
            cmd.end();
        }

        void Renderer::beginDepthRendering(const Command& cmd) {
            VkImageMemoryBarrier depthBarrier{};
            depthBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
            depthBarrier.srcAccessMask = depthMapInitialized ? VK_ACCESS_SHADER_READ_BIT : 0;
            depthBarrier.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT |
                VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
            depthBarrier.oldLayout = depthMapInitialized
                ? VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL
                : VK_IMAGE_LAYOUT_UNDEFINED;
            depthBarrier.newLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
            depthBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            depthBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            depthBarrier.image = depthMap.vkImage;
            depthBarrier.subresourceRange = {VK_IMAGE_ASPECT_DEPTH_BIT, 0, 1, 0, 1};

            vkCmdPipelineBarrier(
                cmd.vkCommandBuffer,
                depthMapInitialized
                    ? VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT
                    : VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT |
                    VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
                0,
                0, nullptr,
                0, nullptr,
                1, &depthBarrier
            );

            VkRenderingAttachmentInfo depthAttachment{};
            depthAttachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
            depthAttachment.imageView = depthMap.vkImageView;
            depthAttachment.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
            depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
            depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
            depthAttachment.clearValue.depthStencil = {1.0f, 0};

            VkRenderingInfo renderingInfo{};
            renderingInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
            renderingInfo.renderArea.offset = {0, 0};
            renderingInfo.renderArea.extent = vkExtent2D;
            renderingInfo.layerCount = 1;
            renderingInfo.pDepthAttachment = &depthAttachment;

            vkCmdBeginRendering(cmd.vkCommandBuffer, &renderingInfo);
            depthMapInitialized = true;
        }

        void Renderer::endDepthRendering(const Command& cmd) {
            vkCmdEndRendering(cmd.vkCommandBuffer);

            VkImageMemoryBarrier depthBarrier{};
            depthBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
            depthBarrier.srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
            depthBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
            depthBarrier.oldLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
            depthBarrier.newLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
            depthBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            depthBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            depthBarrier.image = depthMap.vkImage;
            depthBarrier.subresourceRange = {VK_IMAGE_ASPECT_DEPTH_BIT, 0, 1, 0, 1};

            vkCmdPipelineBarrier(
                cmd.vkCommandBuffer,
                VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT |
                    VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
                VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                0,
                0, nullptr,
                0, nullptr,
                1, &depthBarrier
            );
        }

        VkResult Renderer::submit(){
            const auto& cmd = command[frameIndex];

            cmd.submitGraphics(frameIndex);
            return cmd.presentGraphics();
        }


        void Renderer::beginRendering(const Command& cmd){
            
            const uint32_t imageIndex = swapchain.imageIndex;
            const bool multisampled = vkSampleCountFlagBits != VK_SAMPLE_COUNT_1_BIT;
            const VkImage colorImage = multisampled
                ? framebuffers.colorImages[imageIndex].vkImage
                : swapchain.vkImages[imageIndex];
            const VkImageView colorImageView = multisampled
                ? framebuffers.colorImages[imageIndex].vkImageView
                : swapchain.vkImageViews[imageIndex];

            VkImageMemoryBarrier attachmentBarriers[3]{};
            uint32_t attachmentBarrierCount = 0;

            VkImageMemoryBarrier& swapchainBarrier = attachmentBarriers[attachmentBarrierCount++];
            swapchainBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
            swapchainBarrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
            swapchainBarrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            swapchainBarrier.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
            swapchainBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            swapchainBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            swapchainBarrier.image = swapchain.vkImages[imageIndex];
            swapchainBarrier.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};

            if (multisampled) {
                VkImageMemoryBarrier& colorBarrier = attachmentBarriers[attachmentBarrierCount++];
                colorBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
                colorBarrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
                colorBarrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
                colorBarrier.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
                colorBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
                colorBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
                colorBarrier.image = colorImage;
                colorBarrier.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
            }

            VkImageMemoryBarrier& depthBarrier = attachmentBarriers[attachmentBarrierCount++];
            depthBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
            depthBarrier.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT |
                VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
            depthBarrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            depthBarrier.newLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
            depthBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            depthBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            depthBarrier.image = framebuffers.depthImages[imageIndex].vkImage;
            depthBarrier.subresourceRange = {VK_IMAGE_ASPECT_DEPTH_BIT, 0, 1, 0, 1};

            vkCmdPipelineBarrier(
                cmd.vkCommandBuffer,
                VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
                    VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT |
                    VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
                0,
                0, nullptr,
                0, nullptr,
                attachmentBarrierCount,
                attachmentBarriers
            );

            VkRenderingAttachmentInfo colorAttachment{};
            colorAttachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
            colorAttachment.imageView = colorImageView;
            colorAttachment.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
            colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
            colorAttachment.storeOp = multisampled
                ? VK_ATTACHMENT_STORE_OP_DONT_CARE
                : VK_ATTACHMENT_STORE_OP_STORE;
            colorAttachment.clearValue.color = {{0.0f, 0.0f, 0.0f, 1.0f}};
            if (multisampled) {
                colorAttachment.resolveMode = VK_RESOLVE_MODE_AVERAGE_BIT;
                colorAttachment.resolveImageView = swapchain.vkImageViews[imageIndex];
                colorAttachment.resolveImageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
            }

            VkRenderingAttachmentInfo depthAttachment{};
            depthAttachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
            depthAttachment.imageView = framebuffers.depthImages[imageIndex].vkImageView;
            depthAttachment.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
            depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
            depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
            depthAttachment.clearValue.depthStencil = {1.0f, 0};

            VkRenderingInfo renderingInfo{};
            renderingInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
            renderingInfo.renderArea.offset = {0, 0};
            renderingInfo.renderArea.extent = vkExtent2D;
            renderingInfo.layerCount = 1;
            renderingInfo.colorAttachmentCount = 1;
            renderingInfo.pColorAttachments = &colorAttachment;
            renderingInfo.pDepthAttachment = &depthAttachment;

            vkCmdBeginRendering(cmd.vkCommandBuffer, &renderingInfo);
        }

        void Renderer::endRendering(const Command& cmd){
            
            const uint32_t imageIndex = swapchain.imageIndex;
            vkCmdEndRendering(cmd.vkCommandBuffer);

            VkImageMemoryBarrier presentBarrier{};
            presentBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
            presentBarrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
            presentBarrier.oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
            presentBarrier.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
            presentBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            presentBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            presentBarrier.image = swapchain.vkImages[imageIndex];
            presentBarrier.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};

            vkCmdPipelineBarrier(
                cmd.vkCommandBuffer,
                VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
                0,
                0, nullptr,
                0, nullptr,
                1, &presentBarrier
            );
        }

        void Renderer::copyBarrier(const Command& cmd) {
            recordBarrier(
                cmd.vkCommandBuffer,
                VK_PIPELINE_STAGE_TRANSFER_BIT,
                VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT |
                    VK_PIPELINE_STAGE_VERTEX_SHADER_BIT |
                    VK_PIPELINE_STAGE_VERTEX_INPUT_BIT,
                VK_ACCESS_TRANSFER_WRITE_BIT,
                VK_ACCESS_SHADER_READ_BIT |
                    VK_ACCESS_SHADER_WRITE_BIT |
                    VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT |
                    VK_ACCESS_INDEX_READ_BIT
            );
        }

        void Renderer::transferBarrier(const Command& cmd) {
            recordBarrier(
                cmd.vkCommandBuffer,
                VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_VERTEX_SHADER_BIT,
                VK_ACCESS_SHADER_WRITE_BIT,
                VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT
            );
        }


        void Renderer::cullBarrier(const Command& cmd) {
            recordBarrier(
                cmd.vkCommandBuffer,
                VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT |
                    VK_PIPELINE_STAGE_VERTEX_SHADER_BIT |
                    VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT |
                    VK_PIPELINE_STAGE_COMMAND_PREPROCESS_BIT_EXT,
                VK_ACCESS_SHADER_WRITE_BIT,
                VK_ACCESS_INDIRECT_COMMAND_READ_BIT |
                    VK_ACCESS_SHADER_READ_BIT |
                    VK_ACCESS_COMMAND_PREPROCESS_READ_BIT_EXT
            );
        }

        void Renderer::recordBarrier(
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
    }
}
