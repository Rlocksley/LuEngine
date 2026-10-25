#pragma once

#include "Global.hpp"

namespace Lu{
    namespace Core{

        using FontId = uint32_t;     // index of a registered font (FontPackage::entityToFontId)
        using GlyphId = uint32_t;    // index of a Font struct inside the FontBuffer

        // One glyph of a Text = one element of the FontBuffer = one indirect draw command.
        // A Text with n characters owns a contiguous range of n of these structs.
        // std430, 80 bytes, mirrored by `struct Font` in font_transfer.comp / font_cull.comp / font_basic.vert/frag.
        struct Font{
            glm::vec4 uvRect{0.0f};          // u0, v0, u1, v1 inside the atlas
            glm::vec4 color{1.0f};
            glm::vec2 offset{0.0f};          // top-left of the quad relative to the text origin (pixels, already scaled)
            glm::vec2 size{0.0f};            // size of the quad (pixels, already scaled)
            uint32_t textureId{0};           // TextureId of the font atlas in the global texture array
            uint32_t transform2DId{0};       // slot of the parent's Transform2D
            uint32_t pipelineId{0};          // font pipeline (selects the indirect buffer in font_cull.comp)
            uint32_t valid{0};               // 0 = free slot / whitespace: not drawn
            uint32_t glyphIndex{0};          // index of this glyph in its text (for per-glyph effects in shaders)
            uint32_t glyphCount{0};          // number of glyphs of the text
            float sdf{0.0f};                 // 1 = atlas holds a signed distance field, 0 = coverage
            uint32_t _pad0{0};
        };

        struct FontInstance{
            Font font{};
            GlyphId id{0};
            uint32_t _pad[3]{0, 0, 0};

            FontInstance() = default;
            FontInstance(const Font& font, const GlyphId id) : font(font), id(id) {}
        };

        static_assert(sizeof(Font) == 80);
        static_assert(offsetof(Font, textureId) == 48);
        static_assert(offsetof(FontInstance, id) == 80);
        static_assert(sizeof(FontInstance) == 96);
    }
}
