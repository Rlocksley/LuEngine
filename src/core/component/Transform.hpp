#pragma once

#include "Global.hpp"
#include "flecs.h"

namespace Lu {
    namespace Component {
        struct Static{}; // used for should be updated each frame or not information

        struct Transform {
            glm::vec3 position{0.0f, 0.0f, 0.0f};
            glm::quat rotation{1.0f, 0.0f, 0.0f, 0.0f}; // Identity quaternion
            glm::vec3 scale{1.0f, 1.0f, 1.0f};

            Transform() = default;

            Transform(const glm::vec3& inPosition, const float angleRadians, const glm::vec3& axis, const glm::vec3& inScale)
                    : position(inPosition),
                        rotation(glm::angleAxis(angleRadians, glm::normalize(axis))),
                        scale(inScale) {}

            Transform(const glm::vec3& inPosition, const glm::vec3& eulerAnglesRadians, const glm::vec3& inScale)
                    : position(inPosition), rotation(glm::quat(eulerAnglesRadians)), scale(inScale) {}

            glm::mat4 getMatrix() const {
                glm::mat4 translation = glm::translate(glm::mat4(1.0f), position);
                glm::mat4 rotationMat = glm::toMat4(rotation);
                glm::mat4 scaleMat = glm::scale(glm::mat4(1.0f), scale);
                return translation * rotationMat * scaleMat;
            }

            Transform operator*(const Transform& other) const {
                Transform result;
                result.scale = scale * other.scale;
                result.rotation = rotation * other.rotation;
                glm::vec3 scaledPos = scale * other.position;
                glm::vec3 rotatedPos = rotation * scaledPos;
                result.position = position + rotatedPos;
                return result;
            }
        };

        // Transform of the visual part (Mesh / SkeletonMesh children) of an entity, LOCAL to the
        // entity's Transform:  world = Transform * MeshTransform * (skinning) * vertex.
        //
        //   Transform      : the "body" - written by physics (CharacterCapsule, rigid bodies, ...)
        //   MeshTransform  : offset / facing / scale of the visuals relative to that body
        //
        // Optional. Without a MeshTransform the meshes use only the Transform (= identity offset).
        // It needs the TransformGpu tag on the same entity, like Transform.
        struct MeshTransform : Transform {
            using Transform::Transform;
            MeshTransform() = default;
            MeshTransform(const Transform& other) : Transform(other) {}
        };
    }

    namespace Module{
        struct Transform{
            Transform(flecs::world& world){
                world.component<Component::Transform>();
                world.component<Component::MeshTransform>();

            }
        };
    }
}