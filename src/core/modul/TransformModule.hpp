#pragma once
#include "flecs.h"
#include "component/Transform.hpp"
#include "TransformHierarchy.hpp"

namespace Lu{
    namespace Module{
        struct Transform{
            // Transform = the transform the API user sets: LOCAL to the parent entity if that has a
            // Transform, WORLD if there is none. WorldTransform is added / updated / removed automatically.
            Transform(flecs::world& world){
                world.component<Component::MeshTransform>();
                Component::Detail::TransformHierarchy<Component::Transform, Component::WorldTransform>::install(world);
            }
        };
    }
}