#pragma once

#include "UICoreModule.hpp"
#include "UIBasicWidgets.hpp"
#include "UIContainerWidgets.hpp"

namespace Lu {
    namespace Module {

        // The UI module: widgets as entity hierarchies (see component/UICore.hpp).
        //
        //      .importModule<Lu::Module::UI>()
        //
        // Needs Module::InputState and Module::Transform2D (+ Transform2DGpu, Mesh2D, Text, imported by the App).
        // Frame: PostLoad -> [UIInput: mouse, drag, buttons, sliders, typing, tooltip] -> [UILayout] -> [UISync: redraw dirty
        // widgets] -> PreUpdate ...
        struct UI {
            explicit UI(flecs::world& world) {
                using namespace Lu::UI;
                using namespace Lu::UI::Detail;

                Phases phases;
                phases.input = world.entity("UIPhase::Input").add(flecs::Phase).depends_on(flecs::PostLoad);
                phases.layout = world.entity("UIPhase::Layout").add(flecs::Phase).depends_on(phases.input);
                phases.sync = world.entity("UIPhase::Sync").add(flecs::Phase).depends_on(phases.layout);
                world.entity(flecs::PreUpdate).depends_on(phases.sync);

                installDirtyTracking(world);
                std::shared_ptr<Pointer> pointer = world.get<PointerRef>().state;
                installPointer(world, phases, pointer);
                installBasicWidgets(world, phases, pointer);
                installContainerWidgets(world, phases, pointer);
                installLayout(world, phases);

                // Redraw: every Dirty widget creates / updates the entities that draw it.
                world.system("UISync")
                    .with<Dirty>()
                    .kind(phases.sync)
                    .each([](flecs::entity e) {
                        e.remove<Dirty>();
                        if (!e.has<Node>() || !e.has<Lu::Component::Transform2D>()) return;
                        Ctx c(e);
                        if (const Window* w = e.try_get<Window>()) syncWindow(c, *w);
                        if (const Panel* w = e.try_get<Panel>()) syncPanel(c, *w);
                        if (const Label* w = e.try_get<Label>()) syncLabel(c, *w);
                        if (const Image* w = e.try_get<Image>()) syncImage(c, *w);
                        if (const ProgressBar* w = e.try_get<ProgressBar>()) syncProgress(c, *w);
                        if (const Button* w = e.try_get<Button>()) syncButton(c, *w);
                        if (const Toggle* w = e.try_get<Toggle>()) syncToggle(c, *w);
                        if (const Slider* w = e.try_get<Slider>()) syncSlider(c, *w);
                        if (const TextInput* w = e.try_get<TextInput>()) syncTextInput(c, *w);
                        if (const Dropdown* w = e.try_get<Dropdown>()) syncDropdown(c, *w);
                        if (const ScrollView* w = e.try_get<ScrollView>()) syncScrollView(c, *w);
                        if (const TabView* w = e.try_get<TabView>()) syncTabView(c, *w);
                        c.finish();
                    });
            }
        };

        using UIModule = UI;
    }
}
