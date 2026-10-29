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

                // The renderer mirrors the WorldTransform (computed from Transform and the parents' transforms, see Module::Transform).
                world.observer<const Component::WorldTransform>()
                    .with<Component::TransformGpu>()
                    .event(flecs::OnAdd)
                    .event(flecs::OnSet)
                    .each([](flecs::entity entity, const Component::WorldTransform& transform){
                        entity.add<Component::TransformGpuDirty>();
                    });

                world.system<const Component::WorldTransform>("EcsRequest::CreateTransform System")
                .with<Component::TransformGpuDirty>()
                .run([](flecs::iter it){
                    std::vector<EcsRequest::EcsRequest> transformBuffer;
                    
                    while (it.next()) {
                        auto transforms = it.field<const Component::WorldTransform>(0);
                        for (int i = 0; i < it.count(); ++i) {
                            auto entity = it.entity(i);
                            transformBuffer.push_back(EcsRequest::CreateTransform{ entity, transforms[i]});
                            entity.remove<Component::TransformGpuDirty>();
                        }
                    }

                    GetChannel().send(transformBuffer);
                });
            


                world.observer<const Component::WorldTransform>()
                    .with<Component::TransformGpu>()
                    .event(flecs::OnRemove)
                    .each([](flecs::entity entity, const Component::WorldTransform& transform){
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

                // PostUpdate: after the propagation system of Module::Transform wrote this frame's WorldTransforms
                world.system<const Component::WorldTransform>()
                .with<Component::TransformGpu>()
                .without<Component::Static>()
                .kind(flecs::PostUpdate)
                .run([](flecs::iter it){
                    Core::GetTransformTripleBuffer().write(
                        [&](std::vector<EcsRequest::UpdateTransform>& transformBuffer){
                            transformBuffer.clear();
                            while (it.next()) {
                                    auto transforms = it.field<const Component::WorldTransform>(0);
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
