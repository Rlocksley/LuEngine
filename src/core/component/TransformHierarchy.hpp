#pragma once

#include "Global.hpp"
#include "flecs.h"

#include <vector>

namespace Lu {
    namespace Component {
        namespace Detail {

            // Shared implementation of the Transform / WorldTransform (and Transform2D / WorldTransform2D) hierarchy.
            //
            //   Local : the component the API user sets (Transform, Transform2D)
            //   World : the derived component (WorldTransform, WorldTransform2D), maintained automatically
            //
            // Both need, in namespace Lu::Component (found by ADL):
            //      World composeWorld(const World& parentWorld, const Local& local);   // parentWorld * local
            // and World must be constructible from a Local (= world of an entity without a parent).
            //
            // The "parent" of an entity is its NEAREST ANCESTOR that has a Local. Entities between them that have no
            // Local are transparent (e.g. Root(Transform) -> Group(no Transform) -> Child(Transform) works).
            //
            // The world of an entity is ALWAYS computed from the Locals of its ancestors (never from a WorldTransform
            // that is maybe not there yet), folded from the root down: ((root * a) * b) * entity. This is the same
            // association the per-frame cascade system uses (parentWorld * local), so both give the same result and it
            // does not matter in which order components / parents were added.
            template<class Local, class World>
            struct TransformHierarchy {

                // Describes a hierarchy change that is in flight while an observer runs (observers fire BEFORE the
                // component / pair is really removed).
                struct Change {
                    flecs::entity_t skip{0};   // an entity whose Local is being removed: ignore it as an ancestor
                    flecs::entity_t cut{0};    // an entity that is being detached: ignore its parent
                };

                // Nearest ancestor of `e` (not `e` itself) that has a Local. Invalid entity (id 0) if there is none.
                static flecs::entity findParent(flecs::entity e, const Change& change = {}) {
                    if (e.id() == change.cut) return flecs::entity();
                    flecs::entity ancestor = e.parent();
                    while (ancestor.id() != 0) {
                        if (ancestor.id() != change.skip && ancestor.has<Local>()) return ancestor;
                        if (ancestor.id() == change.cut) return flecs::entity();
                        ancestor = ancestor.parent();
                    }
                    return flecs::entity();
                }

                // World transform of `e` (which must have a Local) computed from its ancestors' Locals.
                static World computeWorld(flecs::entity e, const Change& change = {}) {
                    std::vector<flecs::entity> chain;        // e, parent, grandparent, ... root
                    chain.push_back(e);
                    for (flecs::entity p = findParent(e, change); p.id() != 0; p = findParent(p, change)) {
                        chain.push_back(p);
                    }
                    World world(chain.back().get<Local>());
                    for (size_t i = chain.size() - 1; i-- > 0;) {
                        world = composeWorld(world, chain[i].get<Local>());
                    }
                    return world;
                }

                static std::vector<flecs::entity> collectChildren(flecs::entity parent) {
                    std::vector<flecs::entity> children;
                    parent.children([&](flecs::entity child) { children.push_back(child); });
                    return children;
                }

                // `parentWorld` is the world transform of the nearest Local-carrying ancestor of all entities below
                // `parent`. Writes the WorldTransform of every Local-carrying descendant whose nearest ancestor is
                // `parent` (or a transparent entity below it) and recurses.
                static void propagate(flecs::entity parent, const World& parentWorld, const Change& change = {}) {
                    for (flecs::entity child : collectChildren(parent)) {
                        if (!child.is_alive()) continue;
                        if (child.id() != change.skip && child.has<Local>()) {
                            const World childWorld = composeWorld(parentWorld, child.get<Local>());
                            child.set<World>(childWorld);
                            propagate(child, childWorld, change);
                        } else {
                            propagate(child, parentWorld, change);
                        }
                    }
                }

                // Recomputes `e` and everything below it. If `e` has no Local it is transparent: every Local-carrying
                // descendant is recomputed from its own ancestors (this is what a re-parenting of a transparent entity
                // needs).
                static void refresh(flecs::entity e, const Change& change = {}) {
                    if (!e.is_alive()) return;
                    if (e.id() != change.skip && e.has<Local>()) {
                        const World world = computeWorld(e, change);
                        e.set<World>(world);
                        propagate(e, world, change);
                    } else {
                        for (flecs::entity child : collectChildren(e)) {
                            refresh(child, change);
                        }
                    }
                }

                static void install(flecs::world& world) {
                    world.component<Local>();
                    world.component<World>();

                    // ---- Local added / set: (re)compute its world, then update all descendants ----------------------
                    world.observer<const Local>()
                        .event(flecs::OnAdd)
                        .event(flecs::OnSet)
                        .each([](flecs::entity e, const Local&) {
                            refresh(e);
                        });

                    // ---- Local removed: the WorldTransform goes with it, the children now see the next ancestor ----
                    world.observer<const Local>()
                        .event(flecs::OnRemove)
                        .each([](flecs::entity e, const Local&) {
                            if (ecs_is_fini(e.world().c_ptr())) return;
                            e.remove<World>();
                            Change change;
                            change.skip = e.id();
                            for (flecs::entity child : collectChildren(e)) {
                                refresh(child, change);
                            }
                        });

                    // ---- (re)parenting: attach / detach a subtree anywhere ------------------------------------------
                    world.observer()
                        .with(flecs::ChildOf, flecs::Wildcard)
                        .event(flecs::OnAdd)
                        .each([](flecs::entity e) {
                            refresh(e);
                        });

                    world.observer()
                        .with(flecs::ChildOf, flecs::Wildcard)
                        .event(flecs::OnRemove)
                        .each([](flecs::entity e) {
                            if (ecs_is_fini(e.world().c_ptr())) return;
                            Change change;
                            change.cut = e.id();
                            refresh(e, change);
                        });

                    // ---- every frame: all entities that are not Static -----------------------------------------------
                    // The parent (nearest ancestor with a WorldTransform, found by .cascade() which also orders the
                    // iteration parents-first) may be Static, the iterated entity may not.
                    world.system<const Local, World, const World*>("Transform Propagation")
                        .term_at(2).cascade()
                        .template without<Static>()
                        .kind(flecs::PostUpdate)
                        .each([](const Local& local, World& world, const World* parentWorld) {
                            world = parentWorld ? composeWorld(*parentWorld, local) : World(local);
                        });
                }
            };
        }
    }
}
