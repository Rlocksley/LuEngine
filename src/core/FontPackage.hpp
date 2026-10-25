#pragma once

#include "Global.hpp"
#include "Constant.hpp"
#include "Buffer.hpp"
#include "Command.hpp"
#include "DescriptorSet.hpp"
#include "GraphicsPipeline.hpp"
#include "PipelineConfig.hpp"
#include "Vertex.hpp"
#include "flecs.h"
#include "instance/FontInstance.hpp"
#include "Camera2DPackage.hpp"
#include "Transform2DPackage.hpp"
#include "TexturePackage.hpp"
#include "FontAtlas.hpp"

// FreeType is only used inside FontPackage.cpp; forward declare its library handle.
struct FT_LibraryRec_;

namespace Lu{
    namespace Core{

        using FontPipeId = uint32_t;

        // Registry of fonts + everything that is needed to draw glyphs:
        //  - builds a FontAtlas with FreeType and uploads it as a texture of the global TexturePackage array
        //  - entityToFontId: registered font entity -> FontId
        //  - FontBuffer: range-allocated GPU buffer (replicated per frame in flight) with one Font struct per glyph;
        //    each element stores the TextureId of its atlas and everything else the font shaders need
        //  - the font graphics pipelines (App::registerFontPipe) and the descriptor set they share
        class FontPackage{
        private:
            using FontEntity = flecs::entity_t;
            using TextureEntity = flecs::entity_t;
            using FontPipeEntity = flecs::entity_t;

            FT_LibraryRec_* ftLibrary{nullptr};

            RangeBuffer<Font> fontBuffer;
            std::vector<FontData> fonts;
            std::unordered_map<FontEntity, FontId> entityToFontId;

            DescriptorSet fontDescrSet;
            uint32_t pipeArraySize{0};
            std::array<GraphicsPipeline<Vertex::Glyph>, MAX_FONT_PIPELINES> fontPipes{};
            std::unordered_map<FontPipeEntity, FontPipeId> entityToPipeId;

        public:
            FontPackage(const Camera2DPackage& camera2D, const Transform2DPackage& transforms2D, const TexturePackage& textures);
            ~FontPackage();

            FontPackage(const FontPackage&) = delete;
            FontPackage& operator=(const FontPackage&) = delete;

            // Rasterizes the font with FreeType, packs the atlas and registers it as texture `atlasEntity`.
            FontId createFont(const FontEntity entity, const TextureEntity atlasEntity,
                              const FontConfig& config, TexturePackage& textures);

            // Registers a font pipeline (shaders read the FontBuffer, Camera2D, Transform2D and the texture array).
            FontPipeId createFontPipe(const FontPipeEntity entity, const GraphicsPipelineConfig& config);

            FontId getFontId(const FontEntity entity) const {
                const auto it = entityToFontId.find(entity);
                LU_ASSERT(it != entityToFontId.end(), "FontPackage", "getFontId", "No FontId for given FontEntity. Was it registered with App::registerFont?");
                return it->second;
            }

            FontPipeId getFontPipeId(const FontPipeEntity entity) const {
                const auto it = entityToPipeId.find(entity);
                LU_ASSERT(it != entityToPipeId.end(), "FontPackage", "getFontPipeId", "No FontPipeId for given entity. Was it registered with App::registerFontPipe?");
                return it->second;
            }

            const FontData& getFont(const FontId id) const { return fonts[id]; }

            // ---- FontBuffer ranges ----
            RangeBuffer<Font>::Range allocateGlyphs(const uint32_t count){ return fontBuffer.allocate(count); }
            void freeGlyphs(const RangeBuffer<Font>::Range range){ fontBuffer.free(range); }
            const RangeBuffer<Font>& getFontBuffer() const { return fontBuffer; }

            uint32_t getPipeCount() const { return pipeArraySize; }
            const GraphicsPipeline<Vertex::Glyph>& getPipeline(const FontPipeId id) const { return fontPipes[id]; }
            const DescriptorSet& getDescriptorSet() const { return fontDescrSet; }

            void updateDescriptorSetWrites(){ fontDescrSet.updateDescriptorSetWrites(); }
        };
    }
}
