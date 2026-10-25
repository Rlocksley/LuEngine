#pragma once
#include "Global.hpp"
#include <optional>
#include "flecs.h"
#include "Channel.hpp"
#include "component/Camera2D.hpp"

namespace Lu {
namespace Module {
    // Sends EcsRequest::UpdateCamera2D{Camera2DData} whenever the Camera2D component differs from what
    // was sent last (direct modification of the component through a reference is detected as well).
    struct Camera2D{
        Camera2D(flecs::world& world){
            world.component<Component::Camera2D>();

            auto lastSent = std::make_shared<std::optional<Camera2DData>>();

            world.system<const Component::Camera2D>("EcsRequest::UpdateCamera2D System")
                .each([lastSent](const Component::Camera2D& camera){
                    const Camera2DData data = camera.data();
                    if (lastSent->has_value() && **lastSent == data) return;
                    *lastSent = data;
                    GetChannel().send(EcsRequest::UpdateCamera2D{data});
                });
        }
    };
}
}
