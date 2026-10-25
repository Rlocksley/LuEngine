#pragma once

#include "Global.hpp"
#include "flecs.h"
#include "component/Transform2D.hpp"

namespace Lu{

    enum class TextAlign : uint32_t{
        Left = 0,     // the Transform2D position is the left edge of every line
        Center = 1,   // the Transform2D position is the horizontal center of every line
        Right = 2     // the Transform2D position is the right edge of every line
    };

    namespace Component{

        // A text made of glyph quads. Put it on a CHILD of an entity that has a Component::Transform2D.
        // The text block starts at the Transform2D position: x = line start (see TextAlign),
        // y = top of the first line. Every character occupies one Font struct in the FontBuffer
        // of the renderer; changing the string simply sends a new EcsRequest::CreateText.
        struct Text{
            std::string text;               // UTF-8, '\n' starts a new line
            flecs::entity font;             // registered with App::registerFont(...)
            flecs::entity pipeline;         // registered with App::registerFontPipe(...)
            glm::vec4 color{1.f, 1.f, 1.f, 1.f};
            float size{32.f};               // height of one line of text in pixels
            float lineSpacing{1.f};         // multiplier of the font's line height
            TextAlign align{TextAlign::Left};
        };

        struct TextGpuDirty{};
    }
}
