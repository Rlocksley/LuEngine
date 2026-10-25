#pragma once

#include "Global.hpp"

namespace Lu{
    namespace Core{

        // GPU side of Component::Mesh2D = one element of the Mesh2DBuffer (std430, 80 bytes).
        // Mirrored by `struct Mesh2D` in mesh2d_transfer.comp / mesh2d_cull.comp / mesh2d_basic.vert/frag.
        struct Mesh2D{
            glm::vec4 color{1.0f};
            glm::vec4 uvRect{0.0f, 0.0f, 1.0f, 1.0f};
            glm::vec4 params{0.0f};
            uint32_t textureId{UINT32_MAX};   // slot in the global texture array, UINT32_MAX = no texture
            uint32_t transform2DId{0};        // slot of the parent's Transform2D
            uint32_t pipelineId{0};           // Mesh2D pipeline (selects the indirect buffer in mesh2d_cull.comp)
            uint32_t meshInfoId{0};           // GeometryInfo of the Mesh2D geometry
            uint32_t valid{0};                // 0 = free slot: not drawn
            uint32_t _pad0{0};
            uint32_t _pad1{0};
            uint32_t _pad2{0};

            Mesh2D() = default;

            Mesh2D(const glm::vec4& color, const glm::vec4& uvRect, const glm::vec4& params,
                   const uint32_t textureId, const uint32_t transform2DId,
                   const uint32_t pipelineId, const uint32_t meshInfoId) :
                color(color), uvRect(uvRect), params(params), textureId(textureId),
                transform2DId(transform2DId), pipelineId(pipelineId), meshInfoId(meshInfoId), valid(1) {}
        };

        using Mesh2DId = uint32_t;

        struct Mesh2DInstance{
            Mesh2D mesh{};
            Mesh2DId id{0};
            uint32_t _pad[3]{0, 0, 0};

            Mesh2DInstance() = default;
            Mesh2DInstance(const Mesh2D& mesh, const Mesh2DId id) : mesh(mesh), id(id) {}
        };

        static_assert(sizeof(Mesh2D) == 80);
        static_assert(offsetof(Mesh2D, textureId) == 48);
        static_assert(offsetof(Mesh2DInstance, id) == 80);
        static_assert(sizeof(Mesh2DInstance) == 96);
    }
}
