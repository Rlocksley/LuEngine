#pragma once

#include "Global.hpp"
#include "flecs.h"
#include "component/Transform2D.hpp"
#include "component/Mesh2D.hpp"
#include "component/Text.hpp"

#include <array>
#include <functional>
#include <memory>
#include <string>
#include <vector>

// =====================================================================================================================
//  UI core components.
//
//  A UI is a TREE OF ENTITIES. Every widget is an entity with
//
//      Transform2D   position (pixels, LOCAL to the parent widget, origin = top-left corner of the widget)
//      Node          size in pixels (+ layout hints)            <- added automatically if missing
//      <Widget>      Button / Slider / Panel / Window / ...     <- the component that makes it a widget
//
//  Setting (or changing) the widget component is all that is needed: an observer marks the entity dirty and the UI
//  module creates / updates the child entities that draw it (Transform2D + Mesh2D / Text entities for rectangles, knobs,
//  labels, ...; or complete child widgets, e.g. the close button of a Window is a Button entity). Nothing is created
//  by hand and there is no factory function, the hierarchy IS the configuration:
//
//      auto window = world.entity("Settings").set<Transform2D>({.position = {100, 80}})
//                         .set<Node>({.size = {360, 240}})
//                         .set<Window>({.title = "Settings"});
//      world.entity("Volume").child_of(window)
//                         .set<Transform2D>({.position = {20, 60}})
//                         .set<Node>({.size = {240, 24}})
//                         .set<Slider>({.value = 0.7f});
//
//  The module needs these assets (registered with the App, names can be changed through Style):
//      Mesh2D::UIRect     unit square         registerShape2D<Shape2D::Rectangle>("Mesh2D::UIRect", glm::vec2(1.f))
//      Mesh2D::UICircle   circle, diameter 1  registerShape2D<Shape2D::Circle>("Mesh2D::UICircle", 0.5f, 48u, glm::vec2(0.5f))
//                         (both shapes span (0,0)..(1,1): a widget scales them with the Transform2D of the drawing entity)
//      Mesh2DPipe::Basic, Font::Main, FontPipe::Basic
// =====================================================================================================================
namespace Lu {
    namespace UI {

        using Color = glm::vec4;

        // "not set": the value comes from the Style (alpha < 0).
        inline constexpr Color kUnset{0.0f, 0.0f, 0.0f, -1.0f};
        inline bool isSet(const Color& color) { return color.a >= 0.0f; }
        inline Color pick(const Color& own, const Color& fallback) { return isSet(own) ? own : fallback; }

        // Layers used by the visuals of ONE widget: 0 .. kLayerStep-1 (border, fill, mark, text, knob ...).
        // A child widget sits Node::layer (default kLayerStep) above its parent.
        inline constexpr uint32_t kLayerStep = 8;
        // Distance between two windows of the same parent (a window needs room for its nested widgets).
        inline constexpr uint32_t kWindowLayerGap = 64;

        enum class Align : uint8_t { Start, Center, End, Stretch };
        enum class Direction : uint8_t { None, Horizontal, Vertical, Grid };
        enum class Orientation : uint8_t { Horizontal, Vertical };

        struct ColorSet {
            Color normal{kUnset};
            Color hover{kUnset};
            Color pressed{kUnset};
            Color disabled{kUnset};
        };

        // ---- Node ---------------------------------------------------------------------------------------------------
        // Size and layout hints of every widget. Added automatically; the size of a widget that still has size 0 gets the
        // default size of its widget type.
        struct Node {
            glm::vec2 size{0.0f, 0.0f};     // pixels, local (before the Transform2D scale of the hierarchy)
            glm::vec2 margin{0.0f, 0.0f};   // Layout: free space left+right (x) / top+bottom (y) around the node
            float grow{0.0f};               // Layout: share of the free space on the main axis (0 = keep the size)
            int order{0};                   // Layout: children are sorted by order, then by entity id (= usually the creation
                                            // order, but ids can be reused: set `order` when the sequence matters)
            uint32_t layer{kLayerStep};     // Transform2D::positionZ = layer, relative to the parent (see autoLayer)
            bool autoLayer{true};           // false: you manage Transform2D::positionZ yourself
            bool inLayout{true};            // false: the parent's Layout ignores this node (popups, scrollbars, ...)
        };

        // ---- Layout -------------------------------------------------------------------------------------------------
        // Put it on a node: its child nodes are placed by the module (their Transform2D::position / Node::size are
        // owned by the layout then). Containers inside containers work, sizes are computed bottom-up.
        struct Layout {
            Direction direction{Direction::Vertical};
            float spacing{6.0f};
            glm::vec4 padding{8.0f, 8.0f, 8.0f, 8.0f};   // left, top, right, bottom
            Align crossAlign{Align::Start};              // across the direction: Start / Center / End / Stretch
            Align mainAlign{Align::Start};               // along the direction when there is free space (and no grow)
            uint32_t columns{2};                         // Grid
            glm::vec2 cellSize{0.0f, 0.0f};              // Grid, 0 = largest child
            bool autoSize{false};                        // Node::size of this container follows its content
        };

        // ---- Style / theme --------------------------------------------------------------------------------------------
        // Put it on any entity: it is valid for that entity and all its descendants (the nearest ancestor wins).
        // Entities that are not set fall back to the asset names listed at the top of this file.
        struct Style {
            flecs::entity rect{};
            flecs::entity circle{};
            flecs::entity meshPipeline{};
            flecs::entity font{};
            flecs::entity fontPipeline{};

            float fontSize{20.0f};
            float borderWidth{2.0f};

            Color text{0.93f, 0.90f, 0.82f, 1.0f};
            Color textDim{0.62f, 0.62f, 0.60f, 1.0f};
            Color textDisabled{0.42f, 0.42f, 0.42f, 1.0f};
            Color accent{0.85f, 0.70f, 0.30f, 1.0f};
            Color panel{0.05f, 0.07f, 0.11f, 1.0f};
            Color panelBorder{0.72f, 0.60f, 0.34f, 1.0f};
            Color titleBar{0.10f, 0.14f, 0.22f, 1.0f};
            Color track{0.02f, 0.03f, 0.05f, 1.0f};
            Color knob{0.92f, 0.86f, 0.68f, 1.0f};
            ColorSet button{{0.12f, 0.19f, 0.28f, 1.0f}, {0.18f, 0.28f, 0.40f, 1.0f},
                            {0.07f, 0.12f, 0.19f, 1.0f}, {0.09f, 0.10f, 0.12f, 1.0f}};
            ColorSet input{{0.03f, 0.05f, 0.08f, 1.0f}, {0.06f, 0.09f, 0.14f, 1.0f},
                           {0.09f, 0.13f, 0.21f, 1.0f}, {0.05f, 0.05f, 0.06f, 1.0f}};
        };

        // ---- Interaction ----------------------------------------------------------------------------------------------
        // Makes a node a target of the mouse. Added automatically by interactive widgets, add it yourself to any node
        // (a Panel, an Image, ...) to make it clickable / to stop clicks from reaching nodes below it. The top-most
        // node (highest world layer) under the cursor gets the mouse; the state is written by the module.
        struct Interact {
            bool enabled{true};
            bool hovered{false};
            bool pressed{false};        // mouse button went down on this node and is still down (also outside of it)
            bool justPressed{false};    // 1 frame
            bool justReleased{false};   // 1 frame
            bool clicked{false};        // 1 frame: pressed and released on this node
            bool rightClicked{false};   // 1 frame
            glm::vec2 cursor{0.0f};     // cursor in local pixels of the node (0..size when inside)
            std::function<void(flecs::entity)> onClick{};
            std::function<void(flecs::entity)> onRightClick{};
            std::function<void(flecs::entity, bool)> onHover{};
        };

        // Moves `target` (default: the parent of this entity) with the mouse while this node is held. The title bar of a Window
        // is such a node. `bounds` = minX, minY, maxX, maxY of the target position in the local space of its parent
        // (ignored if maxX <= minX).
        struct Draggable {
            flecs::entity target{};
            glm::vec4 bounds{0.0f};
        };

        // Tags -----------------------------------------------------------------------------------------------------------
        struct Hidden {};       // this widget and all its descendants are not drawn and not clickable
        struct Modal {};        // while visible only this node and its descendants receive the mouse
        struct Focusable {};    // a click on the node gives it the keyboard focus (TextInput has it)
        struct Clipped {};      // set by ScrollView on children outside of the viewport
        struct Inactive {};     // set by TabView on pages that are not selected
        struct Dirty {};        // widget component / size / state changed: the module redraws it

        // ---- Pointer ----------------------------------------------------------------------------------------------------
        // World state of the mouse over the UI (singleton component PointerRef). Use it to not click "through" the UI:
        //      if (!Lu::UI::Pointer::of(world).overUI) { /* click on the game world */ }
        struct Pointer {
            flecs::entity_t hovered{0};     // top-most Interact node under the cursor
            flecs::entity_t pressed{0};     // node that captured the left mouse button
            flecs::entity_t focused{0};     // keyboard focus
            flecs::entity_t rightPressed{0};
            bool overUI{false};
            bool justPressed{false};
            bool justReleased{false};
            glm::vec2 cursor{0.0f};
            float hoverTime{0.0f};
            flecs::entity_t tooltipTarget{0};
            flecs::entity_t tooltip{0};

            static Pointer& of(flecs::world& world);
        };

        struct PointerRef {
            std::shared_ptr<Pointer> state = std::make_shared<Pointer>();
        };

        inline Pointer& Pointer::of(flecs::world& world) {
            return *world.get<PointerRef>().state;
        }

        // ---- internal: the entities that draw a widget ------------------------------------------------------------------
        enum class Slot : uint8_t {
            PanelBorder, PanelFill, LabelText, ImageRect,
            ButtonBorder, ButtonFill, ButtonText,
            ToggleBorder, ToggleFill, ToggleMark, ToggleText,
            SliderBorder, SliderTrack, SliderFill, SliderKnob,
            ProgressBorder, ProgressTrack, ProgressFill, ProgressText,
            InputBorder, InputFill, InputText,
            DropdownBorder, DropdownFill, DropdownText, DropdownArrow,
            WindowTitleBar, WindowTitleText,
            Count
        };
        inline constexpr size_t kSlotCount = static_cast<size_t>(Slot::Count);

        enum class Child : uint8_t { WindowClose, WindowDrag, ScrollBar, Count };

        // Written by the module only. Every part is two entities: `leaf` (Transform2D + Transform2DGpu, child of the widget)
        // and below it the entity with the Mesh2D / Text.
        struct Parts {
            std::array<flecs::entity_t, kSlotCount * 2> part{};
            std::array<flecs::entity_t, static_cast<size_t>(Child::Count)> child{};
            std::vector<flecs::entity_t> list{};   // dropdown options / tab buttons
            glm::vec2 labelEstimate{0.0f};         // size a Label with autoSize gave its Node last time
        };
    }

    namespace Component {
        using UINode = Lu::UI::Node;
    }
}
