#pragma once

#include "UIBasicWidgets.hpp"

namespace Lu {
    namespace UI {
        namespace Detail {

            // Child widget entities that a widget owns (close button, scroll bar, options, tabs): they are ordinary
            // widget entities, only the module creates them. `set` is only called when something differs.
            template<class T>
            inline void setIfDifferent(flecs::entity e, const T& want, bool (*equal)(const T&, const T&)) {
                const T* have = e.try_get<T>();
                if (!have || !equal(*have, want)) e.set<T>(want);
            }
            inline bool sameNode(const Node& a, const Node& b) {
                return a.size == b.size && a.layer == b.layer && a.inLayout == b.inLayout && a.order == b.order;
            }

            inline void placeChild(flecs::entity child, glm::vec2 position, const Node& node) {
                Transform2D t;
                if (const Transform2D* have = child.try_get<Transform2D>()) t = *have;
                t.position = position;
                setIfDifferent<Transform2D>(child, t, [](const Transform2D& a, const Transform2D& b) { return same(a, b); });
                setIfDifferent<Node>(child, node, sameNode);
            }

            inline void destroyList(flecs::world& w, std::vector<flecs::entity_t>& list) {
                for (flecs::entity_t id : list) if (alive(w, id)) w.entity(id).destruct();
                list.clear();
            }

            // ---- Window -------------------------------------------------------------------------------------------------
            inline void syncWindow(Ctx& c, const Window& window) {
                c.defaultSize({400.0f, 300.0f});
                const float th = std::min(window.titleHeight, c.size.y);
                const float bw = c.style.borderWidth;
                const float fs = c.fontSizeOr(window.fontSize);
                c.rect(Slot::WindowTitleBar, {bw, bw}, {std::max(0.0f, c.size.x - bw * 2.0f), std::max(0.0f, th - bw)}, 2, c.style.titleBar);
                c.text(Slot::WindowTitleText, {14.0f, std::max(0.0f, (th - fs) * 0.5f)}, 3, window.title, c.style.accent, fs, TextAlign::Left);

                const float closeSize = std::max(8.0f, th - 12.0f);
                bool created = false;
                if (window.closable) {
                    flecs::entity close = c.ownedChild(Child::WindowClose, created);
                    Node node;
                    node.size = {closeSize, closeSize};
                    node.layer = 4;
                    node.inLayout = false;
                    const glm::vec2 pos(c.size.x - closeSize - 8.0f, (th - closeSize) * 0.5f);
                    if (created) {
                        Transform2D t; t.position = pos;
                        Button button;
                        button.text = "x";
                        button.onClick = [](flecs::entity self) {
                            flecs::entity win = self.parent();
                            if (!win.id() || !win.has<Window>()) return;
                            const Window w = win.get<Window>();
                            if (w.onClose) w.onClose(win);
                            if (w.destroyOnClose) win.destruct(); else win.add<Hidden>();
                        };
                        close.set<Transform2D>(t).set<Node>(node).set<Button>(button);
                    } else {
                        placeChild(close, pos, node);
                    }
                } else {
                    flecs::entity_t& id = c.parts.child[static_cast<size_t>(Child::WindowClose)];
                    if (alive(c.w, id)) c.w.entity(id).destruct();
                    if (id != 0) { id = 0; c.partsChanged = true; }
                }

                if (window.draggable) {
                    flecs::entity drag = c.ownedChild(Child::WindowDrag, created);
                    Node node;
                    node.size = {std::max(0.0f, c.size.x - (window.closable ? closeSize + 16.0f : 0.0f)), th};
                    node.layer = 3;
                    node.inLayout = false;
                    if (created) {
                        Draggable d;
                        d.target = c.e;
                        drag.set<Transform2D>({}).set<Node>(node).set<Draggable>(d).add<Interact>();
                    } else {
                        placeChild(drag, {0.0f, 0.0f}, node);
                    }
                } else {
                    flecs::entity_t& id = c.parts.child[static_cast<size_t>(Child::WindowDrag)];
                    if (alive(c.w, id)) c.w.entity(id).destruct();
                    if (id != 0) { id = 0; c.partsChanged = true; }
                }
            }

            // ---- Dropdown -----------------------------------------------------------------------------------------------
            inline void syncDropdown(Ctx& c, const Dropdown& dd) {
                c.defaultSize({220.0f, 36.0f});
                const Interact in = interactOf(c.e);
                const float fs = c.fontSizeOr(dd.fontSize);
                const bool valid = dd.selected >= 0 && dd.selected < static_cast<int>(dd.options.size());
                c.bordered(Slot::DropdownBorder, Slot::DropdownFill, {0.0f, 0.0f}, c.size, 0, c.style.borderWidth,
                           (dd.open || in.hovered) ? c.style.accent : (in.enabled ? c.style.panelBorder : c.style.textDisabled),
                           stateColor(c.style.input, in));
                const float ty = std::max(0.0f, (c.size.y - fs) * 0.5f);
                c.text(Slot::DropdownText, {10.0f, ty}, 3, valid ? dd.options[static_cast<size_t>(dd.selected)] : dd.placeholder,
                       !in.enabled ? c.style.textDisabled : valid ? c.style.text : c.style.textDim, fs, TextAlign::Left);
                c.text(Slot::DropdownArrow, {c.size.x - 16.0f, ty}, 3, dd.open ? "^" : "v", c.style.accent, fs, TextAlign::Center);

                // the open list: one Button entity per option (children of the dropdown, above everything near it)
                if (!dd.open || !c.visible) {
                    if (!c.parts.list.empty()) { destroyList(c.w, c.parts.list); c.partsChanged = true; }
                    return;
                }
                while (c.parts.list.size() > dd.options.size()) {
                    if (alive(c.w, c.parts.list.back())) c.w.entity(c.parts.list.back()).destruct();
                    c.parts.list.pop_back();
                    c.partsChanged = true;
                }
                for (size_t i = 0; i < dd.options.size(); ++i) {
                    Node node;
                    node.size = c.size;
                    node.layer = 40;
                    node.inLayout = false;
                    const glm::vec2 pos(0.0f, c.size.y * static_cast<float>(i + 1));
                    Button button;
                    button.text = dd.options[i];
                    button.selected = static_cast<int>(i) == dd.selected;
                    button.fontSize = dd.fontSize;
                    const int index = static_cast<int>(i);
                    button.onClick = [index](flecs::entity self) {
                        flecs::entity box = self.parent();
                        if (!box.id() || !box.has<Dropdown>()) return;
                        Dropdown d = box.get<Dropdown>();
                        d.selected = index;
                        d.open = false;
                        box.set<Dropdown>(d);
                        if (d.onChange) d.onChange(box, index);
                    };
                    if (i >= c.parts.list.size()) {
                        flecs::entity option = c.w.entity().child_of(c.e);
                        Transform2D t; t.position = pos;
                        option.set<Transform2D>(t).set<Node>(node).set<Button>(button);
                        c.parts.list.push_back(option.id());
                        c.partsChanged = true;
                    } else if (alive(c.w, c.parts.list[i])) {
                        flecs::entity option = c.w.entity(c.parts.list[i]);
                        placeChild(option, pos, node);
                        const Button* have = option.try_get<Button>();
                        if (!have || have->text != button.text || have->selected != button.selected || have->fontSize != button.fontSize) {
                            option.set<Button>(button);
                        }
                    }
                }
            }

            // ---- ScrollView ---------------------------------------------------------------------------------------------
            inline void syncScrollView(Ctx& c, const ScrollView& view) {
                c.defaultSize({300.0f, 300.0f});
                bool created = false;
                const float viewport = c.size.y;
                const float maxScroll = std::max(0.0f, view.contentHeight - viewport);
                if (view.scrollbar) {
                    flecs::entity bar = c.ownedChild(Child::ScrollBar, created);
                    Node node;
                    node.size = {view.scrollbarWidth, c.size.y};
                    node.layer = 24;
                    node.inLayout = false;
                    const glm::vec2 pos(c.size.x - view.scrollbarWidth, 0.0f);
                    Slider slider;
                    slider.orientation = Orientation::Vertical;
                    slider.value = maxScroll > 0.0f ? std::clamp(view.scroll / maxScroll, 0.0f, 1.0f) : 0.0f;
                    slider.knobSize = view.scrollbarWidth;
                    slider.trackThickness = std::max(4.0f, view.scrollbarWidth * 0.4f);
                    slider.onChange = [](flecs::entity self, float v) {
                        flecs::entity owner = self.parent();
                        if (!owner.id() || !owner.has<ScrollView>() || !owner.has<Node>()) return;
                        ScrollView sv = owner.get<ScrollView>();
                        const float maxS = std::max(0.0f, sv.contentHeight - owner.get<Node>().size.y);
                        sv.scroll = v * maxS;
                        owner.set<ScrollView>(sv);
                    };
                    if (created) {
                        Transform2D t; t.position = pos;
                        bar.set<Transform2D>(t).set<Node>(node).set<Slider>(slider);
                    } else {
                        placeChild(bar, pos, node);
                        const Slider* have = bar.try_get<Slider>();
                        if (!have || std::abs(have->value - slider.value) > 1e-4f || have->knobSize != slider.knobSize) {
                            slider.onChange = have ? have->onChange : slider.onChange;
                            bar.set<Slider>(slider);
                        }
                    }
                    if (maxScroll <= 0.0f) { if (!bar.has<Hidden>()) bar.add<Hidden>(); }
                    else if (bar.has<Hidden>()) bar.remove<Hidden>();
                } else {
                    flecs::entity_t& id = c.parts.child[static_cast<size_t>(Child::ScrollBar)];
                    if (alive(c.w, id)) c.w.entity(id).destruct();
                    if (id != 0) { id = 0; c.partsChanged = true; }
                }
            }

            // ---- TabView ------------------------------------------------------------------------------------------------
            inline void syncTabView(Ctx& c, const TabView& tabs) {
                c.defaultSize({400.0f, 300.0f});
                struct Page { flecs::entity entity; int order; };
                std::vector<Page> pages;
                c.e.children([&](flecs::entity child) {
                    if (const TabPage* p = child.try_get<TabPage>()) pages.push_back({child, p->order});
                });
                std::sort(pages.begin(), pages.end(), [](const Page& a, const Page& b) {
                    return a.order != b.order ? a.order < b.order : a.entity.id() < b.entity.id();
                });
                const int selected = pages.empty() ? 0 : std::clamp(tabs.selected, 0, static_cast<int>(pages.size()) - 1);

                while (c.parts.list.size() > pages.size()) {
                    if (alive(c.w, c.parts.list.back())) c.w.entity(c.parts.list.back()).destruct();
                    c.parts.list.pop_back();
                    c.partsChanged = true;
                }
                for (size_t i = 0; i < pages.size(); ++i) {
                    Node node;
                    node.size = {tabs.tabWidth, tabs.tabHeight};
                    node.layer = 4;
                    node.inLayout = false;
                    const glm::vec2 pos(tabs.tabWidth * static_cast<float>(i), 0.0f);
                    Button button;
                    button.text = pages[i].entity.get<TabPage>().title;
                    button.selected = static_cast<int>(i) == selected;
                    const int index = static_cast<int>(i);
                    button.onClick = [index](flecs::entity self) {
                        flecs::entity view = self.parent();
                        if (!view.id() || !view.has<TabView>()) return;
                        TabView v = view.get<TabView>();
                        if (v.selected == index) return;
                        v.selected = index;
                        view.set<TabView>(v);
                        if (v.onChange) v.onChange(view, index);
                    };
                    if (i >= c.parts.list.size()) {
                        flecs::entity tab = c.w.entity().child_of(c.e);
                        Transform2D t; t.position = pos;
                        tab.set<Transform2D>(t).set<Node>(node).set<Button>(button);
                        c.parts.list.push_back(tab.id());
                        c.partsChanged = true;
                    } else if (alive(c.w, c.parts.list[i])) {
                        flecs::entity tab = c.w.entity(c.parts.list[i]);
                        placeChild(tab, pos, node);
                        const Button* have = tab.try_get<Button>();
                        if (!have || have->text != button.text || have->selected != button.selected) {
                            button.onClick = have ? have->onClick : button.onClick;
                            tab.set<Button>(button);
                        }
                    }

                    // the page itself: below the tab row, the rest of the size, only the selected one is shown
                    Node pageNode = pages[i].entity.get<Node>();
                    pageNode.size = {c.size.x, std::max(0.0f, c.size.y - tabs.tabHeight)};
                    pageNode.inLayout = false;
                    placeChild(pages[i].entity, {0.0f, tabs.tabHeight}, pageNode);
                    const bool active = static_cast<int>(i) == selected;
                    if (active && pages[i].entity.has<Inactive>()) pages[i].entity.remove<Inactive>();
                    else if (!active && !pages[i].entity.has<Inactive>()) pages[i].entity.add<Inactive>();
                }
            }

            // =====================================================================================================
            inline void installContainerWidgets(flecs::world& world, const Phases& phases, std::shared_ptr<Pointer> pointer) {
                watchWidget<Window>(world, true);
                watchWidget<Dropdown>(world, true);
                watchWidget<ScrollView>(world, true);
                watchWidget<TabView>(world);
                world.component<TabPage>();

                // a Window is a Panel, a ScrollView is a vertical Layout (both only if not configured otherwise)
                world.observer<const Window>().event(flecs::OnAdd).event(flecs::OnSet)
                    .each([](flecs::entity e, const Window&) { e.add<Panel>(); });
                world.observer<const ScrollView>().event(flecs::OnAdd).event(flecs::OnSet)
                    .each([](flecs::entity e, const ScrollView&) { e.add<Layout>(); });
                // pages of a TabView
                world.observer<const TabPage>().event(flecs::OnAdd).event(flecs::OnSet)
                    .each([](flecs::entity e, const TabPage&) {
                        e.add<Transform2D>();
                        e.add<Node>();
                        flecs::entity parent = e.parent();
                        if (parent.id() != 0 && parent.has<TabView>()) markDirty(parent);
                    });
                world.observer().with<TabPage>().with(flecs::ChildOf, flecs::Wildcard).event(flecs::OnAdd)
                    .each([](flecs::entity e) {
                        flecs::entity parent = e.parent();
                        if (parent.id() != 0 && parent.has<TabView>()) markDirty(parent);
                    });

                // ---- Dropdown: open / close ---------------------------------------------------------------------------
                world.system<Dropdown, const Interact>("UIDropdown")
                    .kind(phases.input)
                    .each([pointer](flecs::entity e, Dropdown& dd, const Interact& in) {
                        bool changed = false;
                        if (in.clicked && in.enabled) { dd.open = !dd.open; changed = true; }
                        else if (dd.open && pointer->justPressed) {
                            flecs::entity hovered = pointer->hovered != 0 ? e.world().entity(pointer->hovered) : flecs::entity();
                            if (hovered.id() == 0 || !isSelfOrDescendant(hovered, e.id())) { dd.open = false; changed = true; }
                        }
                        if (changed) e.modified<Dropdown>();
                    });

                // ---- ScrollView: mouse wheel over the view or over anything inside it ---------------------------------
                world.system("UIScrollWheel")
                    .kind(phases.input)
                    .run([pointer](flecs::iter& it) {
                        while (it.next()) {}
                        const Component::InputState& input = it.world().get<Component::InputState>();
                        const float wheel = input.scroll.deltaScroll.y;
                        if (wheel == 0.0f || pointer->hovered == 0) return;
                        flecs::world w = it.world();
                        for (flecs::entity c = w.entity(pointer->hovered); c.id() != 0; c = c.parent()) {
                            if (const ScrollView* sv = c.try_get<ScrollView>()) {
                                ScrollView next = *sv;
                                next.scroll = std::max(0.0f, next.scroll - wheel * next.wheelSpeed);
                                c.set<ScrollView>(next);   // the layout clamps it to the content
                                break;
                            }
                        }
                    });

                // ---- Window: raise above the sibling windows on click ----------------------------------------------------
                flecs::query<const Window, const Node> windows = world.query_builder<const Window, const Node>().build();
                world.system("UIWindowRaise")
                    .kind(phases.input)
                    .run([pointer, windows](flecs::iter& it) {
                        while (it.next()) {}
                        if (!pointer->justPressed || pointer->hovered == 0) return;
                        flecs::world w = it.world();
                        flecs::entity window;
                        for (flecs::entity c = w.entity(pointer->hovered); c.id() != 0; c = c.parent()) {
                            if (c.has<Window>()) { window = c; break; }
                        }
                        if (window.id() == 0) return;
                        const flecs::entity_t parentId = window.parent().id();
                        struct Entry { flecs::entity e; Node node; };
                        std::vector<Entry> siblings;
                        uint32_t maxOther = 0;
                        bool haveOther = false;
                        windows.each([&](flecs::entity o, const Window&, const Node& n) {
                            if (o.parent().id() != parentId) return;
                            siblings.push_back({o, n});
                            if (o.id() != window.id()) { maxOther = haveOther ? std::max(maxOther, n.layer) : n.layer; haveOther = true; }
                        });
                        const Node mine = window.get<Node>();
                        if (!haveOther || mine.layer > maxOther) return;      // already on top
                        uint32_t layer = maxOther + kWindowLayerGap;
                        if (layer > 60000) {                                    // out of layers: renumber all
                            std::sort(siblings.begin(), siblings.end(), [&](const Entry& a, const Entry& b) {
                                const uint32_t la = a.e.id() == window.id() ? UINT32_MAX : a.node.layer;
                                const uint32_t lb = b.e.id() == window.id() ? UINT32_MAX : b.node.layer;
                                return la != lb ? la < lb : a.e.id() < b.e.id();
                            });
                            uint32_t next = kLayerStep;
                            for (Entry& s : siblings) {
                                Node n = s.node; n.layer = next; next += kWindowLayerGap;
                                s.e.set<Node>(n);
                            }
                            return;
                        }
                        Node raised = mine;
                        raised.layer = layer;
                        window.set<Node>(raised);
                    });
            }
        }
    }
}
