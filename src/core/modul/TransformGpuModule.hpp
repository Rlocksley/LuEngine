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
                    .with<Component::TransformGpu>()
                    .event(flecs::OnAdd)
                    .event(flecs::OnSet)
                    .each([](flecs::entity entity, const Component::Transform& transform){
                        entity.add<Component::TransformGpuDirty>();
                    });

                world.system<Component::Transform>("EcsRequest::CreateTransform System")
                .with<Component::TransformGpuDirty>()
                .run([](flecs::iter it){
                    std::vector<EcsRequest::EcsRequest> transformBuffer;
                    
                    while (it.next()) {
                        auto transforms = it.field<const Component::Transform>(0);
                        for (int i = 0; i < it.count(); ++i) {
                            auto entity = it.entity(i);
                            transformBuffer.push_back(EcsRequest::CreateTransform{ entity, transforms[i]});
                            entity.remove<Component::TransformGpuDirty>();
                        }
                    }

                    GetChannel().send(transformBuffer);
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
