#include "Renderer.hpp"
#include "Core.hpp"
#include "Constant.hpp"
#include "Channel.hpp"
#include "Swapchain.hpp"
#include "Framebuffers.hpp"
#include "Exit.hpp"

namespace Lu{
    namespace Core{
        Renderer::Renderer()
            : command{},
            camera(),
            transform(),
            meshGeometry(
                MAX_MESH_VERTEX_BUFFER_SIZE,
                MAX_MESH_INDEX_BUFFER_SIZE,
                MAX_VERTICES_PER_MESH,
                MAX_INDICES_PER_MESH,
                MAX_MESH_INFOS
            ),
            mesh(camera, transform, meshGeometry),
            multiMesh(camera, transform, meshGeometry) {
        }

        Renderer::~Renderer() {
            // run() may leave up to MAX_FRAMES_IN_FLIGHT submissions pending.
            LU_CHECK_VULKAN(vkDeviceWaitIdle(vkDevice), "Renderer::~Renderer", "vkDeviceWaitIdle")
        }

        void Renderer::run(FramerateMonitor& framerateMonitor) {
            std::chrono::duration<double> targetFrameTime(1.0 / MAX_FRAMES_PER_SECOND_RENDERER);

            while (Core::isRunning.load(std::memory_order_relaxed) && Core::updateCore()) {
                auto frameStart = std::chrono::high_resolution_clock::now();

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

                // Sleep for the remaining frame budget to cap at MAX_FRAMES_PER_SECOND_RENDERER.
                auto frameEnd = std::chrono::high_resolution_clock::now();
                auto elapsed  = frameEnd - frameStart;
                if (elapsed < targetFrameTime) {
                    std::this_thread::sleep_for(targetFrameTime - elapsed);
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

        void Renderer::createMultiMeshPipe(const flecs::entity_t entity, const GraphicsPipelineConfig& config){
            multiMesh.createGraphicsPipeline(entity, config);
        }

        void Renderer::createMultiMeshComputePipe(const flecs::entity_t entity, const ComputePipelineConfig& config){
            multiMesh.createComputePipeline(entity, config);
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
                        
                        [&](const EcsRequest::CreateMesh& req){
                            mesh.createMesh(
                                req.entity, req.pipe,
                                transform.getTransformId(req.parent),
                                meshGeometry.getGeometryInfoId(req.mesh),
                                req.material
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
                    request.mesh,
                    request.computePipe,
                    request.pipe,
                    transform.getTransformId(request.parent),
                    meshGeometry,
                    request.instances,
                    request.cullSphere
                );
            }
            
            // Apply the latest dynamic transforms after lifecycle requests have
            // established or removed entity-to-transform ID mappings.
            transform.updateTransforms();

            //Writes the Dirty Elements to the InstanceBuffer
            transform.collectDirty(frameIndex);
            mesh.collectDirty(frameIndex);
            multiMesh.collectDirty(frameIndex);
        }

        void Renderer::record(){
            
            const auto& cmd = command[frameIndex];

            //begin command buffer recording
            cmd.begin();

            
            //Copy To GPU
            camera.copy(frameIndex, cmd);
            transform.copy(frameIndex, cmd);
            mesh.copy(frameIndex, cmd);
            multiMesh.copy(frameIndex, cmd);
            mesh.zeroOut(frameIndex, cmd);

            //Pipeline Barrier
            copyBarrier(cmd);

            //Transfer 
            //InstanceBuffer Elements (Type,Id) 
            //to Buffer Elements (Type) 
            //via compute shader
            transform.transfer(frameIndex, cmd);
            mesh.transfer(frameIndex, cmd);
            multiMesh.transfer(frameIndex, cmd);

            //Pipeline Barrier
            transferBarrier(cmd);

            //Mesh Culling
            mesh.cull(frameIndex, cmd);
            multiMesh.cull(frameIndex, cmd);

            //Execute generated commands after culling has written their indirect records.
            cullBarrier(cmd);
            multiMesh.executeCompute(frameIndex, cmd);

            //Pipeline Barrier
            cullBarrier(cmd);

            //begin dynamic rendering
            beginRendering(cmd);

            //Indirect Draws of all MeshPipelines
            mesh.draw(frameIndex, meshGeometry, cmd);
            multiMesh.draw(frameIndex, meshGeometry, cmd);

            //end dynamic rendering
            endRendering(cmd);

            //end command buffer recording
            cmd.end();
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
                VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_VERTEX_SHADER_BIT,
                VK_ACCESS_TRANSFER_WRITE_BIT,
                VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT
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
