#pragma once

#include "UIButton.hpp"
#include "ClickBox2D.hpp"
#include "component/Transform2D.hpp"
#include "component/Transform2DGpu.hpp"
#include "component/Mesh2D.hpp"
#include "component/Text2D.hpp"

namespace Lu {
namespace Module {

// UIButton owns its render and hit-test children. Visuals are ordinary Mesh2D/Text2D
// components, so buttons can use any registered pipeline, including the SDF UI shader.
struct UIButton {
    explicit UIButton(flecs::world& world) {
        world.component<Component::UIButton>();
        world.import<ClickBox2D>();

        world.observer<const Component::UIButton, const Component::Transform2D>()
            .event(flecs::OnSet)
            .each([](flecs::entity e, const Component::UIButton& button, const Component::Transform2D&) {
                e.children([](flecs::entity child) { child.destruct(); });

                // Optional back layer (for a glow, shadow, or other SDF effect).
                // Its larger transform leaves room for the SDF softness around the button.
                if (button.mesh2.id() != 0 && button.meshPipeline2.id() != 0) {
                    auto background = e.world().entity().set(flecs::Parent{e})
                        .set<Component::Transform2D>({.positionZ=0, .scale=button.size + glm::vec2(24.f)})
                        .add<Component::Transform2DGpu>();
                    background.world().entity().set(flecs::Parent{background}).set(Component::Mesh2D{
                        .mesh=button.mesh2, .pipeline=button.meshPipeline2,
                        .color=button.mesh2Color, .params=button.mesh2Param
                    });
                }

                auto visual = e.world().entity().set(flecs::Parent{e})
                    .set<Component::Transform2D>({.positionZ=1, .scale=button.size})
                    .add<Component::Transform2DGpu>();
                visual.world().entity().set(flecs::Parent{visual}).set(Component::Mesh2D{
                        .mesh=button.mesh, .pipeline=button.meshPipeline, .texture=button.texture,
                        .color=button.color, .uvRect=button.uvRect, .params=button.meshParams
                    });

                auto hit = e.world().entity().set(flecs::Parent{e})
                    .set<Component::ClickBox2D>({.size=button.size, .enabled=button.enabled});
                (void)hit;

                if (!button.text.empty()) {
                    auto textTransform = e.world().entity().set(flecs::Parent{e})
                        .set(button.textTransform).add<Component::Transform2DGpu>();
                    textTransform.world().entity().set(flecs::Parent{textTransform}).set(Component::Text2D{
                            .text=button.text, .font=button.font, .pipeline=button.fontPipeline,
                            .color=button.textColor, .size=button.fontSize,
                            .lineSpacing=button.lineSpacing, .align=button.align, .maxWidth=button.maxWidth
                        });
                }
            });

        // Button behavior is driven only by tags on its owned ClickBox2D child.
        world.system<const Component::ClickBox2DClicked, Component::UIButton>("UIButton click")
            .term_at(1).up()
            .each([](flecs::entity box, const Component::ClickBox2DClicked&, Component::UIButton& button) {
                flecs::entity owner = box.parent();
                if (!button.enabled) return;
                button.pressed = true;
                switch (button.mode) {
                case Component::UIButtonMode::Momentary: break;
                case Component::UIButtonMode::Toggle: button.selected = !button.selected; break;
                case Component::UIButtonMode::Hold: button.selected = true; break;
                case Component::UIButtonMode::Radio:
                    button.selected = true;
                    if (button.radioGroup != 0) {
                        owner.world().each([&](flecs::entity other, Component::UIButton& candidate) {
                            if (other != owner && candidate.mode == Component::UIButtonMode::Radio &&
                                candidate.radioGroup == button.radioGroup) candidate.selected = false;
                        });
                    }
                    break;
                }
                if (button.onClicked) button.onClicked(owner);
            });

        world.system<const Component::ClickBox2DReleased, Component::UIButton>("UIButton release")
            .term_at(1).up()
            .each([](flecs::entity box, const Component::ClickBox2DReleased&, Component::UIButton& button) {
                button.pressed = false;
                if (button.mode == Component::UIButtonMode::Hold) button.selected = false;
                if (button.onRelease) button.onRelease(box.parent());
            });

        world.system<const Component::ClickBox2DHoveredEnter, Component::UIButton>("UIButton hover enter")
            .term_at(1).up()
            .each([](flecs::entity box, const Component::ClickBox2DHoveredEnter&, Component::UIButton& button) {
                if (button.onHoverEnter) button.onHoverEnter(box.parent());
            });
        world.system<const Component::ClickBox2DHoveredExit, Component::UIButton>("UIButton hover exit")
            .term_at(1).up()
            .each([](flecs::entity box, const Component::ClickBox2DHoveredExit&, Component::UIButton& button) {
                if (button.onHoverExit) button.onHoverExit(box.parent());
            });
        world.system<const Component::ClickBox2DHovered, Component::UIButton>("UIButton hover")
            .term_at(1).up()
            .each([](flecs::entity box, const Component::ClickBox2DHovered&, Component::UIButton& button) {
                if (button.onHover) button.onHover(box.parent());
            });

    }
};

}
}
