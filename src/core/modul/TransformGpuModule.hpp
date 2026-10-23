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


                // ---- MeshTransform: local transform of the meshes, own slot in the GPU transform buffer ----
                // (these systems are defined before the Mesh / Skeleton modules on purpose: the slot has to
                //  exist on the renderer side before the CreateMesh / CreateSkeletonMesh requests arrive)
                world.observer<const Component::MeshTransform>()
                    .with<Component::TransformGpu>()
                    .event(flecs::OnAdd)
                    .event(flecs::OnSet)
                    .each([](flecs::entity entity, const Component::MeshTransform& meshTransform){
                        entity.add<Component::MeshTransformGpuDirty>();
                    });

                world.system<Component::MeshTransform>("EcsRequest::CreateMeshTransform System")
                .with<Component::MeshTransformGpuDirty>()
                .run([](flecs::iter it){
                    std::vector<EcsRequest::EcsRequest> meshTransformBuffer;

                    while (it.next()) {
                        auto meshTransforms = it.field<const Component::MeshTransform>(0);
                        for (int i = 0; i < it.count(); ++i) {
                            auto entity = it.entity(i);
                            meshTransformBuffer.push_back(EcsRequest::CreateMeshTransform{ entity, meshTransforms[i]});
                            entity.remove<Component::MeshTransformGpuDirty>();
                        }
                    }

                    if (!meshTransformBuffer.empty()) GetChannel().send(std::move(meshTransformBuffer));
                });

                world.observer<const Component::MeshTransform>()
                    .with<Component::TransformGpu>()
                    .event(flecs::OnRemove)
                    .each([](flecs::entity entity, const Component::MeshTransform& meshTransform){
                        GetChannel().send(EcsRequest::DestroyMeshTransform{entity.id()});
                    });

                world.system<const Component::MeshTransform>()
                .with<Component::TransformGpu>()
                .without<Component::Static>()
                .run([](flecs::iter it){
                    Core::GetMeshTransformTripleBuffer().write(
                        [&](std::vector<EcsRequest::UpdateMeshTransform>& meshTransformBuffer){
                            meshTransformBuffer.clear();
                            while (it.next()) {
                                auto meshTransforms = it.field<const Component::MeshTransform>(0);
                                for (int i = 0; i < it.count(); ++i) {
                                    meshTransformBuffer.push_back(EcsRequest::UpdateMeshTransform{ it.entity(i), meshTransforms[i]});
                                }
                            }
                        });
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
