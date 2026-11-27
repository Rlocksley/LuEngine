#pragma once

#include "Global.hpp"
#include "component/Transform2D.hpp"
#include "component/Clip2D.hpp"
#include "component/InputState.hpp"

namespace Lu {
namespace Component {

// Hit rectangle in the local pixel space of the nearest Transform2D ancestor.
struct ClickBox2D {
    glm::vec2 position{0.f};
    glm::vec2 size{0.f};
    bool enabled{true};
    bool hovered{false};
    bool pressed{false};
};

struct ClickBox2DHoveredEnter {};
struct ClickBox2DHoveredExit {};
struct ClickBox2DHovered {};
struct ClickBox2DClicked {};
struct ClickBox2DPressed {};
struct ClickBox2DReleased {};

}

namespace Module {
struct ClickBox2D {
    explicit ClickBox2D(flecs::world& world) {
        world.component<Component::ClickBox2D>();

        // Like Mesh2D and Text2D, a ClickBox2D is a leaf whose transform is
        // inherited from its nearest Transform2D ancestor. Each box evaluates itself.
        // If the box has a Clip2D ancestor (e.g. inside a UIScrollList) the part of the box that is
        // clipped away is not clickable / hoverable: the cursor has to be inside the Clip2D too.
        world.system<const Component::WorldTransform2D, Component::ClickBox2D,
                     const Component::InputState, const Component::Clip2D*>("Update ClickBox2D")
            .term_at(0).up()
            .term_at(3).up().optional()
            .kind(flecs::PostUpdate)
            .each([](flecs::entity e, const Component::WorldTransform2D& transform,
                     Component::ClickBox2D& box, const Component::InputState& input,
                     const Component::Clip2D* clip) {
                e.remove<Component::ClickBox2DHoveredEnter>();
                e.remove<Component::ClickBox2DHoveredExit>();
                e.remove<Component::ClickBox2DClicked>();
                e.remove<Component::ClickBox2DReleased>();

                const glm::vec2 mouse = input.getCursorPosition();
                const glm::vec2 local = glm::vec2(glm::inverse(transform.getMatrix()) *
                    glm::vec4(mouse, 0.f, 1.f));
                const bool insideClip = clip == nullptr ||
                    (mouse.x >= clip->position.x && mouse.y >= clip->position.y &&
                     mouse.x <= clip->position.x + clip->size.x && mouse.y <= clip->position.y + clip->size.y);
                const bool hovered = insideClip && box.enabled && box.size.x > 0.f && box.size.y > 0.f &&
                    local.x >= box.position.x && local.y >= box.position.y &&
                    local.x <= box.position.x + box.size.x &&
                    local.y <= box.position.y + box.size.y;

                if (hovered && !box.hovered) e.add<Component::ClickBox2DHoveredEnter>();
                if (!hovered && box.hovered) e.add<Component::ClickBox2DHoveredExit>();
                if (hovered) e.add<Component::ClickBox2DHovered>();
                else e.remove<Component::ClickBox2DHovered>();

                if (input.wasMouseButtonPressedThisFrame(MouseButton::mouseLeft) && hovered) {
                    box.pressed = true;
                    e.add<Component::ClickBox2DPressed>();
                    e.add<Component::ClickBox2DClicked>();
                }
                if (input.wasMouseButtonReleasedThisFrame(MouseButton::mouseLeft)) {
                    if (box.pressed) e.add<Component::ClickBox2DReleased>();
                    box.pressed = false;
                    e.remove<Component::ClickBox2DPressed>();
                }
                if (!box.enabled && box.pressed) {
                    box.pressed = false;
                    e.remove<Component::ClickBox2DPressed>();
                    e.add<Component::ClickBox2DReleased>();
                }
                box.hovered = hovered;
            });
    }
};
}
}
