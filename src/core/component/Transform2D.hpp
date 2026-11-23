#pragma once

#include "Global.hpp"
#include "flecs.h"
#include "Static.hpp"


namespace Lu {
    namespace Component {

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

             Transform2D operator*(const Transform2D& other) const { 
                Transform2D result;
                result.scale = scale * other.scale;
                result.rotation = rotation * other.rotation;
                const glm::vec3 rotated = rotation * glm::vec3(scale * other.position, 0.0f);
                result.position = position + glm::vec2(rotated.x, rotated.y);
                const uint64_t z = static_cast<uint64_t>(positionZ) + other.positionZ;
                result.positionZ = static_cast<uint32_t>(z > UINT32_MAX ? UINT32_MAX : z);
                return result;
             }
        };

        // The WORLD transform of an entity with a Transform2D, maintained automatically (see Transform2D). This is what the
        // renderer gets (EcsRequest::CreateTransform2D / UpdateTransform2D). Read it, but do not set it.
        // positionZ (layer) is added up: child layer = parent layer + local layer.
        struct WorldTransform2D : Transform2D {
            WorldTransform2D() = default;
            WorldTransform2D(const Transform2D& other) : Transform2D(other) {}
        };


    }
}
