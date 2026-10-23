#pragma once
#include "Global.hpp"
#include "MaterialInstance.hpp"

namespace Lu{
    namespace Core{

        // GPU record of one skeleton mesh (see shader/skeleton_*.vert|comp).
        // nodeIndex == UINT32_MAX : deformed by the bone weights of its vertices
        // nodeIndex != UINT32_MAX : rigidly attached to that skeleton node (sword, helmet, ...)
        struct SkeletonMesh{
            Material material{};
            uint32_t transformId{0};
            uint32_t pipelineId{0};
            uint32_t meshInfoId{0};
            uint32_t skeletonId{0};
            uint32_t nodeIndex{UINT32_MAX};
            uint32_t valid{0};
            uint32_t _pad0{0};
            uint32_t _pad1{0};

            SkeletonMesh() = default;
            SkeletonMesh(const Material& material, uint32_t transformId, uint32_t pipelineId,
                         uint32_t meshInfoId, uint32_t skeletonId, uint32_t nodeIndex):
                material(material), transformId(transformId), pipelineId(pipelineId),
                meshInfoId(meshInfoId), skeletonId(skeletonId), nodeIndex(nodeIndex), valid(1) {}
        };

        using SkeletonMeshId = uint32_t;

        struct SkeletonMeshInstance {
            SkeletonMesh mesh{};
            SkeletonMeshId id{0};
            glm::vec3 _pad{0.f, 0.f, 0.f};

            SkeletonMeshInstance() = default;
            SkeletonMeshInstance(const SkeletonMesh& mesh, SkeletonMeshId id) : mesh(mesh), id(id) {}
        };

        static_assert(sizeof(SkeletonMesh) == 112);
        static_assert(offsetof(SkeletonMeshInstance, id) == 112);
        static_assert(sizeof(SkeletonMeshInstance) == 128);
    }
}
