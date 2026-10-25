#pragma once
#include "Global.hpp"

namespace Lu{
    // Data of the 2D camera (see Component::Camera2D and EcsRequest::UpdateCamera2D).
    //
    // 2D space is measured in pixels, origin = top-left corner of the window, x to the right, y down.
    // Screen position of a world point p:
    //      screen = pivot * windowSize + zoom * Rotate(-rotation) * (p - position)
    // i.e. `position` is the world point that is shown at `pivot` (given as a fraction of the window).
    // The default camera (position 0, pivot 0, rotation 0, zoom 1) is the identity: world pixel == screen pixel.
    struct Camera2DData{
        glm::vec2 position{0.f, 0.f};   // world point shown at `pivot`
        glm::vec2 pivot{0.f, 0.f};      // (0,0) = top-left, (0.5,0.5) = window center
        float rotation{0.f};            // radians, around `pivot`; same sense as Transform2D::rotation
        float zoom{1.f};                // > 1 zooms in, around `pivot`

        bool operator==(const Camera2DData&) const = default;
    };
}
