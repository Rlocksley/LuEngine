#pragma once

#include <algorithm>

#include "UIScrollList.hpp"
#include "ClickBox2D.hpp"
#include "component/Clip2D.hpp"
#include "component/Transform2D.hpp"
#include "component/Transform2DGpu.hpp"
#include "component/Mesh2D.hpp"
#include "component/InputState.hpp"

namespace Lu {
namespace Module {

// UIScrollList owns its render, clip and hit-test children (see Component::UIScrollList). The elements are created
// by the user callback `createItem`, so any UI component can be a list element.
struct UIScrollList {
    explicit UIScrollList(flecs::world& world) {
        world.component<Component::UIScrollList>();
        world.component<Component::UIScrollListState>();
        world.import<ClickBox2D>();

        world.observer<const Component::UIScrollList, const Component::Transform2D>()
            .event(flecs::OnSet)
            .each([](flecs::entity e, const Component::UIScrollList& list, const Component::Transform2D& transform) {
                e.children([](flecs::entity child) { child.destruct(); });
                flecs::world w = e.world();

                const float maxScroll = list.maxScroll();
                const float scroll = std::clamp(list.scroll, 0.f, maxScroll);
                const bool showBar = list.scrollbar && maxScroll > 0.f;
                const float barWidth = showBar ? list.scrollbarWidth : 0.f;
                const glm::vec2 itemSize{
                    std::max(0.f, list.size.x - 2.f * list.padding - (showBar ? barWidth + 4.f : 0.f)),
                    list.itemHeight};

                Component::UIScrollListState state;
                state.appliedScroll = scroll;

                // Optional background.
                if (list.mesh.id() != 0 && list.meshPipeline.id() != 0 && list.backgroundColor.a > 0.f) {
                    auto background = w.entity().set(flecs::Parent{e})
                        .set<Component::Transform2D>({.positionZ=0, .scale=list.size})
                        .add<Component::Transform2DGpu>();
                    w.entity().set(flecs::Parent{background}).set(Component::Mesh2D{
                        .mesh=list.mesh, .pipeline=list.meshPipeline,
                        .color=list.backgroundColor, .params=list.backgroundParams});
                }

                // Clip rectangle in world pixels. It is kept up to date by the "UIScrollList update" system.
                glm::vec2 clipPosition = transform.position;
                glm::vec2 clipScale = transform.scale;
                if (const auto* world2D = e.try_get<Component::WorldTransform2D>()) {
                    clipPosition = world2D->position;
                    clipScale = world2D->scale;
                }
                state.appliedClip = Component::Clip2D{clipPosition, list.size * clipScale};
                state.viewport = w.entity().set(flecs::Parent{e}).set<Component::Clip2D>(state.appliedClip);

                state.content = w.entity().set(flecs::Parent{state.viewport})
                    .set<Component::Transform2D>({.position={list.padding, list.padding - scroll}, .positionZ=2})
                    .add<Component::Transform2DGpu>();

                if (list.createItem) {
                    for (uint32_t i = 0; i < list.itemCount; ++i) {
                        auto slot = w.entity().set(flecs::Parent{state.content})
                            .set<Component::Transform2D>({
                                .position={0.f, static_cast<float>(i) * (list.itemHeight + list.spacing)},
                                .positionZ=1})
                            .add<Component::Transform2DGpu>();
                        list.createItem(slot, i, itemSize);
                    }
                }

                if (showBar && list.mesh.id() != 0 && list.meshPipeline.id() != 0) {
                    const float x = list.size.x - barWidth;
                    auto track = w.entity().set(flecs::Parent{e})
                        .set<Component::Transform2D>({.position={x, 0.f}, .positionZ=40, .scale={barWidth, list.size.y}})
                        .add<Component::Transform2DGpu>();
                    w.entity().set(flecs::Parent{track}).set(Component::Mesh2D{
                        .mesh=list.mesh, .pipeline=list.meshPipeline,
                        .color=list.trackColor, .params=list.trackParams});

                    state.thumbHeight = std::clamp(list.size.y * list.size.y / list.contentHeight(),
                                                   list.minThumbHeight, list.size.y);
                    const float travel = list.size.y - state.thumbHeight;
                    state.thumb = w.entity().set(flecs::Parent{e})
                        .set<Component::Transform2D>({.position={x, travel * scroll / maxScroll}, .positionZ=41})
                        .add<Component::Transform2DGpu>();
                    auto visual = w.entity().set(flecs::Parent{state.thumb})
                        .set<Component::Transform2D>({.scale={barWidth, state.thumbHeight}})
                        .add<Component::Transform2DGpu>();
                    state.thumbMesh = w.entity().set(flecs::Parent{visual}).set(Component::Mesh2D{
                        .mesh=list.mesh, .pipeline=list.meshPipeline,
                        .color=list.thumbColor, .params=list.thumbParams});
                    state.appliedThumbColor = list.thumbColor;
                    state.thumbHit = w.entity().set(flecs::Parent{state.thumb})
                        .set<Component::ClickBox2D>({.size={barWidth, state.thumbHeight}, .enabled=list.enabled});
                }

                e.set<Component::UIScrollListState>(state);
            });

        // Wheel, thumb dragging and clamping change `scroll`; every frame the result is applied to the content,
        // the thumb and the Clip2D (the list may have been moved). Runs after "Update ClickBox2D".
        world.system<const Component::WorldTransform2D, Component::UIScrollList,
                     Component::UIScrollListState, const Component::InputState>("UIScrollList update")
            .each([](flecs::entity e, const Component::WorldTransform2D& world2D, Component::UIScrollList& list,
                     Component::UIScrollListState& state, const Component::InputState& input) {
                const float maxScroll = list.maxScroll();
                float scroll = list.scroll;

                if (list.enabled) {
                    // Mouse wheel while the cursor is over the list rectangle.
                    const float wheel = input.scroll.deltaScroll.y;
                    if (wheel != 0.f) {
                        const glm::vec2 local = glm::vec2(glm::inverse(world2D.getMatrix()) *
                            glm::vec4(input.getCursorPosition(), 0.f, 1.f));
                        if (local.x >= 0.f && local.y >= 0.f && local.x <= list.size.x && local.y <= list.size.y)
                            scroll -= wheel * list.wheelSpeed;
                    }

                    // Thumb dragging: ClickBox2D of the thumb is pressed, the cursor moves the content proportionally.
                    const auto* hit = state.thumbHit.id() ? state.thumbHit.try_get<Component::ClickBox2D>() : nullptr;
                    const float travel = list.size.y - state.thumbHeight;
                    state.dragging = hit && hit->pressed;
                    if (state.dragging && travel > 0.f) {
                        const float dy = input.getCursorDeltaPosition().y / std::max(world2D.scale.y, 1e-4f);
                        scroll += dy * maxScroll / travel;
                    }
                }
                scroll = std::clamp(scroll, 0.f, maxScroll);
                list.scroll = scroll;

                if (scroll != state.appliedScroll) {
                    state.appliedScroll = scroll;
                    if (state.content.is_alive()) {
                        auto t = state.content.get<Component::Transform2D>();
                        t.position.y = list.padding - scroll;
                        state.content.set<Component::Transform2D>(t);
                    }
                    if (state.thumb.id() && state.thumb.is_alive()) {
                        auto t = state.thumb.get<Component::Transform2D>();
                        t.position.y = maxScroll > 0.f ? (list.size.y - state.thumbHeight) * scroll / maxScroll : 0.f;
                        state.thumb.set<Component::Transform2D>(t);
                    }
                    if (list.onScroll) list.onScroll(e, scroll);
                }

                // Thumb color: idle / hovered / dragged.
                if (state.thumbMesh.id() && state.thumbMesh.is_alive()) {
                    const auto* hit = state.thumbHit.try_get<Component::ClickBox2D>();
                    const glm::vec4 color = (hit && hit->pressed) ? list.thumbActiveColor
                        : (hit && hit->hovered) ? list.thumbHoverColor : list.thumbColor;
                    if (color != state.appliedThumbColor) {
                        state.appliedThumbColor = color;
                        auto mesh = state.thumbMesh.get<Component::Mesh2D>();
                        mesh.color = color;
                        state.thumbMesh.set<Component::Mesh2D>(mesh);
                    }
                }

                // Clip2D lives in world pixels.
                const Component::Clip2D clip{world2D.position, list.size * world2D.scale};
                if (state.viewport.is_alive() && (clip.position != state.appliedClip.position ||
                                                  clip.size != state.appliedClip.size)) {
                    state.appliedClip = clip;
                    state.viewport.set<Component::Clip2D>(clip);
                }
            });
    }
};

}
}
