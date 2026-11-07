#pragma once

#include "flecs.h"
#include "component/Transform2D.hpp"
#include "TransformHierarchy.hpp"
namespace Lu{
    namespace Module {
        struct Transform2D {
            Transform2D(flecs::world& world) {
                Component::Detail::TransformHierarchy<Component::Transform2D, Component::WorldTransform2D>::install(world);
            }
        };
    }
}