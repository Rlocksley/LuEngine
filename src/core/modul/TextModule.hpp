#pragma once
#include "Global.hpp"
#include "flecs.h"
#include "Channel.hpp"
#include "component/Text.hpp"
#include "component/Transform2D.hpp"

namespace Lu {
namespace Module {
    // Component::Text (on a child of an entity with a Component::Transform2D) -> EcsRequest::CreateText / DestroyText.
    // Same structure as the Mesh module.
    struct Text{
        Text(flecs::world& world){
            world.component<Component::Text>();

            // Text added / changed while the parent has a Transform2D
            world.observer<const Component::Text>()
                .with<Component::Transform2D>().filter()
                .term_at(1).up()
                .event(flecs::OnAdd)
                .event(flecs::OnSet)
                .each([](flecs::entity e, const Component::Text&) {
                    e.add<Component::TextGpuDirty>();
                });

            // Transform2D added to the parent after the Text exists
            world.observer()
                .with<Component::Text>().filter()
                .with<Component::Transform2D>()
                .term_at(1).up()
                .event(flecs::OnAdd)
                .each([](flecs::entity e) {
                    e.add<Component::TextGpuDirty>();
                });

            world.system<const Component::Text>("EcsRequest::CreateText System")
                .with<Component::TextGpuDirty>()
                .run([](flecs::iter it){
                    std::vector<EcsRequest::EcsRequest> requests;

                    while (it.next()) {
                        auto texts = it.field<const Component::Text>(0);
                        for (size_t i = 0; i < it.count(); ++i) {
                            auto entity = it.entity(i);
                            const auto parent = entity.parent();
                            const Component::Text& text = texts[i];
                            if (parent.is_valid() && parent.has<Component::Transform2D>() &&
                                text.font.id() != 0 && text.pipeline.id() != 0) {
                                requests.push_back(EcsRequest::CreateText{
                                    parent.id(), entity.id(), text.font.id(), text.pipeline.id(),
                                    text.text, text.color, text.size, text.lineSpacing, text.align, text.maxWidth});
                            }
                            entity.remove<Component::TextGpuDirty>();
                        }
                    }

                    if (!requests.empty()) GetChannel().send(std::move(requests));
                });

            world.observer<const Component::Text>()
                .with<Component::Transform2D>()
                .term_at(1).up()
                .event(flecs::OnRemove)
                .each([](flecs::entity e, const Component::Text&) {
                    GetChannel().send(EcsRequest::DestroyText{e.id()});
                });
        }
    };
}
}
