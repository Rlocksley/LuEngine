#include "Global.hpp"
#include "flecs.h"
#include "Channel.hpp"
#include "TripleBuffers.hpp"
#include "component/TransformGpu.hpp"
#include "component/Transform.hpp"
#include "instance/TransformInstance.hpp"

namespace Lu {
namespace Module {
        struct TransformGpu{
            TransformGpu(flecs::world& world){
                world.component<Component::TransformGpu>();

                world.observer<const Component::Transform>()
                    .with<Component::TransformGpu>().filter()
                    .event(flecs::OnAdd)
                    .each([](flecs::entity entity, const Component::Transform& transform){
                        GetChannel().send(EcsRequest::CreateTransform{entity.id(), transform});
                    });


                world.observer()
                    .with<Component::Transform>().filter()
                    .with<Component::TransformGpu>()
                    .event(flecs::OnAdd)
                    .each([](flecs::entity entity){
                        const auto& transform = entity.get<Component::Transform>();
                        GetChannel().send(EcsRequest::CreateTransform{entity.id(), transform});
                    });


                world.observer<const Component::Transform>()
                    .with<Component::TransformGpu>()
                    .event(flecs::OnRemove)
                    .each([](flecs::entity entity, const Component::Transform& transform){
                        GetChannel().send(EcsRequest::DestroyTransform{entity.id()});
                    });

                world.system<const Component::Transform>()
                .with<Component::TransformGpu>()
                .without<Component::Static>()
                .run([](flecs::iter it){
                    Core::GetTransformTripleBuffer().write(
                        [&](std::vector<EcsRequest::UpdateTransform>& transformBuffer){
                            transformBuffer.clear();
                            while (it.next()) {
                                    auto transforms = it.field<const Component::Transform>(0);
                                    for (int i = 0; i < it.count(); ++i) {
                                        transformBuffer.push_back(EcsRequest::UpdateTransform{ it.entity(i), transforms[i]});
                                    }
                                }
                            });
                });
            }
        };
    }
}
