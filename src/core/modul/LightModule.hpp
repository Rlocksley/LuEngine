#pragma once
#include "Channel.hpp"
#include "flecs.h"
#include "component/Light.hpp"
#include "component/Transform.hpp"
#include "component/TransformGpu.hpp"

namespace Lu{

    namespace Module{
        struct Light{
            Light(flecs::world& world){
                world.component<Component::Light>();


                world.observer<const Component::Light>()
                    .with<Component::Transform>().filter()
                    .with<Component::TransformGpu>().filter()
                    .term_at(1).up()
                    .term_at(2).up()
                    .event(flecs::OnAdd)
                    .event(flecs::OnSet)
                    .each([](flecs::entity e, const Component::Light& light) {
                        e.add<Component::LightGpuDirty>();
                    });

                world.observer()
                    .with<Component::Light>().filter()
                    .with<Component::Transform>()
                    .with<Component::TransformGpu>()
                    .term_at(1).up()
                    .term_at(2).up()
                    .event(flecs::OnAdd)
                    .each([](flecs::entity e) {
                        e.add<Component::LightGpuDirty>();
                    });

                world.system<const Component::Light>()
                .with<Component::LightGpuDirty>()
                .run([](flecs::iter it){
                    std::vector<EcsRequest::EcsRequest> lightBuffer;
                    
                    while (it.next()) {
                        auto lights = it.field<const Component::Light>(0);
                        
                        for (int i = 0; i < it.count(); ++i) {
                            auto entity = it.entity(i);
                            lightBuffer.push_back(EcsRequest::CreateLight{entity.parent().id(), entity, lights[i]});
                            entity.remove<Component::LightGpuDirty>();
                        }
                    }

                    GetChannel().send(lightBuffer);
                });


                world.observer<const Component::Light>()
                    .with<Component::Transform>()
                    .with<Component::TransformGpu>()
                    .term_at(1).up()
                    .term_at(2).up()
                    .event(flecs::OnRemove)
                    .each([](flecs::entity e, const Component::Light& light) {
                        GetChannel().send(EcsRequest::DestroyLight{e.id()});
                    });

            }
        };
    }
}