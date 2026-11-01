#pragma once

#include "Global.hpp"
#include "flecs.h"
#include "component/InputState.hpp"
#include "component/Transform2D.hpp"
#include "component/Transform2DGpu.hpp"
#include "component/Mesh2D.hpp"
#include "component/Text.hpp"

#include "UICore.hpp"
#include "UIDisplay.hpp"
#include "Button.hpp"
#include "Toggle.hpp"
#include "Slider.hpp"
#include "TextInput.hpp"
#include "Dropdown.hpp"
#include "ScrollView.hpp"
#include "ScrollBar.hpp"
#include "TabView.hpp"
#include "UIWindow.hpp"

#include <algorithm>
#include <cmath>

// Internal helpers of the UI module (not an API: there is no factory function for widgets, widgets are entities).
namespace Lu {
    namespace UI {
        namespace Detail {

            using Lu::Component::Transform2D;
            using Lu::Component::Transform2DGpu;
            using Lu::Component::WorldTransform2D;
            using Lu::Component::Mesh2D;
            using Lu::Component::Text;

            // The phases of the UI (between PostLoad and PreUpdate): input -> layout -> redraw.
            struct Phases {
                flecs::entity input;
                flecs::entity layout;
                flecs::entity sync;
            };

            // ecs_is_alive asserts on id 0, stored ids are 0 until a part / child exists.
            inline bool alive(flecs::world& w, flecs::entity_t id) { return id != 0 && w.is_alive(id); }

            // The GPU can clip (mesh2d_clip.* / font_clip.* pipelines are registered).
            inline bool canClip(const Style& style) {
                return style.clipMeshPipeline.id() != 0 && style.clipFontPipeline.id() != 0;
            }

            // The clip rectangle entity (a Transform2D + Transform2DGpu that is created by a ScrollView) that clips the
            // parts of `e`: the nearest ScrollView ancestor with clip on, if its clip entity is ready. 0 = no clipping.
            inline flecs::entity_t findClip(flecs::entity e) {
                flecs::world w = e.world();
                for (flecs::entity p = e.parent(); p.id() != 0; p = p.parent()) {
                    const ScrollView* view = p.try_get<ScrollView>();
                    if (!view) continue;
                    if (!view->clip) return 0;
                    const Parts* parts = p.try_get<Parts>();
                    if (!parts) return 0;
                    const flecs::entity_t id = parts->child[static_cast<size_t>(Child::ScrollClip)];
                    if (!alive(w, id)) return 0;
                    flecs::entity clip = w.entity(id);
                    return clip.has<Transform2DGpu>() ? id : 0;
                }
                return 0;
            }

            // ---- text size estimate (the ECS side has no font metrics) ---------------------------------------------------
            inline size_t countCodepoints(const std::string& s) {
                size_t n = 0;
                for (unsigned char c : s) if ((c & 0xC0) != 0x80) ++n;
                return n;
            }
            inline constexpr float kCharWidth = 0.55f;     // average glyph width / font size
            inline constexpr float kLineHeight = 1.25f;    // line box / font size
            inline float estimateWidth(const std::string& s, float fontSize) {
                size_t longest = 0, current = 0;
                for (unsigned char c : s) {
                    if (c == '\n') { longest = std::max(longest, current); current = 0; }
                    else if ((c & 0xC0) != 0x80) ++current;
                }
                return static_cast<float>(std::max(longest, current)) * fontSize * kCharWidth;
            }
            inline float estimateHeight(const std::string& s, float fontSize, float maxWidth) {
                size_t lines = 1;
                for (char c : s) if (c == '\n') ++lines;
                if (maxWidth > 0.0f) {
                    const float perLine = std::max(1.0f, std::floor(maxWidth / (fontSize * kCharWidth)));
                    lines = 0;
                    size_t start = 0;
                    while (start <= s.size()) {
                        size_t end = s.find('\n', start);
                        if (end == std::string::npos) end = s.size();
                        const size_t n = countCodepoints(s.substr(start, end - start));
                        lines += std::max<size_t>(1, static_cast<size_t>(std::ceil(static_cast<float>(n) / perLine)));
                        start = end + 1;
                    }
                }
                return static_cast<float>(lines) * fontSize * kLineHeight;
            }

            // ---- hierarchy ----------------------------------------------------------------------------------------------
            // The nearest Node ancestor (invalid entity if there is none).
            inline flecs::entity nodeParent(flecs::entity e) {
                for (flecs::entity p = e.parent(); p.id() != 0; p = p.parent()) {
                    if (p.has<Node>()) return p;
                }
                return flecs::entity();
            }

            inline bool isSelfHidden(flecs::entity e) {
                return e.has<Hidden>() || e.has<Clipped>() || e.has<Inactive>();
            }

            // Hidden, Clipped or Inactive on the entity or on any ancestor.
            inline bool isHidden(flecs::entity e) {
                for (flecs::entity p = e; p.id() != 0; p = p.parent()) {
                    if (isSelfHidden(p)) return true;
                }
                return false;
            }

            inline bool isSelfOrDescendant(flecs::entity e, flecs::entity_t ancestor) {
                if (ancestor == 0) return false;
                for (flecs::entity p = e; p.id() != 0; p = p.parent()) {
                    if (p.id() == ancestor) return true;
                }
                return false;
            }

            inline size_t depthOf(flecs::entity e) {
                size_t depth = 0;
                for (flecs::entity p = e.parent(); p.id() != 0; p = p.parent()) ++depth;
                return depth;
            }

            inline void markDirty(flecs::entity e) { e.add<Dirty>(); }

            inline void markSubtreeDirty(flecs::entity e) {
                e.add<Dirty>();
                std::vector<flecs::entity> children;
                e.children([&](flecs::entity c) { children.push_back(c); });
                for (flecs::entity c : children) markSubtreeDirty(c);
            }

            // Every widget needs a Transform2D, a Node and (interactive ones) an Interact. `add` only: it never overwrites
            // what the user set, in whatever order the components were added.
            inline void ensureBase(flecs::entity e, bool interactive, bool focusable) {
                e.add<Transform2D>();
                e.add<Node>();
                if (interactive) e.add<Interact>();
                if (focusable) e.add<Focusable>();
                e.add<Dirty>();
            }

            template<class Widget>
            void watchWidget(flecs::world& world, bool interactive = false, bool focusable = false) {
                world.component<Widget>();
                world.observer<const Widget>()
                    .event(flecs::OnAdd)
                    .event(flecs::OnSet)
                    .each([interactive, focusable](flecs::entity e, const Widget&) {
                        ensureBase(e, interactive, focusable);
                    });
            }

            // ---- style --------------------------------------------------------------------------------------------------
            inline Style resolveStyle(flecs::entity e) {
                Style style;
                for (flecs::entity p = e; p.id() != 0; p = p.parent()) {
                    if (const Style* s = p.try_get<Style>()) { style = *s; break; }
                }
                flecs::world w = e.world();
                auto fill = [&](flecs::entity& target, const char* name) {
                    if (target.id() == 0) target = w.lookup(name);
                };
                fill(style.rect, "Mesh2D::UIRect");
                fill(style.circle, "Mesh2D::UICircle");
                fill(style.meshPipeline, "Mesh2DPipe::Basic");
                fill(style.font, "Font::Main");
                fill(style.fontPipeline, "FontPipe::Basic");
                fill(style.clipMeshPipeline, "Mesh2DPipe::UIClip");
                fill(style.clipFontPipeline, "FontPipe::UIClip");
                return style;
            }

            inline const ColorSet pickSet(const ColorSet& own, const ColorSet& fallback) {
                return ColorSet{pick(own.normal, fallback.normal), pick(own.hover, fallback.hover),
                                pick(own.pressed, fallback.pressed), pick(own.disabled, fallback.disabled)};
            }

            inline Color stateColor(const ColorSet& set, const Interact& in) {
                if (!in.enabled) return set.disabled;
                if (in.pressed && in.hovered) return set.pressed;
                if (in.hovered) return set.hover;
                return set.normal;
            }

            // ---- comparisons (only write what changed, writing triggers observers) --------------------------------------
            inline bool same(const Transform2D& a, const Transform2D& b) {
                return a.position == b.position && a.positionZ == b.positionZ && a.scale == b.scale &&
                       a.rotation == b.rotation;
            }
            inline bool same(const Mesh2D& a, const Mesh2D& b) {
                return a.mesh.id() == b.mesh.id() && a.pipeline.id() == b.pipeline.id() &&
                       a.texture.id() == b.texture.id() && a.color == b.color && a.uvRect == b.uvRect &&
                       a.params == b.params && a.clip.id() == b.clip.id();
            }
            inline bool same(const Text& a, const Text& b) {
                return a.text == b.text && a.font.id() == b.font.id() && a.pipeline.id() == b.pipeline.id() &&
                       a.color == b.color && a.size == b.size && a.lineSpacing == b.lineSpacing &&
                       a.align == b.align && a.maxWidth == b.maxWidth && a.clip.id() == b.clip.id();
            }

            // ---- the drawing context of one widget --------------------------------------------------------------------
            // A part is `leaf` (child of the widget: Transform2D + Transform2DGpu) with one child that holds the Mesh2D /
            // Text. Mesh2D and Text have to sit directly below an entity with Transform2D + Transform2DGpu, and a
            // rectangle gets its size through the scale of its leaf (the registered mesh is a unit square / circle).
            // Hidden widgets lose the Transform2DGpu of their leaves, so the renderer drops them.
            struct Ctx {
                flecs::entity e;
                flecs::world w;
                Style style;
                Node node;
                glm::vec2 size{0.0f};
                bool visible{true};
                Parts parts;
                bool partsChanged{false};
                bool nodeChanged{false};
                flecs::entity_t clip{0};     // clip rectangle of the ScrollView this widget is in (0 = none)

                explicit Ctx(flecs::entity entity) : e(entity), w(entity.world()) {
                    style = resolveStyle(e);
                    node = e.get<Node>();
                    size = node.size;
                    visible = !isHidden(e);
                    if (canClip(style)) clip = findClip(e);
                    if (const Parts* p = e.try_get<Parts>()) parts = *p;
                }

                // Node::size 0 = the widget uses (and stores) its default size.
                void defaultSize(glm::vec2 fallback) {
                    if (size.x <= 0.0f && size.y <= 0.0f) {
                        size = fallback;
                        node.size = fallback;
                        nodeChanged = true;
                    }
                }

                void setSize(glm::vec2 newSize) {
                    if (newSize != size) {
                        size = newSize;
                        node.size = newSize;
                        nodeChanged = true;
                    }
                }

                template<class T>
                void part(Slot slot, glm::vec2 position, glm::vec2 scale, uint32_t layer, const T& value) {
                    const size_t index = static_cast<size_t>(slot) * 2;
                    flecs::entity_t& leafId = parts.part[index];
                    flecs::entity_t& visualId = parts.part[index + 1];

                    Transform2D want;
                    want.position = position;
                    want.scale = scale;
                    want.positionZ = layer;

                    if (!alive(w, leafId) || !alive(w, visualId)) {
                        if (alive(w, leafId)) w.entity(leafId).destruct();
                        flecs::entity leaf = w.entity().child_of(e).set<Transform2D>(want);
                        if (visible) leaf.add<Transform2DGpu>();
                        flecs::entity visual = w.entity().child_of(leaf).set<T>(value);
                        leafId = leaf.id();
                        visualId = visual.id();
                        partsChanged = true;
                        return;
                    }

                    flecs::entity leaf = w.entity(leafId);
                    flecs::entity visual = w.entity(visualId);
                    const Transform2D* have = leaf.try_get<Transform2D>();
                    if (!have || !same(*have, want)) leaf.set<Transform2D>(want);
                    const T* haveValue = visual.try_get<T>();
                    if (!haveValue || !same(*haveValue, value)) visual.set<T>(value);
                    setVisible(leaf, visible);
                }

                static void setVisible(flecs::entity leaf, bool show) {
                    const bool has = leaf.has<Transform2DGpu>();
                    if (show && !has) leaf.add<Transform2DGpu>();
                    else if (!show && has) leaf.remove<Transform2DGpu>();
                }

                void hide(Slot slot) {
                    const flecs::entity_t leafId = parts.part[static_cast<size_t>(slot) * 2];
                    if (alive(w, leafId)) setVisible(w.entity(leafId), false);
                }

                // everything that is drawn inside a clipping ScrollView uses the clipping pipelines + the clip rectangle
                void applyClip(Mesh2D& mesh) const {
                    mesh.pipeline = clip != 0 ? style.clipMeshPipeline : style.meshPipeline;
                    if (clip != 0) mesh.clip = w.entity(clip);
                }
                void applyClip(Text& text) const {
                    text.pipeline = clip != 0 ? style.clipFontPipeline : style.fontPipeline;
                    if (clip != 0) text.clip = w.entity(clip);
                }

                void rect(Slot slot, glm::vec2 position, glm::vec2 rectSize, uint32_t layer, const Color& color) {
                    Mesh2D mesh;
                    mesh.mesh = style.rect;
                    applyClip(mesh);
                    mesh.color = color;
                    part(slot, position, glm::max(rectSize, glm::vec2(0.0f)), layer, mesh);
                }

                void circle(Slot slot, glm::vec2 position, glm::vec2 diameter, uint32_t layer, const Color& color) {
                    Mesh2D mesh;
                    mesh.mesh = style.circle;
                    applyClip(mesh);
                    mesh.color = color;
                    part(slot, position, diameter, layer, mesh);
                }

                void image(Slot slot, glm::vec2 position, glm::vec2 rectSize, uint32_t layer, const Image& image) {
                    Mesh2D mesh;
                    mesh.mesh = style.rect;
                    applyClip(mesh);
                    mesh.texture = image.texture;
                    mesh.color = image.tint;
                    mesh.uvRect = image.uvRect;
                    part(slot, position, rectSize, layer, mesh);
                }

                void text(Slot slot, glm::vec2 position, uint32_t layer, const std::string& str, const Color& color,
                          float fontSize, TextAlign align, float maxWidth = 0.0f) {
                    Text text;
                    text.text = str.empty() ? std::string(" ") : str;
                    text.font = style.font;
                    applyClip(text);
                    text.color = color;
                    text.size = fontSize;
                    text.align = align;
                    text.maxWidth = maxWidth;
                    part(slot, position, glm::vec2(1.0f), layer, text);
                }

                // A rectangle with a border: Border at `layer`, fill at `layer + 1`.
                void bordered(Slot border, Slot fill, glm::vec2 position, glm::vec2 rectSize, uint32_t layer,
                              float borderWidth, const Color& borderColor, const Color& fillColor) {
                    const float bw = std::min(borderWidth, std::min(rectSize.x, rectSize.y) * 0.5f);
                    if (bw > 0.0f) {
                        rect(border, position, rectSize, layer, borderColor);
                        rect(fill, position + glm::vec2(bw), rectSize - glm::vec2(bw * 2.0f), layer + 1, fillColor);
                    } else {
                        hide(border);
                        rect(fill, position, rectSize, layer + 1, fillColor);
                    }
                }

                // Rounded rectangle from 7 parts (first .. first+6): a body, a top and a bottom bar and 4 corner circles.
                void roundedRect(Slot first, glm::vec2 position, glm::vec2 rectSize, uint32_t layer, float radius, const Color& color) {
                    auto slot = [&](int i) { return static_cast<Slot>(static_cast<int>(first) + i); };
                    const float r = std::max(0.0f, std::min(radius, std::min(rectSize.x, rectSize.y) * 0.5f));
                    if (r < 0.5f) {
                        rect(slot(0), position, rectSize, layer, color);
                        for (int i = 1; i < 7; ++i) hide(slot(i));
                        return;
                    }
                    const float midH = rectSize.y - 2.0f * r;
                    const float midW = rectSize.x - 2.0f * r;
                    if (midH > 0.01f) rect(slot(0), position + glm::vec2(0.0f, r), {rectSize.x, midH}, layer, color);
                    else hide(slot(0));
                    if (midW > 0.01f) {
                        rect(slot(1), position + glm::vec2(r, 0.0f), {midW, r}, layer, color);
                        rect(slot(2), position + glm::vec2(r, rectSize.y - r), {midW, r}, layer, color);
                    } else {
                        hide(slot(1));
                        hide(slot(2));
                    }
                    const glm::vec2 d(r * 2.0f);
                    circle(slot(3), position, d, layer, color);
                    circle(slot(4), position + glm::vec2(rectSize.x - d.x, 0.0f), d, layer, color);
                    circle(slot(5), position + glm::vec2(0.0f, rectSize.y - d.y), d, layer, color);
                    circle(slot(6), position + rectSize - d, d, layer, color);
                }

                float fontSizeOr(float own) const { return own > 0.0f ? own : style.fontSize; }

                // Child widget entity (close button, scroll bar, ...) owned by this widget.
                flecs::entity ownedChild(Child which, bool& created) {
                    flecs::entity_t& id = parts.child[static_cast<size_t>(which)];
                    created = false;
                    if (alive(w, id)) return w.entity(id);
                    flecs::entity c = w.entity().child_of(e);
                    id = c.id();
                    partsChanged = true;
                    created = true;
                    return c;
                }

                void finish() {
                    if (partsChanged) e.set<Parts>(parts);
                    if (nodeChanged) e.set<Node>(node);
                }
            };
        }
    }
}
