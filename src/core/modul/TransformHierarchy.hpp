#pragma once

#include "Global.hpp"
#include "flecs.h"

#include <vector>

namespace Lu {
    namespace Component {
        namespace Detail {

            template<class Local, class World>
            struct TransformHierarchy {

                static void install(flecs::world& world) {
                    world.component<Local>();
                    world.component<World>();

                    // ---- Local added / set: (re)compute its world, then update all descendants ----------------------
                    world.observer<const Local, const World*>()
                        .term_at(1).up()
                        .event(flecs::OnAdd)
                        .event(flecs::OnSet)
                        .each([](flecs::entity e, const Local& local, const World* parentWorld) {
                            e.set<World>(parentWorld ? composeWorld(*parentWorld, local) : World(local));
                        });


                    flecs::query<const Local, World> query = world.query_builder<const Local, World>()
                    .expr("(flecs.core.ChildOf, $parent) || (flecs.core.Parent, $parent)")
                    .build();

                    world.observer<const World>()
                    .event(flecs::OnSet)
                    .each([query](flecs::entity parent, const World& parentWorld){
                        query
                        .iter()
                        .set_var("$parent", parent)
                        .each([parentWorld](flecs::entity child, const Local& local, World& childWorld){
                            childWorld = composeWorld(parentWorld, local);
                            child.modified<World>();
                        });
                    });

                    world.observer<const Local>()
                        .event(flecs::OnRemove)
                        .each([](flecs::entity e, const Local&) {
                            e.remove<World>();
                        });

                    world.system<const Local, World, const World*>("Transform Propagation")
                        .without<Static>()
                        .term_at(2).cascade()
                        .kind(flecs::PostUpdate)
                        .each([](const Local& local, World& childWorld, const World* parentWorld) {
                            childWorld = parentWorld ? composeWorld(*parentWorld, local) : World(local);
                        });
                }
            };
        }
    }
}
