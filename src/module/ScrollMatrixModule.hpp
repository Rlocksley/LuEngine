#pragma once

#include <algorithm>
#include <cmath>

#include "UIScrollMatrix.hpp"
#include "ClickBox2D.hpp"
#include "component/Clip2D.hpp"
#include "component/Transform2D.hpp"
#include "component/Transform2DGpu.hpp"
#include "component/Mesh2D.hpp"
#include "component/InputState.hpp"

namespace Lu {
namespace Module {

// UIScrollMatrix owns its render, clip and hit-test children (see Component::UIScrollMatrix). The elements are created
// by the user callback `createItem`, so any UI component can be a matrix element.
struct UIScrollMatrix {
    explicit UIScrollMatrix(flecs::world& world) {
        world.component<Component::UIScrollMatrix>();
        world.component<Component::UIScrollMatrixState>();
        world.import<ClickBox2D>();

        world.observer<const Component::UIScrollMatrix, const Component::Transform2D>()
            .event(flecs::OnSet)
            .each([](flecs::entity e, const Component::UIScrollMatrix& matrix, const Component::Transform2D& transform) {
                // The observer owns the row entities: discard the old hierarchy and rebuild slots from the updated matrix.
                e.children([](flecs::entity child) { child.destruct(); });
                flecs::world w = e.world();

                const float maxScroll = matrix.maxScroll();
                const float scroll = std::clamp(matrix.scroll, 0.f, maxScroll);
                const bool showBar = matrix.scrollbar && maxScroll > 0.f;
                const float barWidth = showBar ? matrix.scrollbarWidth : 0.f;
                const uint32_t columns = matrix.columnCount();
                const glm::vec2 itemSize = matrix.itemSize;

                Component::UIScrollMatrixState state;
                state.appliedScroll = scroll;

                // Optional background.
                if (matrix.mesh.id() != 0 && matrix.meshPipeline.id() != 0 && matrix.backgroundColor.a > 0.f) {
                    auto background = w.entity().set(flecs::Parent{e})
                        .set<Component::Transform2D>({.positionZ=0, .scale=matrix.size})
                        .add<Component::Transform2DGpu>();
                    w.entity().set(flecs::Parent{background}).set(Component::Mesh2D{
                        .mesh=matrix.mesh, .pipeline=matrix.meshPipeline,
                        .color=matrix.backgroundColor, .params=matrix.backgroundParams});
                }

                // Clip rectangle in world pixels. It is kept up to date by the "UIScrollMatrix update" system.
                glm::vec2 clipPosition = transform.position;
                glm::vec2 clipScale = transform.scale;
                if (const auto* world2D = e.try_get<Component::WorldTransform2D>()) {
                    clipPosition = world2D->position;
                    clipScale = world2D->scale;
                }
                state.appliedClip = Component::Clip2D{clipPosition, matrix.size * clipScale};
                state.viewport = w.entity().set(flecs::Parent{e}).set<Component::Clip2D>(state.appliedClip);

                state.content = w.entity().set(flecs::Parent{state.viewport})
                    .set<Component::Transform2D>({.position={matrix.padding, matrix.padding - scroll}, .positionZ=2})
                    .add<Component::Transform2DGpu>();

                if (!matrix.items.empty() || matrix.createItem) {
                    const uint32_t itemCount = matrix.elementCount();
                    for (uint32_t k = 0; k < itemCount; ++k) {
                        const uint32_t i = k / columns;   // row
                        const uint32_t j = k % columns;   // column
                        auto slot = w.entity().set(flecs::Parent{state.content})
                            .set<Component::Transform2D>({
                                .position={static_cast<float>(j) * (itemSize.x + matrix.spacing.x),
                                           static_cast<float>(i) * (itemSize.y + matrix.spacing.y)},
                                .positionZ=1})
                            .add<Component::Transform2DGpu>();
                        if (k < matrix.items.size() && matrix.items[k]) matrix.items[k](slot, i, j, itemSize);
                        else if (matrix.createItem) matrix.createItem(slot, i, j, itemSize);
                    }
                }

                if (showBar && matrix.mesh.id() != 0 && matrix.meshPipeline.id() != 0) {
                    const float x = matrix.size.x - barWidth;
                    auto track = w.entity().set(flecs::Parent{e})
                        .set<Component::Transform2D>({.position={x, 0.f}, .positionZ=40, .scale={barWidth, matrix.size.y}})
                        .add<Component::Transform2DGpu>();
                    w.entity().set(flecs::Parent{track}).set(Component::Mesh2D{
                        .mesh=matrix.mesh, .pipeline=matrix.meshPipeline,
                        .color=matrix.trackColor, .params=matrix.trackParams});

                    state.thumbHeight = std::clamp(matrix.size.y * matrix.size.y / matrix.contentHeight(),
                                                   matrix.minThumbHeight, matrix.size.y);
                    const float travel = matrix.size.y - state.thumbHeight;
                    state.thumb = w.entity().set(flecs::Parent{e})
                        .set<Component::Transform2D>({.position={x, travel * scroll / maxScroll}, .positionZ=41})
                        .add<Component::Transform2DGpu>();
                    auto visual = w.entity().set(flecs::Parent{state.thumb})
                        .set<Component::Transform2D>({.scale={barWidth, state.thumbHeight}})
                        .add<Component::Transform2DGpu>();
                    state.thumbMesh = w.entity().set(flecs::Parent{visual}).set(Component::Mesh2D{
                        .mesh=matrix.mesh, .pipeline=matrix.meshPipeline,
                        .color=matrix.thumbColor, .params=matrix.thumbParams});
                    state.appliedThumbColor = matrix.thumbColor;
                    state.thumbHit = w.entity().set(flecs::Parent{state.thumb})
                        .set<Component::ClickBox2D>({.size={barWidth, state.thumbHeight}, .enabled=matrix.enabled});
                }

                e.set<Component::UIScrollMatrixState>(state);
            });

        // Wheel input updates the target scroll; the content eases toward it. Thumb dragging stays direct.
        // Runs after "Update ClickBox2D".
        world.system<const Component::WorldTransform2D, Component::UIScrollMatrix,
                     Component::UIScrollMatrixState, const Component::InputState>("UIScrollMatrix update")
            .each([](flecs::iter& it, size_t row, const Component::WorldTransform2D& world2D, Component::UIScrollMatrix& matrix,
                     Component::UIScrollMatrixState& state, const Component::InputState& input) {
                const flecs::entity e = it.entity(row);
                const float maxScroll = matrix.maxScroll();
                float scroll = matrix.scroll;

                if (matrix.enabled) {
                    // Mouse wheel while the cursor is over the matrix rectangle.
                    const float wheel = input.scroll.deltaScroll.y;
                    if (wheel != 0.f) {
                        const glm::vec2 local = glm::vec2(glm::inverse(world2D.getMatrix()) *
                            glm::vec4(input.getCursorPosition(), 0.f, 1.f));
                        if (local.x >= 0.f && local.y >= 0.f && local.x <= matrix.size.x && local.y <= matrix.size.y)
                            scroll -= wheel * matrix.wheelSpeed;
                    }

                    // Thumb dragging: ClickBox2D of the thumb is pressed, the cursor moves the content proportionally.
                    const auto* hit = state.thumbHit.id() ? state.thumbHit.try_get<Component::ClickBox2D>() : nullptr;
                    const float travel = matrix.size.y - state.thumbHeight;
                    state.dragging = hit && hit->pressed;
                    if (state.dragging && travel > 0.f) {
                        const float dy = input.getCursorDeltaPosition().y / std::max(world2D.scale.y, 1e-4f);
                        scroll += dy * maxScroll / travel;
                    }
                }
                scroll = std::clamp(scroll, 0.f, maxScroll);
                matrix.scroll = scroll;

                // Exponential easing is frame-rate independent and converges smoothly without overshooting.
                float visualScroll = scroll;
                if (!state.dragging && matrix.scrollSmoothTime > 0.f) {
                    const float dt = std::clamp(it.delta_time(), 0.f, 0.1f);
                    const float alpha = 1.f - std::exp(-dt / matrix.scrollSmoothTime);
                    visualScroll = state.appliedScroll + (scroll - state.appliedScroll) * alpha;
                }
                visualScroll = std::clamp(visualScroll, 0.f, maxScroll);

                if (visualScroll != state.appliedScroll) {
                    state.appliedScroll = visualScroll;
                    if (state.content.is_alive()) {
                        auto t = state.content.get<Component::Transform2D>();
                        t.position.y = matrix.padding - visualScroll;
                        state.content.set<Component::Transform2D>(t);
                    }
                    if (state.thumb.id() && state.thumb.is_alive()) {
                        auto t = state.thumb.get<Component::Transform2D>();
                        t.position.y = maxScroll > 0.f ? (matrix.size.y - state.thumbHeight) * visualScroll / maxScroll : 0.f;
                        state.thumb.set<Component::Transform2D>(t);
                    }
                    if (matrix.onScroll) matrix.onScroll(e, visualScroll);
                }

                // Thumb color: idle / hovered / dragged.
                if (state.thumbMesh.id() && state.thumbMesh.is_alive()) {
                    const auto* hit = state.thumbHit.try_get<Component::ClickBox2D>();
                    const glm::vec4 color = (hit && hit->pressed) ? matrix.thumbActiveColor
                        : (hit && hit->hovered) ? matrix.thumbHoverColor : matrix.thumbColor;
                    if (color != state.appliedThumbColor) {
                        state.appliedThumbColor = color;
                        auto mesh = state.thumbMesh.get<Component::Mesh2D>();
                        mesh.color = color;
                        state.thumbMesh.set<Component::Mesh2D>(mesh);
                    }
                }

                // Clip2D lives in world pixels.
                const Component::Clip2D clip{world2D.position, matrix.size * world2D.scale};
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
