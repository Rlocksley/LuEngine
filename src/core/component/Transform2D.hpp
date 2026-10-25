#pragma once

#include "Global.hpp"
#include "flecs.h"

namespace Lu {
    namespace Component {

        // 2D transform in pixels (origin = top-left of the window, x right, y down - before the Camera2D).
        // Put it on the PARENT entity of the entities that have a Component::Text:
        //
        //      auto label = world.entity("Label").set<Component::Transform2D>({.position = {20.f, 20.f}});
        //      world.entity("LabelText").child_of(label).set<Component::Text>({...});
        //
        // It is only mirrored into the Transform2DPackage of the renderer if the entity also has a
        // Component::Transform2DGpu (see Transform2DGpu.hpp), exactly like Transform / TransformGpu:
        //
        //      world.entity("Label").set<Component::Transform2D>({...}).add<Component::Transform2DGpu>();
        //
        // Add Component::Static if it never changes after it was set, to save the per-frame update.
        struct Transform2D {
            glm::vec2 position{0.0f, 0.0f};
            uint32_t positionZ{0};                       // layer: a higher value is drawn in front (clamped to 0..65535)
            glm::quat rotation{1.0f, 0.0f, 0.0f, 0.0f};  // rotation around the screen z axis (identity)
            glm::vec2 scale{1.0f, 1.0f};

            // Rotation around the screen z axis. A positive angle turns clockwise on the screen (y points down).
            void setAngle(const float angleRadians) {
                rotation = glm::angleAxis(angleRadians, glm::vec3(0.0f, 0.0f, 1.0f));
            }

            float getAngle() const {
                return 2.0f * std::atan2(rotation.z, rotation.w);
            }

            glm::mat4 getMatrix() const {
                const glm::mat4 translationMat = glm::translate(glm::mat4(1.0f), glm::vec3(position, 0.0f));
                const glm::mat4 rotationMat = glm::toMat4(rotation);
                const glm::mat4 scaleMat = glm::scale(glm::mat4(1.0f), glm::vec3(scale, 1.0f));
                return translationMat * rotationMat * scaleMat;
            }
        };
    }
}
