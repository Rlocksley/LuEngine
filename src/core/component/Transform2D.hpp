#pragma once

#include "Global.hpp"
#include "flecs.h"
#include "Static.hpp"
#include "TransformHierarchy.hpp"

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
        //
        // Transform2D is LOCAL to the parent entity if that (or an ancestor) has a Transform2D, and WORLD if there is
        // none. The WorldTransform2D (parent world * local) is added / updated / removed automatically.
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

        // The WORLD transform of an entity with a Transform2D, maintained automatically (see Transform2D). This is what the
        // renderer gets (EcsRequest::CreateTransform2D / UpdateTransform2D). Read it, but do not set it.
        // positionZ (layer) is added up: child layer = parent layer + local layer.
        struct WorldTransform2D : Transform2D {
            WorldTransform2D() = default;
            WorldTransform2D(const Transform2D& other) : Transform2D(other) {}
        };

        inline WorldTransform2D composeWorld(const WorldTransform2D& parentWorld, const Transform2D& local) {
            WorldTransform2D result;
            result.scale = parentWorld.scale * local.scale;
            result.rotation = parentWorld.rotation * local.rotation;
            const glm::vec3 rotated = parentWorld.rotation * glm::vec3(parentWorld.scale * local.position, 0.0f);
            result.position = parentWorld.position + glm::vec2(rotated.x, rotated.y);
            const uint64_t z = static_cast<uint64_t>(parentWorld.positionZ) + local.positionZ;
            result.positionZ = static_cast<uint32_t>(z > UINT32_MAX ? UINT32_MAX : z);
            return result;
        }
    }

    namespace Module {
        struct Transform2D {
            Transform2D(flecs::world& world) {
                Component::Detail::TransformHierarchy<Component::Transform2D, Component::WorldTransform2D>::install(world);
            }
        };
    }
}
