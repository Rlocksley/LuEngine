#include "FontPackage.hpp"

#include <ft2build.h>
#include FT_FREETYPE_H

namespace Lu{
    namespace Core{

        FontPackage::FontPackage(const Camera2DPackage& camera2D, const Transform2DPackage& transforms2D, const TexturePackage& textures)
            : fontBuffer(MAX_FONT_GLYPHS, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT)
        {
            FT_Library library = nullptr;
            LU_ASSERT(FT_Init_FreeType(&library) == 0, "FontPackage", "FontPackage", "FT_Init_FreeType failed.")
            ftLibrary = library;

            // Zero the FontBuffer: valid == 0 everywhere, so slots that were never written draw nothing.
            {
                Command command;
                command.begin();
                for(uint32_t frameIndex = 0; frameIndex < MAX_FRAMES_IN_FLIGHT; ++frameIndex){
                    vkCmdFillBuffer(command.vkCommandBuffer, fontBuffer.getVkBuffer(frameIndex), 0, VK_WHOLE_SIZE, 0);
                }
                VkMemoryBarrier barrier{};
                barrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
                barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
                barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
                vkCmdPipelineBarrier(command.vkCommandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT,
                    VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_VERTEX_SHADER_BIT,
                    0, 1, &barrier, 0, nullptr, 0, nullptr);
                command.end();
                command.submit();
                command.waitForFence();
                command.resetFence();
            }

            fontDescrSet
            .addUniformBuffer(0, camera2D.getCameraBuffer(), VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT)
            .addStorageBuffer(1, transforms2D.getTransform2DBuffer(), VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT)
            .addStorageBuffer(2, fontBuffer, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT)
            .addTextureArray(3, textures.getTextureArray(), VK_SHADER_STAGE_FRAGMENT_BIT)
            .build();
        }

        FontPackage::~FontPackage(){
            if(ftLibrary != nullptr){
                FT_Done_FreeType(static_cast<FT_Library>(ftLibrary));
                ftLibrary = nullptr;
            }
        }

        FontPipeId FontPackage::createFontPipe(const FontPipeEntity entity, const GraphicsPipelineConfig& config){
            LU_ASSERT(pipeArraySize < MAX_FONT_PIPELINES, "FontPackage", "createFontPipe", "FontPipelineArray size overflow")
            LU_ASSERT(entityToPipeId.find(entity) == entityToPipeId.end(), "FontPackage", "createFontPipe", "Entity already is a font pipeline.")

            const FontPipeId pipeId = pipeArraySize++;
            entityToPipeId[entity] = pipeId;
            fontPipes[pipeId].create(fontDescrSet.getVkPipelineLayout(), false, config);
            return pipeId;
        }

        FontId FontPackage::createFont(const FontEntity entity, const TextureEntity atlasEntity,
                                       const FontConfig& config, TexturePackage& textures)
        {
            LU_ASSERT(entityToFontId.find(entity) == entityToFontId.end(), "FontPackage", "createFont", "Entity already is a font.")
            LU_ASSERT(fonts.size() < MAX_FONTS, "FontPackage", "createFont", "Font capacity exceeded.")
            LU_ASSERT(config.pixelSize > 0, "FontPackage", "createFont", "FontConfig::pixelSize must be bigger 0.")
            LU_ASSERT(!config.codepointRanges.empty(), "FontPackage", "createFont", "FontConfig::codepointRanges is empty.")

            FontAtlas atlas = buildFontAtlas(ftLibrary, config);
            const uint32_t atlasWidth = atlas.width;
            const uint32_t atlasHeight = atlas.height;
            FontData font = std::move(atlas.font);
            font.id = static_cast<FontId>(fonts.size());

            // atlas -> global texture array
            textures.createTexture(atlasEntity, TextureCpu(atlasWidth, atlasHeight, atlas.pixels));
            font.textureId = textures.getTextureId(atlasEntity);

            const FontId id = font.id;
            const size_t glyphCount = font.glyphs.size();
            fonts.push_back(std::move(font));
            entityToFontId[entity] = id;

#ifdef LU_DEBUG
            LU_LOGI("FontPackage", "createFont", config.name + ": " + std::to_string(glyphCount) + " glyphs, atlas " +
                std::to_string(atlasWidth) + "x" + std::to_string(atlasHeight) + (config.sdf ? " (sdf)" : ""))
#endif
            return id;
        }
    }
}
