#pragma once
#include "Global.hpp"
#include "flecs.h"
#include "Channel.hpp"
#include "TripleBuffers.hpp"
#include "component/Transform2D.hpp"
#include "component/Transform2DGpu.hpp"
#include "component/Transform.hpp"     // Component::Static

namespace Lu {
namespace Module {
    // Mirrors every Component::WorldTransform2D (computed from Transform2D) that also has a Component::Transform2DGpu to the renderer's
    // Transform2DPackage (same structure as Module::TransformGpu):
    //   add/set  -> EcsRequest::CreateTransform2D (upsert)
    //   remove   -> EcsRequest::DestroyTransform2D (also when only the Transform2DGpu tag is removed)
    //   each frame (entities without Component::Static) -> EcsRequest::UpdateTransform2D via the triple buffer
    // Imported before Module::Text, so the slot exists on the renderer side before CreateText arrives.
    struct Transform2DGpu{
        Transform2DGpu(flecs::world& world){
            world.component<Component::Transform2DGpu>();

            world.observer<const Component::WorldTransform2D>()
                .with<Component::Transform2DGpu>()
                .event(flecs::OnAdd)
                .event(flecs::OnSet)
                .each([](flecs::entity entity, const Component::WorldTransform2D&){
                    entity.add<Component::Transform2DGpuDirty>();
                });

            world.system<const Component::WorldTransform2D>("EcsRequest::CreateTransform2D System")
                .with<Component::Transform2DGpu>()
                .with<Component::Transform2DGpuDirty>()
                .run([](flecs::iter it){
                    std::vector<EcsRequest::EcsRequest> requests;

                    while (it.next()) {
                        auto transforms = it.field<const Component::WorldTransform2D>(0);
                        for (size_t i = 0; i < it.count(); ++i) {
                            auto entity = it.entity(i);
                            requests.push_back(EcsRequest::CreateTransform2D{entity, transforms[i]});
                            entity.remove<Component::Transform2DGpuDirty>();
                        }
                    }

                    if (!requests.empty()) GetChannel().send(std::move(requests));
                });

            world.observer<const Component::WorldTransform2D>()
                .with<Component::Transform2DGpu>()
                .event(flecs::OnRemove)
                .each([](flecs::entity entity, const Component::WorldTransform2D&){
                    GetChannel().send(EcsRequest::DestroyTransform2D{entity.id()});
                });

            world.system<const Component::WorldTransform2D>("EcsRequest::UpdateTransform2D System")
                .with<Component::Transform2DGpu>()
                .without<Component::Static>()
                .kind(flecs::PostUpdate)   // after the propagation system of Module::Transform2D
                .run([](flecs::iter it){
                    Core::GetTransform2DTripleBuffer().write(
                        [&](std::vector<EcsRequest::UpdateTransform2D>& buffer){
                            buffer.clear();
                            while (it.next()) {
                                auto transforms = it.field<const Component::WorldTransform2D>(0);
                                for (size_t i = 0; i < it.count(); ++i) {
                                    buffer.push_back(EcsRequest::UpdateTransform2D{it.entity(i), transforms[i]});
                                }
                            }
                        });
                });
        }
    };
}
}
