#pragma once

#include "flecs.h"

namespace Lu::Module {

// Declares the ECS phases that order world-transform propagation, GPU request
// generation, and publication of each ECS frame to the renderer.
struct EcsRequestPhases {
    explicit EcsRequestPhases(flecs::world& world) {
        auto worldTransform3D = world.entity("EcsWorldTransform3D")
            .add(flecs::Phase)
            .depends_on(flecs::PostFrame);
        auto updateTransform3D = world.entity("EcsUpdateTransform3D")
            .add(flecs::Phase)
            .depends_on(worldTransform3D);
        auto requestTransform3D = world.entity("EcsRequestTransform3D")
            .add(flecs::Phase)
            .depends_on(updateTransform3D);
        auto requestMeshTransform3D = world.entity("EcsRequestMeshTransform3D")
            .add(flecs::Phase)
            .depends_on(requestTransform3D);
        auto updateMeshTransform3D = world.entity("EcsUpdateMeshTransform3D")
            .add(flecs::Phase)
            .depends_on(requestMeshTransform3D);
        auto requestSkeleton3D = world.entity("EcsRequestSkeleton3D")
            .add(flecs::Phase)
            .depends_on(updateMeshTransform3D);
        auto requestObjects3D = world.entity("EcsRequestObjects3D")
            .add(flecs::Phase)
            .depends_on(requestSkeleton3D);

        auto worldTransform2D = world.entity("EcsWorldTransform2D")
            .add(flecs::Phase)
            .depends_on(flecs::PostFrame);
        auto updateTransform2D = world.entity("EcsUpdateTransform2D")
            .add(flecs::Phase)
            .depends_on(worldTransform2D);
        auto requestTransform2D = world.entity("EcsRequestTransform2D")
            .add(flecs::Phase)
            .depends_on(updateTransform2D);
        auto visual2D = world.entity("EcsRequestVisual2D")
            .add(flecs::Phase)
            .depends_on(requestTransform2D);

        world.entity("EcsRequestTransfer")
            .add(flecs::Phase)
            .depends_on(requestObjects3D)
            .depends_on(visual2D);
    }
};

} // namespace Lu::Module
