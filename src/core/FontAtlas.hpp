#pragma once

#include "Global.hpp"
#include "PipelineConfig.hpp"
#include "instance/FontInstance.hpp"

// FreeType is only used inside FontAtlas.cpp; forward declare its library handle.
struct FT_LibraryRec_;

namespace Lu{
    namespace Core{

        // Metrics of one glyph, in pixels at the rasterization size of the font (FontConfig::pixelSize).
        struct GlyphInfo{
            glm::vec4 uv{0.0f};          // u0, v0, u1, v1 in the atlas
            glm::vec2 size{0.0f};        // size of the atlas bitmap (0 for whitespace)
            glm::vec2 bearing{0.0f};     // x: bitmap left of the pen, y: bitmap top above the baseline
            float advance{0.0f};
        };

        // CPU side of a registered font (needed to lay out strings).
        struct FontData{
            FontId id{0};
            uint32_t textureId{0};       // atlas in the global texture array of the TexturePackage
            float pixelSize{0.0f};
            float ascender{0.0f};        // baseline distance from the top of a line
            float descender{0.0f};       // negative
            float lineHeight{0.0f};
            bool sdf{false};
            uint32_t fallbackCodepoint{0};
            std::unordered_map<uint32_t, GlyphInfo> glyphs;
            std::unordered_map<uint64_t, float> kerning;

            // glyph of the codepoint, or of the fallback ('?') if the font has none
            const GlyphInfo& glyph(const uint32_t codepoint) const {
                const auto it = glyphs.find(codepoint);
                return it != glyphs.end() ? it->second : glyphs.at(fallbackCodepoint);
            }

            float kern(const uint32_t left, const uint32_t right) const {
                if(kerning.empty()) return 0.0f;
                const auto it = kerning.find((uint64_t{left} << 32) | right);
                return it != kerning.end() ? it->second : 0.0f;
            }
        };

        // Result of rasterizing a font: the atlas bitmap (RGBA, RGB = 255, A = coverage or distance)
        // plus the metrics needed to lay out text.
        struct FontAtlas{
            uint32_t width{0};
            uint32_t height{0};
            std::shared_ptr<uint8_t[]> pixels;
            FontData font;        // textureId and id are filled in by the FontPackage
        };

        // Rasterizes all glyphs of `config` with FreeType and packs them into an atlas. Does not touch Vulkan.
        FontAtlas buildFontAtlas(FT_LibraryRec_* ftLibrary, const FontConfig& config);
    }
}
