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

                    world.observer<const Local, const World*>()
                        .term_at(1).up()
                        .event(flecs::OnAdd)
                        .event(flecs::OnSet)
                        .each([](flecs::entity e, const Local& local, const World* parentWorld) {
                            e.set<World>(parentWorld ? *parentWorld * local : World(local));
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
                            childWorld = parentWorld ? *parentWorld * local : World(local);
                        });
                }
            };
        }
    }
}
