#pragma once

#include "Global.hpp"
#include "flecs.h"
#include "Channel.hpp"
#include "component/MultiMesh.hpp"
#include "component/Transform.hpp"
#include "component/TransformGpu.hpp"

namespace Lu{
    namespace Module{
        struct MultiMeshGpu{
            explicit MultiMeshGpu(flecs::world& world){
                world.component<Component::MultiMesh>();
                world.component<Component::MultiMeshGpuDirty>();

                world.observer<const Component::MultiMesh>()
                    .with<Component::Transform>().filter()
                    .with<Component::TransformGpu>().filter()
                    .term_at(1).up()
                    .term_at(2).up()
                    .event(flecs::OnAdd)
                    .event(flecs::OnSet)
                    .each([](flecs::entity entity, const Component::MultiMesh&){
                        entity.add<Component::MultiMeshGpuDirty>();
                    });

                world.observer()
                    .with<Component::MultiMesh>().filter()
                    .with<Component::Transform>()
                    .with<Component::TransformGpu>()
                    .term_at(1).up()
                    .term_at(2).up()
                    .event(flecs::OnAdd)
                    .each([](flecs::entity entity){
                        entity.add<Component::MultiMeshGpuDirty>();
                    });

                world.system<const Component::MultiMesh>("EcsRequest::CreateMultiMesh System")
                    .with<Component::MultiMeshGpuDirty>()
                    .run([](flecs::iter it){
                        std::vector<EcsRequest::EcsRequest> requests;
                        uint32_t processed = 0;
                        while(processed < Core::MAX_ECS_REQUEST_CREATE_MULTI_MESH_PER_FRAME && it.next()){
                            const auto multiMeshes = it.field<const Component::MultiMesh>(0);
                            for(int index = 0; index < it.count() &&
                                processed < Core::MAX_ECS_REQUEST_CREATE_MULTI_MESH_PER_FRAME; ++index){
                                auto entity = it.entity(index);
                                const auto& multiMesh = multiMeshes[index];
                                requests.push_back(EcsRequest::CreateMultiMesh{
                                    entity.parent().id(),
                                    entity.id(),
                                    multiMesh.meshId.id(),
                                    multiMesh.computePipeId.id(),
                                    multiMesh.pipelineId.id(),
                                    multiMesh.cullSphere,
                                    multiMesh.instances
                                });
                                entity.remove<Component::MultiMeshGpuDirty>();
                                ++processed;
                            }
                        }
                        GetChannel().send(std::move(requests));
                    });

                world.observer<const Component::MultiMesh>()
                    .event(flecs::OnRemove)
                    .each([](flecs::entity entity, const Component::MultiMesh&){
                        GetChannel().send(EcsRequest::DestroyMultiMesh{entity.id()});
                    });
            }
        };
    }
}