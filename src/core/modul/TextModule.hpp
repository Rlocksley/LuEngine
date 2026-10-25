#pragma once
#include "Global.hpp"
#include "flecs.h"
#include "Channel.hpp"
#include "component/Text.hpp"
#include "component/Transform2D.hpp"
#include "component/Transform2DGpu.hpp"

namespace Lu {
namespace Module {
    // Component::Text (on a child of an entity with a Component::Transform2D AND a Component::Transform2DGpu)
    // -> EcsRequest::CreateText / DestroyText. The parent has to be mirrored by the GPU, because the renderer
    // resolves its Transform2D slot. Same structure as the Mesh module.
    struct Text{
        Text(flecs::world& world){
            world.component<Component::Text>();

            // Text added / changed while the parent has a Transform2D and a Transform2DGpu
            world.observer<const Component::Text>()
                .with<Component::Transform2D>().filter()
                .with<Component::Transform2DGpu>().filter()
                .term_at(1).up()
                .term_at(2).up()
                .event(flecs::OnAdd)
                .event(flecs::OnSet)
                .each([](flecs::entity e, const Component::Text&) {
                    e.add<Component::TextGpuDirty>();
                });

            // Transform2D / Transform2DGpu added to the parent after the Text exists
            world.observer()
                .with<Component::Text>().filter()
                .with<Component::Transform2D>()
                .with<Component::Transform2DGpu>()
                .term_at(1).up()
                .term_at(2).up()
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
                            if (parent.is_valid() && parent.has<Component::Transform2D>() && parent.has<Component::Transform2DGpu>() &&
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
                .with<Component::Transform2DGpu>()
                .term_at(1).up()
                .term_at(2).up()
                .event(flecs::OnRemove)
                .each([](flecs::entity e, const Component::Text&) {
                    GetChannel().send(EcsRequest::DestroyText{e.id()});
                });
        }
    };
}
}
