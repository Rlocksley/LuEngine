#pragma once
#include "Global.hpp"
#include "flecs.h"
#include "Channel.hpp"
#include "component/Clip2D.hpp"

namespace Lu::Module {
    struct Clip2D {
        explicit Clip2D(flecs::world& world) {
            world.component<Component::Clip2D>();
            world.observer<const Component::Clip2D>()
                .event(flecs::OnAdd).event(flecs::OnSet)
                .each([](flecs::entity e, const Component::Clip2D& clip) {
                    GetChannel().send(EcsRequest::CreateClip2D{e.id(), clip});
                });
            world.observer<const Component::Clip2D>()
                .event(flecs::OnRemove)
                .each([](flecs::entity e, const Component::Clip2D&) {
                    GetChannel().send(EcsRequest::DestroyClip2D{e.id()});
                });
        }
    };
}
