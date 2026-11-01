#pragma once

#include "UICore.hpp"

namespace Lu {
    namespace UI {

        // Scroll bar: a rounded track with a rounded thumb. The thumb keeps its size (thumbRatio of the track, e.g. visible
        // part of the content / content), only its position moves: position 0 = thumb at the start (top / left),
        // position 1 = thumb all the way at the end. Drag the thumb, or click / drag on the track. The ScrollView creates one
        // as child; it can be used on its own (lists, text areas, a volume bar ...). Node::size default: 14 x 200
        // (horizontal: 200 x 14).
        struct ScrollBar {
            float position{0.0f};                // 0..1
            float thumbRatio{0.25f};             // 0..1, length of the thumb / length of the track
            Orientation orientation{Orientation::Vertical};
            float minThumbLength{28.0f};         // pixels
            float padding{2.0f};                 // between track edge and thumb
            float radius{6.0f};                  // corner radius of the thumb (the track is rounded accordingly)
            Color trackColor{kUnset};            // Style::track
            Color thumbColor{kUnset};            // Style::knob (accent while hovered / dragged)
            std::function<void(flecs::entity, float)> onChange{};

            // state, written by the module
            float grab{0.0f};                    // distance between the cursor and the start of the thumb while dragging
        };
    }
}
