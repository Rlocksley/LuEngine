#pragma once

#include "component/Transform2D.hpp"
#include "component/Clip2D.hpp"
#include "UICore.hpp"
#include <algorithm>
#include <utility>
#include <vector>

namespace Lu {
    namespace Component {

        // A vertically scrollable list. Same strategy as UIButton: put the component on an entity that has a Transform2D
        // (+ Transform2DGpu); the module (Module::UIScrollList, src/module/ScrollListModule.hpp) builds the child entities:
        //
        //      list (Transform2D, UIScrollList)
        //       |- background   (optional Mesh2D)
        //       |- viewport     (Clip2D: everything below is clipped to the list rectangle)
        //       |   `- content  (Transform2D, moves with `scroll`)
        //       |       |- slot 0 (Transform2D + Transform2DGpu)   <- createItem(slot, 0, itemSize)
        //       |       |- slot 1 ...
        //       |- track / thumb (optional scrollbar, the thumb has a ClickBox2D and can be dragged)
        //
        // The list is GENERAL: it does not know what its elements are. For every index the module creates an empty "slot"
        // entity (it already has a Transform2D at the right place) and calls `createItem(slot, index, itemSize)`. The callback
        // puts the element component on the slot, so any UI component that lives on an entity with a Transform2D can be a
        // list element: UIButton, another UIScrollList (nested lists), your own components, or plain Mesh2D / Text2D children.
        //
        //      .createItem = [&](flecs::entity slot, uint32_t i, glm::vec2 itemSize) {
        //          slot.set(Component::UIButton{ ..., .size = itemSize });
        //      }
        // For a list where each element has its own description, leave createItem empty and call addItem(index, callback):
        //      Component::UIScrollList list{ .size = {240.f, 320.f}, .itemHeight = 48.f };
        //      list.addItem(0, [](flecs::entity slot, uint32_t, glm::vec2 size) { slot.set(MyHeader{...}); });
        //      list.addItem(1, [](flecs::entity slot, uint32_t, glm::vec2 size) { slot.set(MyButton{..., .size=size}); });
        //      world.entity("list").set(list);
        //
        // Because the slots are below a Clip2D entity, every Mesh2D / Text2D of the elements inherits the clip rectangle. Use
        // pipelines that support clipping (shader/mesh2d_clip.*, shader/mesh2d_ui.*, shader/font_clip.*) for the elements.
        // ClickBox2D respects the Clip2D too, so elements that are scrolled out of the list can not be clicked.
        struct UIScrollList {
            glm::vec2 size{240.f, 320.f};   // viewport in pixels (local to the list's Transform2D)
            uint32_t itemCount{0};
            float itemHeight{48.f};
            float spacing{6.f};
            float padding{6.f};             // space between the list border and the elements
            bool enabled{true};

            float scroll{0.f};              // pixels; read / write it any time (it is clamped every frame)
            float wheelSpeed{48.f};         // pixels per mouse wheel notch
            float scrollSmoothTime{0.18f};  // seconds to settle after wheel/code scrolling; 0 disables smoothing

            // Optional visuals. All parts use `mesh` (a unit square: Mesh2D::...) and `meshPipeline`.
            flecs::entity mesh;
            flecs::entity meshPipeline;
            glm::vec4 backgroundColor{0.f, 0.f, 0.f, 0.f};    // alpha 0 = no background
            glm::vec4 backgroundParams{0.f, 0.f, 0.f, 0.f};

            bool scrollbar{true};           // shown only if the content is higher than the viewport
            float scrollbarWidth{8.f};
            float minThumbHeight{24.f};
            glm::vec4 trackColor{1.f, 1.f, 1.f, 0.08f};
            glm::vec4 thumbColor{0.55f, 0.65f, 0.8f, 1.f};
            glm::vec4 thumbHoverColor{0.70f, 0.80f, 0.95f, 1.f};
            glm::vec4 thumbActiveColor{0.85f, 0.92f, 1.f, 1.f};
            glm::vec4 trackParams{0.f, 0.f, 0.f, 0.f};
            glm::vec4 thumbParams{0.f, 0.f, 0.f, 0.f};

            // Called for every element while the list is (re)built. Use this for a uniform/index-driven list.
            std::function<void(flecs::entity slot, uint32_t index, glm::vec2 itemSize)> createItem{};
            // Optional explicit elements. When non-empty, these callbacks are used instead of createItem and determine
            // the list's element count. Each callback can build a different kind of element in its slot.
            std::vector<std::function<void(flecs::entity slot, uint32_t index, glm::vec2 itemSize)>> items{};

            UIScrollList& addItem(uint32_t index,
                                  std::function<void(flecs::entity slot, uint32_t index, glm::vec2 itemSize)> describe) {
                // Preserve existing factory-created rows as empty overrides when switching to indexed items.
                if (items.empty() && itemCount != 0) items.resize(itemCount);
                // Indices past the end append. The module rebuilds slots in order, repositioning every later item.
                const auto insertAt = items.begin() + std::min<size_t>(index, items.size());
                items.insert(insertAt, std::move(describe));
                itemCount = static_cast<uint32_t>(items.size());
                return *this;
            }

            UIScrollList& removeItem(uint32_t index) {
                const uint32_t count = elementCount();
                // Out-of-range indices are ignored. Rebuilt slots compact and reposition the later rows.
                if (index >= count) return *this;

                // Materialize factory rows so erasing one index doesn't discard the factory for every other row.
                if (items.empty()) items.resize(count);
                items.erase(items.begin() + index);
                itemCount = static_cast<uint32_t>(items.size());
                return *this;
            }

            uint32_t elementCount() const { return items.empty() ? itemCount : static_cast<uint32_t>(items.size()); }
            // Called when `scroll` changed (wheel, thumb dragging or code).
            std::function<void(flecs::entity list, float scroll)> onScroll{};

            float contentHeight() const {
                const uint32_t count = elementCount();
                const float n = static_cast<float>(count);
                return 2.f * padding + (count ? n * itemHeight + (n - 1.f) * spacing : 0.f);
            }
            float maxScroll() const { return std::max(0.f, contentHeight() - size.y); }
        };

        // Written by the module (like the children of a UIButton): entities of the built hierarchy.
        struct UIScrollListState {
            flecs::entity viewport;     // has the Clip2D
            flecs::entity content;      // moved by -scroll
            flecs::entity thumb;        // unscaled transform entity, parent of the thumb ClickBox2D
            flecs::entity thumbHit;     // ClickBox2D entity
            flecs::entity thumbMesh;    // leaf with the thumb Mesh2D
            float thumbHeight{0.f};
            float appliedScroll{0.f};
            glm::vec4 appliedThumbColor{0.f};
            Clip2D appliedClip{};
            bool dragging{false};
        };
    }
}
