#pragma once

#include "component/Transform2D.hpp"
#include "component/Clip2D.hpp"
#include "UICore.hpp"
#include <algorithm>
#include <cstdint>
#include <functional>
#include <utility>
#include <vector>

namespace Lu {
    namespace Component {

        // A vertically scrollable matrix with `columns` elements per row (World of Warcraft style inventory).
        // Same strategy as UIScrollList / UIButton: put the component on an entity that has a Transform2D
        // (+ Transform2DGpu); the module (Module::UIScrollMatrix, src/module/ScrollMatrixModule.hpp) builds the children:
        //
        //      matrix (Transform2D, UIScrollMatrix)
        //       |- background   (optional Mesh2D)
        //       |- viewport     (Clip2D: everything below is clipped to the matrix rectangle)
        //       |   `- content  (Transform2D, moves with `scroll`)
        //       |       |- slot (0,0) (Transform2D + Transform2DGpu)   <- createItem(slot, 0, 0, itemSize)
        //       |       |- slot (0,1) ...
        //       |       |- slot (1,0) ...
        //       |- track / thumb (optional scrollbar, the thumb has a ClickBox2D and can be dragged)
        //
        // Elements are stored row-major: element (i, j) is the j-th element (column) of the i-th row and has the flat
        // index i * columns + j. Like the UIScrollList the matrix is GENERAL: it does not know what its elements are.
        // For every cell the module creates an empty "slot" entity and calls `createItem(slot, i, j, itemSize)`; the
        // callback puts any UI component on the slot (UIButton, a UIScrollList, your own component, Mesh2D / Text2D ...).
        //
        //      .createItem = [&](flecs::entity slot, uint32_t i, uint32_t j, glm::vec2 itemSize) {
        //          slot.set(Component::UIButton{ ..., .size = itemSize });
        //      }
        // For a matrix where each element has its own description, leave createItem empty and use addItem(i, j, callback):
        //      Component::UIScrollMatrix inv{ .size = {260.f, 300.f}, .columns = 4, .itemSize = {52.f, 52.f} };
        //      inv.addItem(0, 0, [](flecs::entity slot, uint32_t i, uint32_t j, glm::vec2 size) { slot.set(MyItem{...}); });
        //      world.entity("inventory").set(inv);
        //
        // Adding / removing an element at (i, j) shifts all later elements (row-major order), like inserting into the
        // UIScrollList does: the module rebuilds the slots, so the elements flow into the following cells. The
        // last row may be incomplete.
        //
        // Because the slots are below a Clip2D entity, every Mesh2D / Text2D of the elements inherits the clip rectangle.
        // Use pipelines that support clipping (shader/mesh2d_clip.*, shader/mesh2d_ui.*, shader/font_clip.*).
        struct UIScrollMatrix {
            glm::vec2 size{260.f, 320.f};   // viewport in pixels (local to the matrix's Transform2D)
            uint32_t columns{4};            // m: elements per row
            uint32_t itemCount{0};          // number of elements when createItem is used (n * m for a full matrix)
            glm::vec2 itemSize{52.f, 52.f}; // size of one element
            glm::vec2 spacing{6.f, 6.f};    // x: between columns, y: between rows
            float padding{6.f};             // space between the border and the elements
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

            using ItemCallback = std::function<void(flecs::entity slot, uint32_t i, uint32_t j, glm::vec2 itemSize)>;

            // Called for every element while the matrix is (re)built. Use this for a uniform/index-driven matrix.
            ItemCallback createItem{};
            // Optional explicit elements in row-major order. When non-empty, these callbacks are used instead of
            // createItem and determine the element count. An empty entry falls back to createItem.
            std::vector<ItemCallback> items{};

            uint32_t columnCount() const { return std::max<uint32_t>(columns, 1u); }
            uint32_t elementCount() const { return items.empty() ? itemCount : static_cast<uint32_t>(items.size()); }
            uint32_t rowCount() const { const uint32_t c = columnCount(); return (elementCount() + c - 1u) / c; }
            // Flat (row-major) index of cell (i, j). j is clamped to the last column.
            uint32_t flatIndex(uint32_t i, uint32_t j) const { return i * columnCount() + std::min(j, columnCount() - 1u); }
            // Row / column of a flat index.
            std::pair<uint32_t, uint32_t> cellOf(uint32_t flat) const { return {flat / columnCount(), flat % columnCount()}; }

            // Inserts an element at (i, j); every later element moves one cell forward. Cells past the end append.
            UIScrollMatrix& addItem(uint32_t i, uint32_t j, ItemCallback describe) {
                // Preserve existing factory-created cells as empty overrides when switching to indexed items.
                if (items.empty() && itemCount != 0) items.resize(itemCount);
                const auto insertAt = items.begin() + std::min<size_t>(flatIndex(i, j), items.size());
                items.insert(insertAt, std::move(describe));
                itemCount = static_cast<uint32_t>(items.size());
                return *this;
            }

            // Removes the element at (i, j); every later element moves one cell back. Out-of-range cells are ignored.
            UIScrollMatrix& removeItem(uint32_t i, uint32_t j) {
                const uint32_t count = elementCount();
                if (j >= columnCount()) return *this;
                const uint32_t flat = flatIndex(i, j);
                if (flat >= count) return *this;

                // Materialize factory cells so erasing one doesn't discard the factory for all the others.
                if (items.empty()) items.resize(count);
                items.erase(items.begin() + flat);
                itemCount = static_cast<uint32_t>(items.size());
                return *this;
            }

            // Called when `scroll` changed (wheel, thumb dragging or code).
            std::function<void(flecs::entity matrix, float scroll)> onScroll{};

            // Width needed by `columns` elements without scrollbar.
            float contentWidth() const {
                const float m = static_cast<float>(columnCount());
                return 2.f * padding + m * itemSize.x + (m - 1.f) * spacing.x;
            }
            float contentHeight() const {
                const uint32_t rows = rowCount();
                const float n = static_cast<float>(rows);
                return 2.f * padding + (rows ? n * itemSize.y + (n - 1.f) * spacing.y : 0.f);
            }
            float maxScroll() const { return std::max(0.f, contentHeight() - size.y); }
        };

        // Written by the module (like the children of a UIButton): entities of the built hierarchy.
        struct UIScrollMatrixState {
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
