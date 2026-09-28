#pragma once

#include "Channel.hpp"
#include "flecs.h"
#include "component/Material.hpp"
#include "component/Transform.hpp"
#include "component/TransformGpu.hpp"

namespace Lu{
    namespace Component{
        struct Mesh{
            flecs::entity mesh;
            flecs::entity pipeline;
        };
    }

    namespace Module{
        struct Mesh{
            Mesh(flecs::world& world){
                world.component<Component::Mesh>();


                world.observer<const Component::Mesh, const Component::Material>()
                    .with<Component::Transform>().filter()
                    .with<Component::TransformGpu>().filter()
                    .term_at(2).up()
                    .term_at(3).up()
                    .event(flecs::OnSet)
                    .each([](flecs::entity e, const Component::Mesh& mesh, const Component::Material& material) {
                        GetChannel().send(EcsRequest::CreateMesh{
                            e.parent().id(), e.id(), mesh.mesh.id(), mesh.pipeline.id(), material
                        });
                    });

                world.observer<const Component::Transform, const Component::TransformGpu>()
                    .with<Component::Mesh>().filter()
                    .with<Component::Material>().filter()
                    .term_at(0).up()
                    .term_at(1).up()
                    .event(flecs::OnAdd)
                    .each([](flecs::entity e, const Component::Transform& transform, const Component::TransformGpu& transformGpu) {
                        const auto& mesh = e.get<Component::Mesh>();
                        const auto& material = e.get<Component::Material>();
                        GetChannel().send(EcsRequest::CreateMesh{
                            e.parent().id(), e.id(), mesh.mesh.id(), mesh.pipeline.id(), material
                        });
                    });

                world.observer<const Component::Mesh, const Component::Material>()
                    .with<Component::Transform>()
                    .with<Component::TransformGpu>()
                    .term_at(2).up()
                    .term_at(3).up()
                    .event(flecs::OnRemove)
                    .each([](flecs::entity e, const Component::Mesh& mesh, const Component::Material& material) {
                        GetChannel().send(EcsRequest::DestroyMesh{e.id()});
                    });



            }
        };
    }
}