#pragma once

#include "Global.hpp"
#include "FontAtlas.hpp"
#include "component/Text.hpp"

namespace Lu{
    namespace Core{

        // UTF-8 -> unicode codepoints. Invalid sequences become U+FFFD.
        inline std::vector<uint32_t> decodeUtf8(const std::string& string){
                std::vector<uint32_t> codepoints;
                codepoints.reserve(string.size());
                const size_t length = string.size();
                for(size_t i = 0; i < length;){
                    const uint8_t first = static_cast<uint8_t>(string[i]);
                    uint32_t codepoint = 0xFFFD;
                    size_t extra = 0;
                    if(first < 0x80){ codepoint = first; }
                    else if((first & 0xE0) == 0xC0){ codepoint = first & 0x1F; extra = 1; }
                    else if((first & 0xF0) == 0xE0){ codepoint = first & 0x0F; extra = 2; }
                    else if((first & 0xF8) == 0xF0){ codepoint = first & 0x07; extra = 3; }
                    else { codepoints.push_back(0xFFFD); ++i; continue; }

                    if(i + extra >= length){
                        // truncated sequence at the end of the string
                        codepoints.push_back(0xFFFD);
                        break;
                    }
                    bool valid = true;
                    for(size_t k = 1; k <= extra; ++k){
                        const uint8_t next = static_cast<uint8_t>(string[i + k]);
                        if((next & 0xC0) != 0x80){ valid = false; break; }
                        codepoint = (codepoint << 6) | (next & 0x3F);
                    }
                    if(!valid){ codepoints.push_back(0xFFFD); ++i; continue; }
                    codepoints.push_back(codepoint);
                    i += extra + 1;
                }
                return codepoints;
            }


        // Lays the codepoints out into one Font struct per codepoint (whitespace/newlines get valid = 0, so the n characters
        // of a text always map to exactly n structs). Origin = top-left of the first line, y points down, units are pixels.
        // Pure function (no Vulkan), the TextPackage writes the result into its range of the FontBuffer.
        //
        // Word wrapping: with maxWidth > 0 (pixels, same unit as `size`, before the Transform2D scale) a line is broken
        // at the last space/tab that fits; the space stays at the end of the previous line and does not count towards its width.
        // A word that is wider than maxWidth on its own is broken between two characters. A line always keeps at least
        // one character, so a single glyph that is wider than maxWidth overflows instead of looping forever.
        // maxWidth <= 0 turns wrapping off (only '\n' breaks lines).
        inline std::vector<Font> layoutText(const FontData& font, const std::vector<uint32_t>& codepoints,
                        const uint32_t transform2DId, const uint32_t pipeId, const glm::vec4& color,
                        const float size, const float lineSpacing, const TextAlign align,
                        const float maxWidth = 0.0f)
            {
                const uint32_t count = static_cast<uint32_t>(codepoints.size());
                std::vector<Font> result(count);
                if(count == 0) return result;

                const float scale = size / font.pixelSize;
                const float lineAdvance = font.lineHeight * lineSpacing * scale;

                // maxWidth in atlas pixels (the pen is measured at the rasterization size of the font)
                const float maxPen = maxWidth > 0.0f ? maxWidth / scale : 0.0f;

                // pass 1: line and pen x (in atlas pixels) of every glyph
                std::vector<float> penX(count, 0.0f);
                std::vector<uint32_t> lineOf(count, 0);
                const float spaceAdvance = font.glyph(' ').advance;
                constexpr uint32_t NO_BREAK = UINT32_MAX;

                float pen = 0.0f;
                uint32_t line = 0;
                uint32_t previous = 0;
                bool lineHasContent = false;      // a non-whitespace glyph was placed on the current line
                uint32_t breakIndex = NO_BREAK;   // first glyph after the last space of the current line = where a wrap may start

                uint32_t i = 0;
                while(i < count){
                    const uint32_t codepoint = codepoints[i];
                    lineOf[i] = line;

                    if(codepoint == '\n'){
                        penX[i] = pen;
                        ++line;
                        pen = 0.0f;
                        previous = 0;
                        lineHasContent = false;
                        breakIndex = NO_BREAK;
                        ++i;
                        continue;
                    }
                    if(codepoint == '\r'){
                        penX[i] = pen;
                        ++i;
                        continue;
                    }

                    const bool tab = codepoint == '\t';
                    const bool space = codepoint == ' ' || tab;
                    const float kern = tab ? 0.0f : font.kern(previous, codepoint);
                    const float start = pen + kern;
                    const float end = start + (tab ? 4.0f * spaceAdvance : font.glyph(codepoint).advance);

                    // A visible glyph that does not fit moves to a new line, together with the word it belongs to.
                    // (Spaces never wrap, they hang over the edge. A line without content cannot wrap: it would only be empty.)
                    if(maxPen > 0.0f && !space && lineHasContent && end > maxPen){
                        const uint32_t wrapStart = breakIndex != NO_BREAK ? breakIndex : i;
                        ++line;
                        pen = 0.0f;
                        previous = 0;
                        lineHasContent = false;
                        breakIndex = NO_BREAK;
                        i = wrapStart;      // place the word again, now at the start of the new line
                        continue;
                    }

                    penX[i] = start;
                    pen = end;
                    previous = tab ? 0 : codepoint;
                    if(space){
                        // spaces in front of the first visible glyph of a line are no break opportunity (the wrap would leave an empty line)
                        if(lineHasContent) breakIndex = i + 1;
                    } else {
                        lineHasContent = true;
                    }
                    ++i;
                }

                // width of every line = right edge of its last visible glyph (trailing spaces do not count),
                // this is what Center / Right alignment use
                std::vector<float> lineWidth(size_t{line} + 1, 0.0f);
                for(uint32_t g = 0; g < count; ++g){
                    const uint32_t codepoint = codepoints[g];
                    if(codepoint == '\n' || codepoint == '\r' || codepoint == '\t' || codepoint == ' ') continue;
                    float& width = lineWidth[lineOf[g]];
                    width = std::max(width, penX[g] + font.glyph(codepoint).advance);
                }

                // pass 2: the Font structs
                for(uint32_t i = 0; i < count; ++i){
                    const uint32_t codepoint = codepoints[i];
                    Font glyph{};
                    glyph.color = color;
                    glyph.textureId = font.textureId;
                    glyph.transform2DId = transform2DId;
                    glyph.pipelineId = pipeId;
                    glyph.glyphIndex = i;
                    glyph.glyphCount = count;
                    glyph.sdf = font.sdf ? 1.0f : 0.0f;

                    const bool control = codepoint == '\n' || codepoint == '\r' || codepoint == '\t';
                    const GlyphInfo& info = font.glyph(codepoint);
                    if(!control && info.size.x > 0.0f && info.size.y > 0.0f){
                        float alignOffset = 0.0f;
                        if(align == TextAlign::Center) alignOffset = -0.5f * lineWidth[lineOf[i]];
                        else if(align == TextAlign::Right) alignOffset = -lineWidth[lineOf[i]];

                        const float baseline = font.ascender * scale + static_cast<float>(lineOf[i]) * lineAdvance;
                        glyph.offset = glm::vec2(
                            (penX[i] + alignOffset + info.bearing.x) * scale,
                            baseline - info.bearing.y * scale);
                        glyph.size = info.size * scale;
                        glyph.uvRect = info.uv;
                        glyph.valid = 1;
                    }

                    result[i] = glyph;
                }
                return result;
            }

    }
}
