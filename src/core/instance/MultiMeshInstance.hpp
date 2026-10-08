#pragma once

#include "Global.hpp"
#include "MaterialInstance.hpp"
#include "TransformInstance.hpp"

namespace Lu{
    namespace Core{

        struct MultiMesh {
            uint32_t transformId{0};
            uint32_t computePipeId{0};
            uint32_t pipelineId{0};
            uint32_t meshInfoId{0};
            uint32_t size{0};
            uint32_t valid{0};
            uint32_t _pad0{0};
            uint32_t _pad1{0};
            VkDeviceAddress meshInstanceBufferAddress[2]{0, 0};
            glm::vec4 cullSphere{0.f, 0.f, 0.f, 0.f};
        };

        using MultiMeshId = uint32_t;
        using MultiMeshMeshInstanceRangeId = uint32_t;

        struct MultiMeshInstance {
            MultiMesh mesh{};
            MultiMeshId id{0};
            MultiMeshMeshInstanceRangeId begin{0};
            uint32_t _pad0{0};
            uint32_t _pad1{0};

            MultiMeshInstance() = default;
            MultiMeshInstance(const MultiMesh mesh, MultiMeshId id)
                : mesh(mesh), id(id) {}
            MultiMeshInstance(const MultiMesh mesh, MultiMeshId id,
                              MultiMeshMeshInstanceRangeId begin)
                : mesh(mesh), id(id), begin(begin), _pad0(0), _pad1(0) {}
        };

        static_assert(sizeof(Material) == 80);
        static_assert(offsetof(MultiMesh, meshInstanceBufferAddress) == 32);
        static_assert(offsetof(MultiMesh, cullSphere) == 48);
        static_assert(sizeof(MultiMesh) == 64);
        static_assert(offsetof(MultiMeshInstance, id) == 64);
        static_assert(offsetof(MultiMeshInstance, begin) == 68);
        static_assert(offsetof(MultiMeshInstance, _pad0) == 72);
        static_assert(offsetof(MultiMeshInstance, _pad1) == 76);
        static_assert(sizeof(MultiMeshInstance) == 80);
    }
}