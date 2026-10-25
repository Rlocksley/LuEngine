#pragma once

#include "Global.hpp"
#include "Constant.hpp"
#include "Buffer.hpp"
#include "Command.hpp"
#include "ComputePipeline.hpp"
#include "DescriptorSet.hpp"
#include "DirtyTracker.hpp"
#include "flecs.h"
#include "instance/FontInstance.hpp"
#include "component/Text.hpp"
#include "Camera2DPackage.hpp"
#include "Transform2DPackage.hpp"
#include "FontPackage.hpp"
#include "TextLayout.hpp"

namespace Lu{
    namespace Core{

        // Text entities -> Font structs in the FontBuffer (the 2D counterpart of MeshPackage).
        //
        //   CreateText(n characters) allocates a range of n Font structs in the FontBuffer
        //   (if the entity already has a text with a different n the old range is freed first)
        //   -> the structs are written to the CPU mirror and marked dirty
        //   -> FontInstanceBuffer (elements (Font, id)) --font_transfer.comp--> FontBuffer
        //   -> font_cull.comp writes ONE VkDrawIndirectCommand PER GLYPH into the indirect buffer of its pipeline
        //   -> vkCmdDrawIndirectCount per font pipeline
        class TextPackage{
        private:
            using TextEntity = flecs::entity_t;
            using Range = RangeBuffer<Font>::Range;

            FontPackage& fonts;

            DualBuffer<FontInstance> instanceBuffer;
            std::array<std::shared_ptr<IBufferResource>, MAX_FONT_PIPELINES> indirectBufferArray;
            BufferGpu<uint32_t> drawCountBuffer;
            BufferGpuIndexed<uint32_t> capacityBuffer;
            std::array<uint32_t, MAX_FONT_PIPELINES> indirectDrawCapacity{};

            DescriptorSet transferDescrSet;
            ComputePipeline transferPipe;
            DescriptorSet cullDescrSet;
            ComputePipeline cullPipe;

            std::unordered_map<TextEntity, Range> entityToRange;
            std::vector<Font> glyphs;                                  // CPU mirror of the FontBuffer, indexed by GlyphId
            DirtyTracker<GlyphId> dirty{MAX_FRAMES_IN_FLIGHT};

            Command command;
            uint32_t instanceBufferSize{0};

        public:
            TextPackage(const Camera2DPackage& camera2D, const Transform2DPackage& transforms2D, FontPackage& fontPackage)
                : fonts(fontPackage),
                instanceBuffer(MAX_FONT_GLYPHS, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT),
                indirectBufferArray(initIndirectBufferArray()),
                drawCountBuffer(MAX_FONT_PIPELINES, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT),
                capacityBuffer(MAX_FONT_PIPELINES, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT)
            {
                createTransferPipe();
                createCullPipe(camera2D, transforms2D);
            }

            // ---------------------------------------------------------------- pipes
            void createTransferPipe(){
                transferDescrSet
                .addStorageBuffer(0, instanceBuffer, VK_SHADER_STAGE_COMPUTE_BIT)
                .addStorageBuffer(1, fonts.getFontBuffer(), VK_SHADER_STAGE_COMPUTE_BIT)
                .addPushConstantRange(VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(uint32_t))
                .build();

                transferPipe.create(
                    transferDescrSet.getVkPipelineLayout(),
                    ComputePipelineConfig{
                        .name = "font_transfer",
                        .computeShader = "shader/font_transfer.comp.spv"
                    }
                );
            }

            void createCullPipe(const Camera2DPackage& camera2D, const Transform2DPackage& transforms2D){
                cullDescrSet
                .addUniformBuffer(0, camera2D.getCameraBuffer(), VK_SHADER_STAGE_COMPUTE_BIT)
                .addStorageBuffer(1, transforms2D.getTransform2DBuffer(), VK_SHADER_STAGE_COMPUTE_BIT)
                .addStorageBuffer(2, fonts.getFontBuffer(), VK_SHADER_STAGE_COMPUTE_BIT)
                .addStorageBuffer(3, drawCountBuffer, VK_SHADER_STAGE_COMPUTE_BIT)
                .addStorageBuffer(4, capacityBuffer, VK_SHADER_STAGE_COMPUTE_BIT)
                .addStorageBufferArray(5, indirectBufferArray, VK_SHADER_STAGE_COMPUTE_BIT)
                .addPushConstantRange(VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(uint32_t))
                .build();

                cullPipe.create(
                    cullDescrSet.getVkPipelineLayout(),
                    ComputePipelineConfig{
                        .name = "font_cull",
                        .computeShader = "shader/font_cull.comp.spv"
                    }
                );
            }

            // Called by the Renderer right after FontPackage::createFontPipe: creates the indirect buffer of the pipeline.
            void createIndirectBuffer(const FontPipeId pipeId, const uint32_t capacity){
                LU_ASSERT(pipeId < MAX_FONT_PIPELINES, "TextPackage", "createIndirectBuffer", "FontPipeId out of range")
                LU_ASSERT(capacity > 0, "TextPackage", "createIndirectBuffer", "GraphicsPipelineConfig::capacity must be bigger 0")
                indirectBufferArray[pipeId] = std::make_shared<BufferGpu<VkDrawIndirectCommand>>(
                    capacity,
                    VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                    VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT |
                    VK_BUFFER_USAGE_TRANSFER_DST_BIT
                );
                indirectDrawCapacity[pipeId] = capacity;

                cullDescrSet.updateDescriptorSetWrites();

                command.begin();
                capacityBuffer.upload(pipeId, capacity, command);
                command.end();
                command.submit();
                command.waitForFence();
                command.resetFence();
            }

            // ---------------------------------------------------------------- requests
            // EcsRequest::CreateText (upsert)
            void createText(const TextEntity entity, const Transform2DId transform2DId,
                            const FontId fontId, const FontPipeId pipeId,
                            const std::string& string, const glm::vec4& color,
                            const float size, const float lineSpacing, const TextAlign align,
                            const float maxWidth)
            {
                LU_ASSERT(pipeId < fonts.getPipeCount(), "TextPackage", "createText", "Invalid font pipeline.")

                std::vector<uint32_t> codepoints = decodeUtf8(string);
                if(codepoints.size() > MAX_GLYPHS_PER_TEXT){
                    LU_LOGI("TextPackage", "createText", "Text is longer than MAX_GLYPHS_PER_TEXT and gets truncated.")
                    codepoints.resize(MAX_GLYPHS_PER_TEXT);
                }
                const uint32_t count = static_cast<uint32_t>(codepoints.size());

                Range range{0, 0};
                const auto it = entityToRange.find(entity);
                if(it != entityToRange.end()){
                    range = it->second;
                    if(range.size != count){
                        // size changed: free the old n Font structs, allocate new ones
                        releaseRange(range);
                        range = Range{0, 0};
                    }
                }
                if(range.size == 0 && count > 0){
                    range = fonts.allocateGlyphs(count);
                    if(glyphs.size() < size_t{range.offset} + range.size){
                        glyphs.resize(size_t{range.offset} + range.size);
                    }
                }
                entityToRange[entity] = range;

                const std::vector<Font> laidOut = layoutText(
                    fonts.getFont(fontId), codepoints, transform2DId, pipeId, color, size, lineSpacing, align, maxWidth);
                for(uint32_t i = 0; i < count; ++i){
                    glyphs[range.offset + i] = laidOut[i];
                    dirty.mark(range.offset + i);
                }
            }

            // EcsRequest::DestroyText
            void destroyText(const TextEntity entity){
                const auto it = entityToRange.find(entity);
                if(it == entityToRange.end()) return;
                if(it->second.size > 0) releaseRange(it->second);
                entityToRange.erase(it);
            }

            // ---------------------------------------------------------------- per frame
            void collectDirty(const uint32_t frameIndex){
                dirty.collect([&](GlyphId id){ instanceBuffer.push_back(frameIndex, FontInstance{glyphs[id], id}); });
            }

            void copy(const uint32_t frameIndex, const Command& cmd){
                zeroOut(frameIndex, cmd);

                if((instanceBufferSize = instanceBuffer.sizeCpu(frameIndex)) == 0){
                    return;
                }
                instanceBuffer.copyToGpu(frameIndex, cmd);
            }

            void transfer(const uint32_t frameIndex, const Command& cmd) const {
                if(instanceBufferSize == 0){
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

            // font_cull.comp: one thread per Font struct; visible glyphs append a draw command to the indirect buffer of their pipeline
            void cull(const uint32_t frameIndex, const Command& cmd) const {
                const uint32_t glyphCount = fonts.getFontBuffer().size();
                if(glyphCount == 0 || fonts.getPipeCount() == 0){
                    return;
                }

                vkCmdBindPipeline(
                    cmd.vkCommandBuffer, 
                    VK_PIPELINE_BIND_POINT_COMPUTE, 
                    cullPipe.getVkPipeline()
                );
                
                vkCmdPushConstants(
                    cmd.vkCommandBuffer, 
                    cullDescrSet.getVkPipelineLayout(),
                    VK_SHADER_STAGE_COMPUTE_BIT, 
                    0, 
                    sizeof(uint32_t), 
                    &glyphCount
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
                    (glyphCount + 256 - 1) / 256, 
                    1, 
                    1
                );
            }

            // true if any glyph slot is in use (the Renderer clears the depth image only then)
            bool hasGlyphs() const { return fonts.getFontBuffer().size() > 0 && fonts.getPipeCount() > 0; }

            // Must be recorded inside dynamic rendering, after the depth image was cleared.
            void draw(const uint32_t frameIndex, const Command& cmd) const {
                if(!hasGlyphs()){
                    return;
                }

                vkCmdBindDescriptorSets(
                    cmd.vkCommandBuffer, 
                    VK_PIPELINE_BIND_POINT_GRAPHICS,
                    fonts.getDescriptorSet().getVkPipelineLayout(), 
                    0, 
                    1,
                    &fonts.getDescriptorSet().getVkDescriptorSet(frameIndex), 
                    0, 
                    nullptr
                );

                for(FontPipeId pipeId = 0; pipeId < fonts.getPipeCount(); ++pipeId){
                    vkCmdBindPipeline(
                        cmd.vkCommandBuffer, 
                        VK_PIPELINE_BIND_POINT_GRAPHICS,
                        fonts.getPipeline(pipeId).getVkPipeline()
                    );

                    vkCmdDrawIndirectCount(
                        cmd.vkCommandBuffer,
                        indirectBufferArray[pipeId]->getVkBuffer(frameIndex),
                        0,
                        drawCountBuffer.getVkBuffer(frameIndex),
                        static_cast<VkDeviceSize>(pipeId) * sizeof(uint32_t),
                        indirectDrawCapacity[pipeId],
                        sizeof(VkDrawIndirectCommand)
                    );
                }
            }

        private:
            // free a range and make every slot of it invalid (written to the GPU like any other dirty slot)
            void releaseRange(const Range range){
                for(uint32_t i = 0; i < range.size; ++i){
                    glyphs[range.offset + i] = Font{};
                    dirty.mark(range.offset + i);
                }
                fonts.freeGlyphs(range);
            }

            static std::shared_ptr<IBufferResource> initDummyIndirectBuffer(){
                return std::make_shared<BufferGpu<VkDrawIndirectCommand>>(
                    1,
                    VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                    VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT |
                    VK_BUFFER_USAGE_TRANSFER_DST_BIT
                );
            }

            static std::array<std::shared_ptr<IBufferResource>, MAX_FONT_PIPELINES> initIndirectBufferArray(){
                std::array<std::shared_ptr<IBufferResource>, MAX_FONT_PIPELINES> buffers;
                buffers.fill(initDummyIndirectBuffer());
                return buffers;
            }

            void zeroOut(const uint32_t frameIndex, const Command& cmd){
                vkCmdFillBuffer(
                    cmd.vkCommandBuffer, 
                    drawCountBuffer.getVkBuffer(frameIndex), 
                    0, 
                    VK_WHOLE_SIZE, 
                    0
                );
            }
        };
    }
}
