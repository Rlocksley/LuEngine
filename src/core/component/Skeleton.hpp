#pragma once

#include "Channel.hpp"
#include "flecs.h"
#include "component/Material.hpp"
#include "component/Transform.hpp"
#include "component/TransformGpu.hpp"

// ============================================================================
//  Skeleton / SkeletonMesh
//
//  parent entity   : Transform + TransformGpu + Skeleton (+ AnimationStateMachine)
//  child entities  : SkeletonMesh + Material       -> deformed by (or attached to) the parent's skeleton
//
//  loadSkeletonModel() builds exactly this as a prefab ("Skeleton::<name>").
//  Attach an extra mesh (a sword) to a node at runtime:
//
//      world.entity().child_of(character)
//           .set<Lu::Component::SkeletonMesh>({ .mesh = swordGeometry, .pipeline = pipe,
//                                               .nodeIndex = Lu::Core::AssetManager::skeletonNode("Hero", "mixamorig:RightHand") })
//           .set<Lu::Component::Material>(...);
// ============================================================================

namespace Lu{
    namespace Component{
        struct Skeleton{
            flecs::entity asset;     // skeleton asset entity ("SkeletonAsset::<name>")
        };

        struct SkeletonMesh{
            flecs::entity mesh;      // geometry registered with a Vertex::Skeleton vertex buffer
            flecs::entity pipeline;  // skeleton pipeline (App::registerSkeletonPipe)
            uint32_t nodeIndex{UINT32_MAX}; // UINT32_MAX = deform by bone weights, else rigidly attach to that node
        };

        struct SkeletonGpuDirty{};
        struct SkeletonMeshGpuDirty{};
    }

    namespace Module{
        struct Skeleton{
            Skeleton(flecs::world& world){
                world.component<Component::Skeleton>();
                world.component<Component::SkeletonMesh>();

                // ---- skeleton instance: create / update / destroy ---------------------
                world.observer<const Component::Skeleton>()
                    .with<Component::Transform>().filter()
                    .with<Component::TransformGpu>().filter()
                    .event(flecs::OnAdd)
                    .event(flecs::OnSet)
                    .each([](flecs::entity e, const Component::Skeleton&){
                        e.add<Component::SkeletonGpuDirty>();
                    });

                // Transform/TransformGpu may arrive after Skeleton
                world.observer()
                    .with<Component::Skeleton>().filter()
                    .with<Component::Transform>()
                    .with<Component::TransformGpu>()
                    .event(flecs::OnAdd)
                    .each([](flecs::entity e){
                        e.add<Component::SkeletonGpuDirty>();
                    });

                world.system<const Component::Skeleton>("EcsRequest::CreateSkeleton System")
                    .with<Component::SkeletonGpuDirty>()
                    .run([](flecs::iter it){
                        std::vector<EcsRequest::EcsRequest> requests;
                        while(it.next()){
                            auto skeletons = it.field<const Component::Skeleton>(0);
                            for(int i = 0; i < it.count(); ++i){
                                auto entity = it.entity(i);
                                requests.push_back(EcsRequest::CreateSkeleton{entity.id(), skeletons[i].asset.id()});
                                entity.remove<Component::SkeletonGpuDirty>();
                            }
                        }
                        if(!requests.empty()) GetChannel().send(std::move(requests));
                    });

                world.observer<const Component::Skeleton>()
                    .with<Component::Transform>()
                    .with<Component::TransformGpu>()
                    .event(flecs::OnRemove)
                    .each([](flecs::entity e, const Component::Skeleton&){
                        GetChannel().send(EcsRequest::DestroySkeleton{e.id()});
                    });

                // ---- skeleton mesh: create / update / destroy -------------------------
                // term order: 0 SkeletonMesh, 1 Material, 2 Transform, 3 TransformGpu, 4 Skeleton (parent)
                world.observer<const Component::SkeletonMesh, const Component::Material>()
                    .with<Component::Transform>().filter()
                    .with<Component::TransformGpu>().filter()
                    .with<Component::Skeleton>().filter()
                    .term_at(2).up()
                    .term_at(3).up()
                    .term_at(4).up()
                    .event(flecs::OnAdd)
                    .event(flecs::OnSet)
                    .each([](flecs::entity e, const Component::SkeletonMesh&, const Component::Material&){
                        e.add<Component::SkeletonMeshGpuDirty>();
                    });

                world.observer()
                    .with<Component::SkeletonMesh>().filter()
                    .with<Component::Material>().filter()
                    .with<Component::Transform>()
                    .with<Component::TransformGpu>()
                    .with<Component::Skeleton>()
                    .term_at(2).up()
                    .term_at(3).up()
                    .term_at(4).up()
                    .event(flecs::OnAdd)
                    .each([](flecs::entity e){
                        e.add<Component::SkeletonMeshGpuDirty>();
                    });

                world.system<const Component::SkeletonMesh, const Component::Material>("EcsRequest::CreateSkeletonMesh System")
                    .with<Component::SkeletonMeshGpuDirty>()
                    .run([](flecs::iter it){
                        std::vector<EcsRequest::EcsRequest> requests;
                        while(it.next()){
                            auto meshes = it.field<const Component::SkeletonMesh>(0);
                            auto materials = it.field<const Component::Material>(1);
                            for(int i = 0; i < it.count(); ++i){
                                auto entity = it.entity(i);
                                requests.push_back(EcsRequest::CreateSkeletonMesh{
                                    entity.parent().id(), entity.id(),
                                    meshes[i].mesh.id(), meshes[i].pipeline.id(),
                                    EcsRequest::Material{materials[i]},
                                    meshes[i].nodeIndex});
                                entity.remove<Component::SkeletonMeshGpuDirty>();
                            }
                        }
                        if(!requests.empty()) GetChannel().send(std::move(requests));
                    });

                world.observer<const Component::SkeletonMesh, const Component::Material>()
                    .with<Component::Transform>()
                    .with<Component::TransformGpu>()
                    .with<Component::Skeleton>()
                    .term_at(2).up()
                    .term_at(3).up()
                    .term_at(4).up()
                    .event(flecs::OnRemove)
                    .each([](flecs::entity e, const Component::SkeletonMesh&, const Component::Material&){
                        GetChannel().send(EcsRequest::DestroySkeletonMesh{e.id()});
                    });
            }
        };
    }
}
