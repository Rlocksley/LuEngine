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

        struct MeshGpuDirty{};
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
                    .event(flecs::OnAdd)
                    .event(flecs::OnSet)
                    .each([](flecs::entity e, const Component::Mesh& mesh, const Component::Material& material) {
                        e.add<Component::MeshGpuDirty>();
                    });

                world.observer()
                    .with<Component::Mesh>().filter()
                    .with<Component::Material>().filter()
                    .with<Component::Transform>()
                    .with<Component::TransformGpu>()
                    .term_at(2).up()
                    .term_at(3).up()
                    .event(flecs::OnAdd)
                    .each([](flecs::entity e) {
                        e.add<Component::MeshGpuDirty>();
                    });

                world.system<const Component::Mesh, const Component::Material>()
                .with<Component::MeshGpuDirty>()
                .run([](flecs::iter it){
                    std::vector<EcsRequest::EcsRequest> meshBuffer;
                    
                    while (it.next()) {
                        auto meshes = it.field<const Component::Mesh>(0);
                        auto materials = it.field<const Component::Material>(1);
                        
                        for (int i = 0; i < it.count(); ++i) {
                            auto entity = it.entity(i);
                            meshBuffer.push_back(EcsRequest::CreateMesh{entity.parent(), entity , meshes[i].mesh, meshes[i].pipeline, materials[i]});
                            entity.remove<Component::MeshGpuDirty>();
                        }
                    }

                    GetChannel().send(meshBuffer);
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