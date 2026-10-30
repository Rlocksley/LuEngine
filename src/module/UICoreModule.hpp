#pragma once

#include "UIDetail.hpp"

#include <unordered_map>

namespace Lu {
    namespace UI {
        namespace Detail {

            // =====================================================================================================
            //  Dirty tracking: everything that changes how a widget looks marks it (or its subtree) Dirty.
            // =====================================================================================================
            inline void installDirtyTracking(flecs::world& world) {
                world.component<Node>();
                world.component<Layout>();
                world.component<Style>();
                world.component<Interact>();
                world.component<Draggable>();
                world.component<Hidden>();
                world.component<Modal>();
                world.component<Focusable>();
                world.component<Clipped>();
                world.component<Inactive>();
                world.component<Dirty>();
                world.component<Parts>();
                world.component<PointerRef>().add(flecs::Singleton);
                world.set<PointerRef>({});

                world.observer<const Node>()
                    .event(flecs::OnAdd).event(flecs::OnSet)
                    .each([](flecs::entity e, const Node&) { markDirty(e); });

                world.observer<const Interact>()
                    .event(flecs::OnSet)
                    .each([](flecs::entity e, const Interact&) { markDirty(e); });

                world.observer<const Style>()
                    .event(flecs::OnAdd).event(flecs::OnSet)
                    .each([](flecs::entity e, const Style&) { markSubtreeDirty(e); });

                // visibility of a whole subtree
                world.observer().with<Hidden>().event(flecs::OnAdd).event(flecs::OnRemove)
                    .each([](flecs::entity e) { markSubtreeDirty(e); });
                world.observer().with<Clipped>().event(flecs::OnAdd).event(flecs::OnRemove)
                    .each([](flecs::entity e) { markSubtreeDirty(e); });
                world.observer().with<Inactive>().event(flecs::OnAdd).event(flecs::OnRemove)
                    .each([](flecs::entity e) { markSubtreeDirty(e); });

                // re-parenting a widget below / out of a hidden or differently styled parent
                world.observer().with<Node>().with(flecs::ChildOf, flecs::Wildcard)
                    .event(flecs::OnAdd)
                    .each([](flecs::entity e) { markSubtreeDirty(e); });
            }

            // =====================================================================================================
            //  Mouse: hover / press / click / focus, top-most node first, modal windows, drag.
            // =====================================================================================================
            inline void installPointer(flecs::world& world, const Phases& phases, std::shared_ptr<Pointer> pointer) {
                world.component<Component::InputState>();

                flecs::query<const Modal, const WorldTransform2D> modalQuery =
                    world.query_builder<const Modal, const WorldTransform2D>().build();

                world.system<const Node, const WorldTransform2D, Interact>("UIPointer")
                    .kind(phases.input)
                    .run([pointer, modalQuery](flecs::iter& it) {
                        const Component::InputState& input = it.world().get<Component::InputState>();
                        const glm::vec2 cursor = input.getCursorPosition();
                        const bool leftPressed = input.wasMouseButtonPressedThisFrame(MouseButton::mouseLeft);
                        const bool leftReleased = input.wasMouseButtonReleasedThisFrame(MouseButton::mouseLeft);
                        const bool rightReleased = input.wasMouseButtonReleasedThisFrame(MouseButton::mouseRight);
                        const bool rightPressed = input.wasMouseButtonPressedThisFrame(MouseButton::mouseRight);

                        struct Candidate {
                            flecs::entity entity;
                            Interact* interact;
                            glm::vec2 local;
                            bool hit;
                        };
                        std::vector<Candidate> candidates;

                        // the top-most visible modal node
                        flecs::entity_t modalId = 0;
                        uint32_t modalLayer = 0;
                        modalQuery.each([&](flecs::entity m, const Modal&, const WorldTransform2D& wt) {
                            if (isHidden(m)) return;
                            if (modalId == 0 || wt.positionZ >= modalLayer) { modalId = m.id(); modalLayer = wt.positionZ; }
                        });

                        flecs::entity_t best = 0;
                        uint32_t bestLayer = 0;
                        size_t bestDepth = 0;
                        while (it.next()) {
                            auto nodes = it.field<const Node>(0);
                            auto worlds = it.field<const WorldTransform2D>(1);
                            auto interacts = it.field<Interact>(2);
                            for (size_t i = 0; i < it.count(); ++i) {
                                flecs::entity e = it.entity(i);
                                Candidate c{e, &interacts[i], glm::vec2(0.0f), false};
                                if (!isHidden(e)) {
                                    const WorldTransform2D& wt = worlds[i];
                                    if (wt.scale.x != 0.0f && wt.scale.y != 0.0f) {
                                        const glm::vec3 d = glm::inverse(wt.rotation) * glm::vec3(cursor - wt.position, 0.0f);
                                        c.local = glm::vec2(d.x / wt.scale.x, d.y / wt.scale.y);
                                        c.hit = c.local.x >= 0.0f && c.local.y >= 0.0f &&
                                                c.local.x <= nodes[i].size.x && c.local.y <= nodes[i].size.y;
                                    }
                                    if (c.hit && modalId != 0 && !isSelfOrDescendant(e, modalId)) c.hit = false;
                                    if (c.hit) {
                                        const size_t depth = depthOf(e);
                                        if (best == 0 || wt.positionZ > bestLayer ||
                                            (wt.positionZ == bestLayer && (depth > bestDepth ||
                                             (depth == bestDepth && e.id() > best)))) {
                                            best = e.id(); bestLayer = wt.positionZ; bestDepth = depth;
                                        }
                                    }
                                }
                                candidates.push_back(c);
                            }
                        }

                        Pointer& p = *pointer;
                        p.cursor = cursor;
                        p.justPressed = leftPressed;
                        p.justReleased = leftReleased;
                        const flecs::entity_t previousHover = p.hovered;
                        p.hovered = best;
                        p.overUI = best != 0;
                        if (p.hovered != previousHover) p.hoverTime = 0.0f; else p.hoverTime += it.delta_time();

                        auto isEnabled = [&](flecs::entity_t id) {
                            for (const Candidate& c : candidates) if (c.entity.id() == id) return c.interact->enabled;
                            return false;
                        };
                        auto hasFocusable = [&](flecs::entity_t id) {
                            for (const Candidate& c : candidates) if (c.entity.id() == id) return c.entity.has<Focusable>();
                            return false;
                        };

                        flecs::entity_t released = 0;
                        if (leftPressed) {
                            p.pressed = (best != 0 && isEnabled(best)) ? best : 0;
                            p.focused = (best != 0 && isEnabled(best) && hasFocusable(best)) ? best : 0;
                        }
                        if (rightPressed) p.rightPressed = (best != 0 && isEnabled(best)) ? best : 0;
                        flecs::entity_t rightReleasedOn = 0;
                        if (leftReleased) { released = p.pressed; }
                        if (rightReleased) { rightReleasedOn = (p.rightPressed == best) ? best : 0; p.rightPressed = 0; }
                        const flecs::entity_t pressedNow = p.pressed;
                        if (leftReleased || !input.isMouseButtonDown(MouseButton::mouseLeft)) p.pressed = 0;

                        for (Candidate& c : candidates) {
                            Interact& in = *c.interact;
                            const Interact before = in;
                            const flecs::entity_t id = c.entity.id();
                            const bool enabled = in.enabled;
                            const bool hovered = enabled && id == best;
                            in.hovered = hovered;
                            in.pressed = enabled && id == pressedNow && (input.isMouseButtonDown(MouseButton::mouseLeft) || leftReleased);
                            in.justPressed = enabled && leftPressed && id == pressedNow;
                            in.justReleased = enabled && leftReleased && id == released;
                            in.clicked = in.justReleased && hovered;
                            in.rightClicked = enabled && rightReleased && id == rightReleasedOn && hovered;
                            if (in.hovered || in.pressed) in.cursor = c.local;
                            // a node that was pressed and released this frame is no longer "pressed"
                            if (leftReleased && id == released) in.pressed = false;
                            const bool changed = before.hovered != in.hovered || before.pressed != in.pressed ||
                                before.justPressed != in.justPressed || before.justReleased != in.justReleased ||
                                before.clicked != in.clicked || before.rightClicked != in.rightClicked;
                            if (changed) c.entity.modified<Interact>();
                            if (before.hovered != in.hovered && in.onHover) in.onHover(c.entity, in.hovered);
                            if (in.clicked && in.onClick) in.onClick(c.entity);
                            if (in.rightClicked && in.onRightClick) in.onRightClick(c.entity);
                        }
                    });

                // ---- Draggable --------------------------------------------------------------------------------------
                world.system<const Draggable, const Interact, const Component::InputState>("UIDraggable")
                    .kind(phases.input)
                    .each([](flecs::entity e, const Draggable& drag, const Interact& in, const Component::InputState& input) {
                        if (!in.pressed || !in.enabled) return;
                        const glm::vec2 delta = input.getCursorDeltaPosition();
                        if (delta == glm::vec2(0.0f)) return;
                        flecs::entity target = drag.target.id() != 0 ? e.world().entity(drag.target.id()) : e.parent();
                        if (target.id() == 0 || !target.is_alive()) return;
                        const Transform2D* have = target.try_get<Transform2D>();
                        if (!have) return;
                        glm::vec2 parentScale(1.0f);
                        flecs::entity parent = target.parent();
                        while (parent.id() != 0 && !parent.has<WorldTransform2D>()) parent = parent.parent();
                        if (parent.id() != 0) parentScale = parent.get<WorldTransform2D>().scale;
                        Transform2D moved = *have;
                        moved.position += glm::vec2(delta.x / (parentScale.x != 0.0f ? parentScale.x : 1.0f),
                                                    delta.y / (parentScale.y != 0.0f ? parentScale.y : 1.0f));
                        if (drag.bounds.z > drag.bounds.x) {
                            moved.position.x = std::clamp(moved.position.x, drag.bounds.x, drag.bounds.z);
                            moved.position.y = std::clamp(moved.position.y, drag.bounds.y, drag.bounds.w);
                        }
                        target.set<Transform2D>(moved);
                    });

                // ---- Tooltip ----------------------------------------------------------------------------------------
                world.component<Tooltip>();
                world.system("UITooltip")
                    .kind(phases.input)
                    .run([pointer](flecs::iter& it) {
                        while (it.next()) {}
                        flecs::world w = it.world();
                        Pointer& p = *pointer;

                        flecs::entity target;
                        for (flecs::entity c = p.hovered != 0 ? w.entity(p.hovered) : flecs::entity(); c.id() != 0; c = c.parent()) {
                            if (c.has<Tooltip>()) { target = c; break; }
                        }
                        const flecs::entity_t targetId = target.id();
                        if (p.tooltip != 0 && (!alive(w, p.tooltip) || targetId != p.tooltipTarget || p.hovered == 0)) {
                            if (alive(w, p.tooltip)) w.entity(p.tooltip).destruct();
                            p.tooltip = 0;
                        }
                        if (targetId == 0) { p.tooltipTarget = 0; return; }
                        const Tooltip& tip = target.get<Tooltip>();
                        if (p.tooltip == 0 && p.hoverTime >= tip.delay && !tip.text.empty()) {
                            const Style style = resolveStyle(target);
                            const float pad = 8.0f;
                            const float textWidth = std::min(tip.maxWidth, estimateWidth(tip.text, style.fontSize) + 2.0f);
                            const float textHeight = estimateHeight(tip.text, style.fontSize, textWidth);
                            Node node;
                            node.size = {textWidth + pad * 2.0f, textHeight + pad * 2.0f};
                            node.layer = 60000;
                            node.inLayout = false;
                            Transform2D t;
                            t.position = p.cursor + glm::vec2(16.0f, 20.0f);
                            flecs::entity popup = w.entity().set<Style>(style).set<Transform2D>(t).set<Node>(node).set<Panel>({});
                            Node labelNode;
                            labelNode.size = {textWidth, textHeight};
                            Transform2D lt;
                            lt.position = {pad, pad};
                            Label label;
                            label.text = tip.text;
                            label.maxWidth = textWidth;
                            label.autoSize = false;
                            label.color = style.text;
                            w.entity().child_of(popup).set<Transform2D>(lt).set<Node>(labelNode).set<Label>(label);
                            p.tooltip = popup.id();
                        }
                        p.tooltipTarget = targetId;
                        if (p.tooltip != 0 && alive(w, p.tooltip)) {
                            flecs::entity popup = w.entity(p.tooltip);
                            // (a popup created this frame is not committed yet: try_get)
                            if (const Transform2D* have = popup.try_get<Transform2D>()) {
                                Transform2D t = *have;
                                const glm::vec2 want = p.cursor + glm::vec2(16.0f, 20.0f);
                                if (t.position != want) { t.position = want; popup.set<Transform2D>(t); }
                            }
                        }
                    });
            }

            // =====================================================================================================
            //  Layout: Transform2D::positionZ from Node::layer, then Layout containers bottom-up.
            // =====================================================================================================
            struct LayoutChild {
                flecs::entity entity;
                Node node;
                glm::vec2 size;
                glm::vec2 outer;
                glm::vec2 position{0.0f};
            };

            inline void layoutContainer(flecs::entity e, std::unordered_map<flecs::entity_t, glm::vec2>& sizes) {
                const Layout layout = e.get<Layout>();
                Node node = e.get<Node>();
                glm::vec2 size = sizes.count(e.id()) ? sizes[e.id()] : node.size;
                const ScrollView* scroll = e.try_get<ScrollView>();

                std::vector<LayoutChild> kids;
                e.children([&](flecs::entity c) {
                    if (c.has<Node>() && c.has<Transform2D>() && !c.has<Hidden>()) {
                        LayoutChild k;
                        k.entity = c;
                        k.node = c.get<Node>();
                        if (!k.node.inLayout) return;
                        k.size = sizes.count(c.id()) ? sizes[c.id()] : k.node.size;
                        kids.push_back(k);
                    }
                });
                std::sort(kids.begin(), kids.end(), [](const LayoutChild& a, const LayoutChild& b) {
                    return a.node.order != b.node.order ? a.node.order < b.node.order : a.entity.id() < b.entity.id();
                });
                for (LayoutChild& k : kids) k.outer = k.size + k.node.margin * 2.0f;

                const glm::vec2 pad0(layout.padding.x, layout.padding.y);
                const glm::vec2 pad1(layout.padding.z, layout.padding.w);
                glm::vec2 contentSize(0.0f);      // size of the content without padding
                const size_t n = kids.size();

                if (layout.direction == Direction::Horizontal || layout.direction == Direction::Vertical) {
                    const int m = layout.direction == Direction::Horizontal ? 0 : 1;   // main axis
                    const int c = 1 - m;                                                // cross axis
                    float sumMain = n > 1 ? layout.spacing * static_cast<float>(n - 1) : 0.0f;
                    float maxCross = 0.0f, totalGrow = 0.0f;
                    for (const LayoutChild& k : kids) { sumMain += k.outer[m]; maxCross = std::max(maxCross, k.outer[c]); totalGrow += k.node.grow; }
                    contentSize[m] = sumMain;
                    contentSize[c] = maxCross;
                    if (layout.autoSize) size = contentSize + pad0 + pad1;
                    const glm::vec2 inner = size - pad0 - pad1;
                    float free = inner[m] - sumMain;
                    if (!layout.autoSize && free > 0.0f && totalGrow > 0.0f) {
                        for (LayoutChild& k : kids) {
                            if (k.node.grow > 0.0f) { k.size[m] += free * k.node.grow / totalGrow; k.outer[m] = k.size[m] + k.node.margin[m] * 2.0f; }
                        }
                        free = 0.0f;
                    }
                    float offset = 0.0f;
                    if (free > 0.0f) offset = layout.mainAlign == Align::Center ? free * 0.5f : layout.mainAlign == Align::End ? free : 0.0f;
                    if (scroll) contentSize[m] = sumMain;
                    float cursorMain = pad0[m] + offset;
                    for (LayoutChild& k : kids) {
                        k.position[m] = cursorMain + k.node.margin[m];
                        const float innerCross = layout.autoSize ? maxCross : inner[c];
                        switch (layout.crossAlign) {
                            case Align::Start:   k.position[c] = pad0[c] + k.node.margin[c]; break;
                            case Align::Center:  k.position[c] = pad0[c] + (innerCross - k.outer[c]) * 0.5f + k.node.margin[c]; break;
                            case Align::End:     k.position[c] = pad0[c] + innerCross - k.outer[c] + k.node.margin[c]; break;
                            case Align::Stretch: k.size[c] = innerCross - k.node.margin[c] * 2.0f; k.position[c] = pad0[c] + k.node.margin[c]; break;
                        }
                        cursorMain += k.outer[m] + layout.spacing;
                    }
                } else if (layout.direction == Direction::Grid) {
                    const size_t columns = std::max<uint32_t>(1, layout.columns);
                    const size_t rows = (n + columns - 1) / columns;
                    std::vector<float> colWidth(columns, 0.0f), rowHeight(rows, 0.0f);
                    for (size_t i = 0; i < n; ++i) {
                        colWidth[i % columns] = std::max(colWidth[i % columns], layout.cellSize.x > 0.0f ? layout.cellSize.x : kids[i].outer.x);
                        rowHeight[i / columns] = std::max(rowHeight[i / columns], layout.cellSize.y > 0.0f ? layout.cellSize.y : kids[i].outer.y);
                    }
                    float totalW = columns > 1 ? layout.spacing * static_cast<float>(columns - 1) : 0.0f;
                    float totalH = rows > 1 ? layout.spacing * static_cast<float>(rows - 1) : 0.0f;
                    for (float w : colWidth) totalW += w;
                    for (float h : rowHeight) totalH += h;
                    contentSize = {totalW, totalH};
                    if (layout.autoSize) size = contentSize + pad0 + pad1;
                    std::vector<float> colX(columns, 0.0f), rowY(rows, 0.0f);
                    float x = pad0.x;
                    for (size_t i = 0; i < columns; ++i) { colX[i] = x; x += colWidth[i] + layout.spacing; }
                    float y = pad0.y;
                    for (size_t i = 0; i < rows; ++i) { rowY[i] = y; y += rowHeight[i] + layout.spacing; }
                    for (size_t i = 0; i < n; ++i) {
                        LayoutChild& k = kids[i];
                        const glm::vec2 cell(colWidth[i % columns], rowHeight[i / columns]);
                        glm::vec2 free = cell - k.outer;
                        glm::vec2 shift(0.0f);
                        if (layout.crossAlign == Align::Center) shift = free * 0.5f;
                        else if (layout.crossAlign == Align::End) shift = free;
                        else if (layout.crossAlign == Align::Stretch) { k.size = cell - k.node.margin * 2.0f; }
                        k.position = glm::vec2(colX[i % columns], rowY[i / columns]) + shift + k.node.margin;
                    }
                } else {
                    return;
                }

                // ScrollView: shift by the scroll offset, clamp it, hide what is outside
                if (scroll) {
                    const float contentHeight = contentSize.y + pad0.y + pad1.y;
                    const float viewport = size.y;
                    const float maxScroll = std::max(0.0f, contentHeight - viewport);
                    const float offset = std::clamp(scroll->scroll, 0.0f, maxScroll);
                    if (offset != scroll->scroll || contentHeight != scroll->contentHeight) {
                        ScrollView sv = *scroll;
                        sv.scroll = offset;
                        sv.contentHeight = contentHeight;
                        e.set<ScrollView>(sv);
                    }
                    for (LayoutChild& k : kids) {
                        k.position.y -= offset;
                        const bool inside = k.position.y >= 0.0f && k.position.y + k.size.y <= viewport + 0.01f;
                        if (inside && k.entity.has<Clipped>()) k.entity.remove<Clipped>();
                        else if (!inside && !k.entity.has<Clipped>()) k.entity.add<Clipped>();
                    }
                }

                for (LayoutChild& k : kids) {
                    const Transform2D& have = k.entity.get<Transform2D>();
                    if (glm::distance(have.position, k.position) > 0.001f) {
                        Transform2D t = have;
                        t.position = k.position;
                        k.entity.set<Transform2D>(t);
                    }
                    if (glm::distance(k.node.size, k.size) > 0.001f) {
                        Node nn = k.node;
                        nn.size = k.size;
                        k.entity.set<Node>(nn);
                        sizes[k.entity.id()] = k.size;
                    }
                }
                if (layout.autoSize && glm::distance(node.size, size) > 0.001f) {
                    node.size = size;
                    e.set<Node>(node);
                    sizes[e.id()] = size;
                }
            }

            inline void installLayout(flecs::world& world, const Phases& phases) {
                world.component<Layout>();
                world.component<ScrollView>();

                world.system<const Node, Transform2D>("UILayer")
                    .kind(phases.layout)
                    .each([](flecs::entity e, const Node& node, Transform2D& transform) {
                        if (node.autoLayer && transform.positionZ != node.layer) {
                            transform.positionZ = node.layer;
                            e.modified<Transform2D>();
                        }
                    });

                world.system<const Layout>("UILayout")
                    .kind(phases.layout)
                    .run([](flecs::iter& it) {
                        std::vector<std::pair<size_t, flecs::entity>> containers;
                        while (it.next()) {
                            for (size_t i = 0; i < it.count(); ++i) {
                                flecs::entity e = it.entity(i);
                                if (e.has<Node>() && !isHidden(e)) containers.emplace_back(depthOf(e), e);
                            }
                        }
                        // children first: a container needs the final size of the containers inside of it
                        std::sort(containers.begin(), containers.end(), [](const auto& a, const auto& b) {
                            return a.first != b.first ? a.first > b.first : a.second.id() < b.second.id();
                        });
                        std::unordered_map<flecs::entity_t, glm::vec2> sizes;
                        for (auto& [depth, e] : containers) layoutContainer(e, sizes);
                    });
            }
        }
    }
}
