#pragma once
#include "Global.hpp"
#include "../Constant.hpp"

namespace Lu{
    namespace Core{

        // Local pose of one skeleton node (translation / rotation / scale).
        // This exact layout is used on the CPU (animation sampling/blending),
        // in the ECS->renderer triple buffer and in the GPU upload buffer
        // (std430: three vec4). rotation = (x, y, z, w) like glm::quat in memory.
        struct NodePose {
            glm::quat rotation{1.0f, 0.0f, 0.0f, 0.0f};
            glm::vec4 position{0.0f, 0.0f, 0.0f, 0.0f};
            glm::vec4 scale{1.0f, 1.0f, 1.0f, 0.0f};

            static NodePose fromTRS(const glm::vec3& p, const glm::quat& r, const glm::vec3& s) {
                NodePose n;
                n.rotation = r;
                n.position = glm::vec4(p, 0.0f);
                n.scale = glm::vec4(s, 0.0f);
                return n;
            }
            glm::mat4 toMatrix() const {
                glm::mat4 m = glm::toMat4(rotation);
                m[0] *= scale.x; m[1] *= scale.y; m[2] *= scale.z;
                m[3] = glm::vec4(position.x, position.y, position.z, 1.0f);
                return m;
            }
        };
        static_assert(sizeof(NodePose) == 48);

        // GPU record of ONE skeleton instance: two transforms per node.
        //   skin[i] = nodeGlobal[i] * inverseBind[i]   (bone nodes, identity otherwise) -> deform a mesh
        //   node[i] = nodeGlobal[i]                    (every node)                     -> attach a mesh
        // Both are indexed by the skeleton node index (bone index == node index).
        struct SkeletonInstance {
            glm::mat4 skin[MAX_BONES_PER_SKELETON];
            glm::mat4 node[MAX_BONES_PER_SKELETON];
        };
        static_assert(sizeof(SkeletonInstance) == sizeof(glm::mat4) * MATS_PER_SKELETON);

        // Static per-node data of a skeleton asset (uploaded once).
        // Table index: assetId * MAX_BONES_PER_SKELETON + nodeIndex.
        struct SkeletonNodeGpu {
            glm::mat4 inverseBind{1.0f};
            int32_t parent{-1};
            uint32_t isBone{0};
            uint32_t _pad0{0};
            uint32_t _pad1{0};
        };
        static_assert(sizeof(SkeletonNodeGpu) == 80);

        // One entry per skeleton whose pose is uploaded this frame.
        // The poses live in a separate flat array at [firstPose, firstPose+nodeCount).
        struct SkeletonUpload {
            uint32_t slot{0};      // index into the SkeletonBuffer
            uint32_t assetId{0};   // index into the static node table
            uint32_t nodeCount{0};
            uint32_t firstPose{0};
        };
        static_assert(sizeof(SkeletonUpload) == 16);

        using SkeletonId = uint32_t;
        using SkeletonAssetId = uint32_t;
    }
}
