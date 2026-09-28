#pragma once
#include "Global.hpp"
#include "flecs.h"
#include "Channel.hpp"

namespace Lu{
    namespace Component{
        struct TransformGpu{};
    }

    namespace Module{
    struct TransformGpuUtilities{
        using TransformEntity = flecs::entity_t;
        std::mutex mutex;
        std::unordered_map<flecs::entity_t, uint32_t> entities;
    };

    inline TransformGpuUtilities transformGpuUtilities;

        struct TransformGpu{
            TransformGpu(flecs::world& world){
                world.component<Component::TransformGpu>();

                world.observer<const Component::Transform>()
                    .with<Component::TransformGpu>().filter()
                    .event(flecs::OnSet)
                    .each([](flecs::entity entity, const Component::Transform& transform){
                        bool isNotFirstTime;
                        {
                            std::lock_guard guard(transformGpuUtilities.mutex);
                            transformGpuUtilities.entities[entity.id()]++;
                            isNotFirstTime = (transformGpuUtilities.entities[entity.id()] > 1);
                        }
                        if(isNotFirstTime){
                            GetChannel().send(EcsRequest::CreateTransform{entity.id(), transform});
                        }
                    });

                world.observer<const Component::Transform>()
                    .with<Component::TransformGpu>().filter()
                    .event(flecs::OnAdd)
                    .each([](flecs::entity entity, const Component::Transform& transform){
                        {
                            std::lock_guard guard(transformGpuUtilities.mutex);
                            transformGpuUtilities.entities[entity.id()] = 0;
                        }
                        GetChannel().send(EcsRequest::CreateTransform{entity.id(), transform});
                    });


                world.observer()
                    .with<Component::Transform>().filter()
                    .with<Component::TransformGpu>()
                    .event(flecs::OnAdd)
                    .each([](flecs::entity entity){
                        {
                            std::lock_guard guard(transformGpuUtilities.mutex);
                            transformGpuUtilities.entities[entity.id()] = 1;
                        }
                        const auto& transform = entity.get<Component::Transform>();
                        GetChannel().send(EcsRequest::CreateTransform{entity.id(), transform});
                    });


                world.observer<const Component::Transform>()
                    .with<Component::TransformGpu>()
                    .event(flecs::OnRemove)
                    .each([](flecs::entity entity, const Component::Transform& transform){
                        {
                            std::lock_guard guard(transformGpuUtilities.mutex);
                            transformGpuUtilities.entities.erase(entity.id());
                        }
                        GetChannel().send(EcsRequest::DestroyTransform{entity.id()});
                    });
            }
        };
    }
}