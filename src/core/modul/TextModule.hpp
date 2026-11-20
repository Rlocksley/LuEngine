#pragma once
#include "Global.hpp"
#include "flecs.h"
#include "Channel.hpp"
#include "component/Text2D.hpp"
#include "component/Clip2D.hpp"
#include "component/Transform2D.hpp"
#include "component/Transform2DGpu.hpp"

namespace Lu {
namespace Module {
    // Component::Text (on a child of an entity with a Component::Transform2D AND a Component::Transform2DGpu)
    // -> EcsRequest::CreateText / DestroyText. The parent has to be mirrored by the GPU, because the renderer
    // resolves its Transform2D slot. Same structure as the Mesh module.
    struct Text{
        Text(flecs::world& world){
            world.component<Component::Text2D>();

            // Inherit the nearest ancestor Clip2D only when the Text2D has no explicit clip.
            // With no Clip2D ancestor, leave the component's clip as supplied by the caller.
            world.observer<Component::Text2D, const Component::Clip2D*>()
                .term_at(1).up().optional()
                .event(flecs::OnAdd)
                .event(flecs::OnSet)
                .each([](flecs::iter& it, size_t row, Component::Text2D& text, const Component::Clip2D* ancestorClip) {
                    if (text.clip.id() != 0 || ancestorClip == nullptr) return;
                    auto entity = it.entity(row);
                    text.clip = it.src(1);
                    entity.modified<Component::Text2D>();
                });

            // Text added / changed while the parent has a Transform2D and a Transform2DGpu
            world.observer<const Component::Text2D>()
                .with<Component::Transform2D>().filter()
                .with<Component::Transform2DGpu>().filter()
                .term_at(1).up()
                .term_at(2).up()
                .event(flecs::OnAdd)
                .event(flecs::OnSet)
                .each([](flecs::entity e, const Component::Text2D&) {
                    e.add<Component::Text2DGpuDirty>();
                });

            // Transform2D / Transform2DGpu added to the parent after the Text exists
            world.observer()
                .with<Component::Text2D>().filter()
                .with<Component::Transform2D>()
                .with<Component::Transform2DGpu>()
                .term_at(1).up()
                .term_at(2).up()
                .event(flecs::OnAdd)
                .each([](flecs::entity e) {
                    e.add<Component::Text2DGpuDirty>();
                });

            world.system<const Component::Text2D>("EcsRequest::CreateText System")
                .with<Component::Text2DGpuDirty>()
                .run([](flecs::iter it){
                    std::vector<EcsRequest::EcsRequest> requests;

                    while (it.next()) {
                        auto texts = it.field<const Component::Text2D>(0);
                        for (size_t i = 0; i < it.count(); ++i) {
                            auto entity = it.entity(i);
                            const auto parent = entity.parent();
                            const Component::Text2D& text = texts[i];
                            
                            if (parent.is_valid() && parent.has<Component::Transform2D>() && parent.has<Component::Transform2DGpu>() &&
                                text.font && text.pipeline) {
                                requests.push_back(EcsRequest::CreateText{
                                    parent.id(), entity.id(), text.font.id(), text.pipeline.id(),
                                    text.text, text.color, text.size, text.lineSpacing, text.align, text.maxWidth, text.clip.id()});
                            }
                            entity.remove<Component::Text2DGpuDirty>();
                        }
                    }

                    if (!requests.empty()) GetChannel().send(std::move(requests));
                });

            world.observer<const Component::Text2D>()
                .with<Component::Transform2D>()
                .with<Component::Transform2DGpu>()
                .term_at(1).up()
                .term_at(2).up()
                .event(flecs::OnRemove)
                .each([](flecs::entity e, const Component::Text2D&) {
                    GetChannel().send(EcsRequest::DestroyText{e.id()});
                });
        }
    };
}
}
