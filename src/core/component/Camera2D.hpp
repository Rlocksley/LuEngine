#pragma once
#include "Global.hpp"
#include "flecs.h"
#include "Camera2DData.hpp"

namespace Lu{
    namespace Component{
        // The 2D camera used by all Text (every Transform2D based visual). Use ONE Camera2D entity.
        // Whenever it changes, the Camera2D module sends EcsRequest::UpdateCamera2D to the renderer,
        // which mirrors it into a uniform buffer that the 2D shaders read (see Camera2DPackage).
        // Without any Camera2D entity the identity camera is used (world pixel == screen pixel),
        // which is exactly what a HUD needs.
        struct Camera2D{
            glm::vec2 position{0.f, 0.f};
            glm::vec2 pivot{0.f, 0.f};
            float rotation{0.f};
            float zoom{1.f};

            Camera2DData data() const {
                return Camera2DData{position, pivot, rotation, zoom};
            }
        };
    }
}
