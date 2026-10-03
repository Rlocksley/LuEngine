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
            VkDeviceAddress transformBufferAddress{0};
            uint32_t _pad2{0};
            uint32_t _pad3{0};
            glm::vec4 cullSphere{0.f, 0.f, 0.f, 0.f};
        };

        using MultiMeshId = uint32_t;
        using MultiMeshTransformRangeId = uint32_t;

        struct MultiMeshInstance {
            MultiMesh mesh{};
            MultiMeshId id{0};
            MultiMeshTransformRangeId begin{0};
            MultiMeshTransformRangeId end{0};
            uint32_t _pad{0};

            MultiMeshInstance() = default;
            MultiMeshInstance(const MultiMesh mesh, MultiMeshId id)
                : mesh(mesh), id(id) {}
            MultiMeshInstance(const MultiMesh mesh, MultiMeshId id,
                              MultiMeshTransformRangeId begin, MultiMeshTransformRangeId end,
                              uint32_t padding = 0)
                : mesh(mesh), id(id), begin(begin), end(end), _pad(padding) {}
        };

        static_assert(sizeof(Material) == 80);
        static_assert(offsetof(MultiMesh, transformBufferAddress) == 32);
        static_assert(offsetof(MultiMesh, cullSphere) == 48);
        static_assert(sizeof(MultiMesh) == 64);
        static_assert(offsetof(MultiMeshInstance, id) == 64);
        static_assert(offsetof(MultiMeshInstance, begin) == 68);
        static_assert(offsetof(MultiMeshInstance, end) == 72);
        static_assert(sizeof(MultiMeshInstance) == 80);
    }
}