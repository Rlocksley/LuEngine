#include "FontAtlas.hpp"

#include <ft2build.h>
#include FT_FREETYPE_H

namespace Lu{
    namespace Core{

        namespace{
            struct RasterizedGlyph{
                uint32_t codepoint{0};
                uint32_t width{0};
                uint32_t height{0};
                float left{0.0f};
                float top{0.0f};
                float advance{0.0f};
                std::vector<uint8_t> pixels;     // width * height, 8 bit
                uint32_t x{0};                   // position in the atlas
                uint32_t y{0};
            };

            // Shelf packing of glyphs (sorted by height) into an atlas that is `width` wide.
            // Returns the needed atlas height, positions are written into the glyphs (indices in `order`).
            uint32_t packShelves(std::vector<RasterizedGlyph>& glyphs, const std::vector<size_t>& order,
                                 const uint32_t width, const uint32_t gap)
            {
                uint32_t x = gap, y = gap, shelfHeight = 0;
                for(const size_t index : order){
                    RasterizedGlyph& glyph = glyphs[index];
                    if(glyph.width == 0 || glyph.height == 0) continue;
                    if(glyph.width + 2 * gap > width) return UINT32_MAX;      // does not fit at all
                    if(x + glyph.width + gap > width){                        // next shelf
                        x = gap;
                        y += shelfHeight + gap;
                        shelfHeight = 0;
                    }
                    glyph.x = x;
                    glyph.y = y;
                    x += glyph.width + gap;
                    shelfHeight = std::max(shelfHeight, glyph.height);
                }
                return y + shelfHeight + gap;
            }
        }

        FontAtlas buildFontAtlas(FT_LibraryRec_* ftLibrary, const FontConfig& config)
        {
            LU_ASSERT(config.pixelSize > 0, "FontAtlas", "buildFontAtlas", "FontConfig::pixelSize must be bigger 0.")
            LU_ASSERT(!config.codepointRanges.empty(), "FontAtlas", "buildFontAtlas", "FontConfig::codepointRanges is empty.")

            FT_Library library = static_cast<FT_Library>(ftLibrary);
            FT_Face face = nullptr;
            LU_ASSERT(FT_New_Face(library, config.filePath.c_str(), 0, &face) == 0,
                "FontAtlas", "buildFontAtlas", "Failed to load font file: " + config.filePath)
            LU_ASSERT(FT_Set_Pixel_Sizes(face, 0, config.pixelSize) == 0,
                "FontAtlas", "buildFontAtlas", "FT_Set_Pixel_Sizes failed for: " + config.filePath)

            FT_Render_Mode renderMode = FT_RENDER_MODE_NORMAL;
            if(config.sdf){
#if (FREETYPE_MAJOR > 2) || (FREETYPE_MAJOR == 2 && FREETYPE_MINOR >= 11)
                renderMode = FT_RENDER_MODE_SDF;
#else
                LU_LOGE("FontAtlas", "buildFontAtlas", "FontConfig::sdf needs FreeType >= 2.11, set sdf = false.")
#endif
            }

            FontData font;
            font.pixelSize = static_cast<float>(config.pixelSize);
            font.ascender = static_cast<float>(face->size->metrics.ascender) / 64.0f;
            font.descender = static_cast<float>(face->size->metrics.descender) / 64.0f;
            font.lineHeight = static_cast<float>(face->size->metrics.height) / 64.0f;
            if(font.lineHeight <= 0.0f) font.lineHeight = font.ascender - font.descender;
            font.sdf = config.sdf;

            // 1. rasterize all glyphs
            std::vector<RasterizedGlyph> glyphs;
            std::vector<std::pair<uint32_t, FT_UInt>> glyphIndices;   // codepoint, FreeType glyph index
            for(const auto& range : config.codepointRanges){
                for(uint32_t codepoint = range.first; codepoint <= range.second; ++codepoint){
                    const FT_UInt glyphIndex = FT_Get_Char_Index(face, codepoint);
                    if(glyphIndex == 0) continue;                       // font has no such glyph
                    if(FT_Load_Glyph(face, glyphIndex, FT_LOAD_DEFAULT | FT_LOAD_NO_BITMAP) != 0) continue;
                    if(FT_Render_Glyph(face->glyph, renderMode) != 0) continue;

                    const FT_Bitmap& bitmap = face->glyph->bitmap;
                    RasterizedGlyph glyph;
                    glyph.codepoint = codepoint;
                    glyph.width = bitmap.width;
                    glyph.height = bitmap.rows;
                    glyph.left = static_cast<float>(face->glyph->bitmap_left);
                    glyph.top = static_cast<float>(face->glyph->bitmap_top);
                    glyph.advance = static_cast<float>(face->glyph->advance.x) / 64.0f;
                    if(glyph.width > 0 && glyph.height > 0 && bitmap.buffer != nullptr &&
                       bitmap.pixel_mode == FT_PIXEL_MODE_GRAY){
                        glyph.pixels.resize(size_t{glyph.width} * glyph.height);
                        for(uint32_t row = 0; row < glyph.height; ++row){
                            std::memcpy(glyph.pixels.data() + size_t{row} * glyph.width,
                                        bitmap.buffer + static_cast<ptrdiff_t>(row) * bitmap.pitch, glyph.width);
                        }
                    } else {
                        glyph.width = glyph.height = 0;                 // whitespace or unsupported bitmap
                    }
                    glyphs.push_back(std::move(glyph));
                    glyphIndices.emplace_back(codepoint, glyphIndex);
                }
            }
            LU_ASSERT(!glyphs.empty(), "FontAtlas", "buildFontAtlas", "No glyph could be rasterized from: " + config.filePath)

            // 2. kerning (legacy 'kern' table only, FreeType has no GPOS support)
            if(FT_HAS_KERNING(face)){
                for(const auto& left : glyphIndices){
                    for(const auto& right : glyphIndices){
                        FT_Vector delta{};
                        if(FT_Get_Kerning(face, left.second, right.second, FT_KERNING_UNFITTED, &delta) == 0 && delta.x != 0){
                            font.kerning[(uint64_t{left.first} << 32) | right.first] = static_cast<float>(delta.x) / 64.0f;
                        }
                    }
                }
            }
            FT_Done_Face(face);

            // 3. pack: tallest glyphs first, smallest power of two width whose height is not bigger than the width
            std::vector<size_t> order(glyphs.size());
            for(size_t i = 0; i < order.size(); ++i) order[i] = i;
            std::stable_sort(order.begin(), order.end(), [&](size_t a, size_t b){ return glyphs[a].height > glyphs[b].height; });

            const uint32_t gap = 2;     // keeps linear filtering of neighbours out of a glyph
            uint32_t atlasWidth = 0, atlasHeight = 0;
            for(uint32_t width = 256; width <= 4096; width *= 2){
                const uint32_t height = packShelves(glyphs, order, width, gap);
                if(height != UINT32_MAX && (height <= width || width == 4096)){
                    atlasWidth = width;
                    atlasHeight = height;
                    break;
                }
            }
            LU_ASSERT(atlasWidth != 0 && atlasHeight <= 4096, "FontAtlas", "buildFontAtlas",
                "Font atlas does not fit into 4096x4096. Reduce FontConfig::pixelSize or codepointRanges.")

            // 4. atlas pixels: RGB = 255, A = coverage / distance (the texture format is R8G8B8A8_SRGB, alpha is linear)
            std::shared_ptr<uint8_t[]> pixels(new uint8_t[size_t{atlasWidth} * atlasHeight * 4]);
            for(size_t i = 0; i < size_t{atlasWidth} * atlasHeight; ++i){
                pixels[i * 4 + 0] = 255;
                pixels[i * 4 + 1] = 255;
                pixels[i * 4 + 2] = 255;
                pixels[i * 4 + 3] = 0;
            }

            const float invWidth = 1.0f / static_cast<float>(atlasWidth);
            const float invHeight = 1.0f / static_cast<float>(atlasHeight);
            for(const RasterizedGlyph& glyph : glyphs){
                for(uint32_t row = 0; row < glyph.height; ++row){
                    for(uint32_t column = 0; column < glyph.width; ++column){
                        pixels[(size_t{glyph.y + row} * atlasWidth + glyph.x + column) * 4 + 3] =
                            glyph.pixels[size_t{row} * glyph.width + column];
                    }
                }

                GlyphInfo info;
                info.size = glm::vec2(static_cast<float>(glyph.width), static_cast<float>(glyph.height));
                info.bearing = glm::vec2(glyph.left, glyph.top);
                info.advance = glyph.advance;
                if(glyph.width > 0){
                    info.uv = glm::vec4(
                        static_cast<float>(glyph.x) * invWidth,
                        static_cast<float>(glyph.y) * invHeight,
                        static_cast<float>(glyph.x + glyph.width) * invWidth,
                        static_cast<float>(glyph.y + glyph.height) * invHeight);
                }
                font.glyphs[glyph.codepoint] = info;
            }

            // fallback glyph for codepoints the font does not have
            font.fallbackCodepoint = font.glyphs.count('?') ? '?' : font.glyphs.begin()->first;

            FontAtlas atlas;
            atlas.width = atlasWidth;
            atlas.height = atlasHeight;
            atlas.pixels = std::move(pixels);
            atlas.font = std::move(font);
            return atlas;
        }
    }
}
