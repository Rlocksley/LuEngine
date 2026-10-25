#pragma once
#include "Global.hpp"
#include "flecs.h"
#include "Channel.hpp"
#include "component/Mesh2D.hpp"
#include "component/Transform2D.hpp"
#include "component/Transform2DGpu.hpp"

namespace Lu {
namespace Module {
    // Component::Mesh2D (on a child of an entity with a Component::Transform2D AND a Component::Transform2DGpu)
    // -> EcsRequest::CreateMesh2D / DestroyMesh2D. The parent has to be mirrored by the GPU, because the renderer
    // resolves its Transform2D slot. Same structure as Module::Text and Module::Mesh.
    struct Mesh2D{
        Mesh2D(flecs::world& world){
            world.component<Component::Mesh2D>();

            // Mesh2D added / changed while the parent has a Transform2D and a Transform2DGpu
            world.observer<const Component::Mesh2D>()
                .with<Component::Transform2D>().filter()
                .with<Component::Transform2DGpu>().filter()
                .term_at(1).up()
                .term_at(2).up()
                .event(flecs::OnAdd)
                .event(flecs::OnSet)
                .each([](flecs::entity e, const Component::Mesh2D&) {
                    e.add<Component::Mesh2DGpuDirty>();
                });

            // Transform2D / Transform2DGpu added to the parent after the Mesh2D exists
            world.observer()
                .with<Component::Mesh2D>().filter()
                .with<Component::Transform2D>()
                .with<Component::Transform2DGpu>()
                .term_at(1).up()
                .term_at(2).up()
                .event(flecs::OnAdd)
                .each([](flecs::entity e) {
                    e.add<Component::Mesh2DGpuDirty>();
                });

            world.system<const Component::Mesh2D>("EcsRequest::CreateMesh2D System")
                .with<Component::Mesh2DGpuDirty>()
                .run([](flecs::iter it){
                    std::vector<EcsRequest::EcsRequest> requests;

                    while (it.next()) {
                        auto meshes = it.field<const Component::Mesh2D>(0);
                        for (size_t i = 0; i < it.count(); ++i) {
                            auto entity = it.entity(i);
                            const auto parent = entity.parent();
                            const Component::Mesh2D& mesh = meshes[i];
                            if (parent.is_valid() && parent.has<Component::Transform2D>() && parent.has<Component::Transform2DGpu>() &&
                                mesh.mesh.id() != 0 && mesh.pipeline.id() != 0) {
                                requests.push_back(EcsRequest::CreateMesh2D{
                                    parent.id(), entity.id(), mesh.mesh.id(), mesh.pipeline.id(), mesh.texture.id(),
                                    mesh.color, mesh.uvRect, mesh.params});
                            }
                            entity.remove<Component::Mesh2DGpuDirty>();
                        }
                    }

                    if (!requests.empty()) GetChannel().send(std::move(requests));
                });

            world.observer<const Component::Mesh2D>()
                .with<Component::Transform2D>()
                .with<Component::Transform2DGpu>()
                .term_at(1).up()
                .term_at(2).up()
                .event(flecs::OnRemove)
                .each([](flecs::entity e, const Component::Mesh2D&) {
                    GetChannel().send(EcsRequest::DestroyMesh2D{e.id()});
                });
        }
    };
}
}
