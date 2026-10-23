#pragma once
#include "TripleBuffer.hpp"
#include "Channel.hpp"
#include "instance/TransformInstance.hpp"
#include "instance/SkeletonInstance.hpp"
#include "Constant.hpp"
#include <atomic>

namespace Lu{
    namespace Core{
        inline TripleBuffer<std::vector<EcsRequest::UpdateTransform>>& GetTransformTripleBuffer(){
        static TripleBuffer<std::vector<EcsRequest::UpdateTransform>> transforms;
        return transforms;
    }

        // One ECS frame of skeleton poses. Both arrays are allocated once at full capacity.
        // The ECS worker threads reserve space with the two atomic cursors and write
        // their blended local poses directly into `poses` (no vectors, no locks); a single
        // publish step then stores the counts. The renderer reads the latest snapshot.
        struct SkeletonPoseSnapshot {
            std::vector<EcsRequest::UpdateSkeletonPose> headers;
            std::vector<NodePose> poses;
            std::atomic<uint32_t> headerCursor{0};
            std::atomic<uint32_t> poseCursor{0};
            uint32_t headerCount{0};
            uint32_t poseCount{0};

            SkeletonPoseSnapshot() : headers(MAX_SKELETONS), poses(MAX_SKELETON_POSES_PER_FRAME) {}
        };

        inline TripleBuffer<SkeletonPoseSnapshot>& GetSkeletonPoseTripleBuffer(){
            static TripleBuffer<SkeletonPoseSnapshot> poses;
            return poses;
        }
    }
}