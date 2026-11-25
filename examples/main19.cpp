#include "App.hpp"
#include "Shape2D.hpp"
#include "module/ButtonModule.hpp"
#include "module/ScrollListModule.hpp"
#include "component/Transform2D.hpp"
#include "component/Transform2DGpu.hpp"
#include "component/Mesh2D.hpp"
#include "component/Text2D.hpp"
#include <filesystem>
#include <memory>
#include <random>
#include <vector>

namespace Demo {
static std::string findFont() {
    for (const auto& path : {"C:/Windows/Fonts/arial.ttf", "C:/Windows/Fonts/segoeui.ttf"})
        if (std::filesystem::exists(path)) return path;
    return {};
}

// Apply one button's SDF treatment. The button owns a crisp body and a separate
// soft SDF layer; callbacks decide their colors, gradient, glow, and depression.
static void style(flecs::entity owner, glm::vec4 fill, float gradient,
                  glm::vec4 glow, float glowAlpha, float offsetY) {
    owner.children([&](flecs::entity part) {
        const auto* layerTransform = part.try_get<Lu::Component::Transform2D>();
        if (layerTransform && layerTransform->positionZ == 0) {
            if (auto* transform = part.try_get<Lu::Component::Transform2D>()) {
                auto updated = *transform;
                if (updated.position.y != -12.f) {
                    updated.position = {-12.f, -12.f};
                    part.set<Lu::Component::Transform2D>(updated);
                }
            }
            part.children([&](flecs::entity leaf) {
                if (auto* mesh = leaf.try_get<Lu::Component::Mesh2D>()) {
                    auto updated = *mesh;
                    const glm::vec4 color{glow.r, glow.g, glow.b, glowAlpha};
                    if (updated.color.r != color.r || updated.color.g != color.g ||
                        updated.color.b != color.b || updated.color.a != color.a ||
                        updated.params.x != 18.f || updated.params.y != 12.f ||
                        updated.params.z != 9.f || updated.params.w != 0.f) {
                        updated.color = color;
                        updated.params = {18.f, 12.f, 9.f, 0.f};
                        leaf.set<Lu::Component::Mesh2D>(updated);
                    }
                }
            });
        } else if (layerTransform && layerTransform->positionZ == 1) {
            if (auto* transform = part.try_get<Lu::Component::Transform2D>()) {
                auto updated = *transform;
                if (updated.position.y != offsetY) {
                    updated.position.y = offsetY;
                    part.set<Lu::Component::Transform2D>(updated);
                }
            }
            part.children([&](flecs::entity leaf) {
                if (auto* mesh = leaf.try_get<Lu::Component::Mesh2D>()) {
                    auto updated = *mesh;
                    if (updated.color.r != fill.r || updated.color.g != fill.g ||
                        updated.color.b != fill.b || updated.color.a != fill.a ||
                        updated.params.w != gradient) {
                        updated.color = fill;
                        updated.params.w = gradient;
                        leaf.set<Lu::Component::Mesh2D>(updated);
                    }
                }
            });
        }
    });
}

struct ButtonDemo {
    explicit ButtonDemo(flecs::world& world) {
        const auto mesh = world.lookup("Mesh2D::Button");
        const auto sdf = world.lookup("Mesh2DPipe::SDFButton");
        const auto softSdf = world.lookup("Mesh2DPipe::SDFBackground");
        const auto font = world.lookup("Font::Main");
        const auto textPipe = world.lookup("FontPipe::Basic");

        auto titleTransform = world.entity("TitleTransform")
            .set<Lu::Component::Transform2D>({.position={56.f,28.f}, .positionZ=20})
            .add<Lu::Component::Transform2DGpu>();
        world.entity("TitleText").set(flecs::Parent{titleTransform})
            .set(Lu::Component::Text2D{.text="MODULAR BUTTONS", .font=font, .pipeline=textPipe,
                .color={0.78f,0.88f,1.f,1.f}, .size=30.f});

        auto statusTransform = world.entity("StatusTransform")
            .set<Lu::Component::Transform2D>({.position={370.f,92.f}, .positionZ=20})
            .add<Lu::Component::Transform2DGpu>();
        auto status = world.entity("StatusText").set(flecs::Parent{statusTransform})
            .set(Lu::Component::Text2D{.text="Choose a button", .font=font, .pipeline=textPipe,
                .color={0.92f,0.88f,0.72f,1.f}, .size=22.f});

        auto rotating = world.entity("RotatingMeshTransform")
            .set<Lu::Component::Transform2D>({.position={650.f,360.f}, .positionZ=2, .scale={132.f,132.f}})
            .add<Lu::Component::Transform2DGpu>();
        world.entity("RotatingMesh").set(flecs::Parent{rotating})
            .set(Lu::Component::Mesh2D{.mesh=mesh, .pipeline=sdf,
                .color={0.22f,0.67f,0.93f,1.f}, .params={12.f,2.f,0.f,0.15f}});
        auto hintTransform = world.entity("SpinHintTransform")
            .set<Lu::Component::Transform2D>({.position={552.f,445.f}, .positionZ=3})
            .add<Lu::Component::Transform2DGpu>();
        world.entity("SpinHint").set(flecs::Parent{hintTransform})
            .set(Lu::Component::Text2D{.text="Toggle keeps it spinning", .font=font,
                .pipeline=textPipe, .color={0.62f,0.76f,0.86f,1.f}, .size=17.f});

        auto addButton = [&](const char* name, const char* label, float y,
                             Lu::Component::UIButtonMode mode,
                             std::function<void(flecs::entity)> onClicked,
                             std::function<void(flecs::entity)> onHoverEnter,
                             std::function<void(flecs::entity)> onHover,
                             std::function<void(flecs::entity)> onHoverExit,
                             std::function<void(flecs::entity)> onRelease,
                             bool selected=false, uint32_t group=0) {
            const glm::vec2 size{228.f,54.f};
            auto entity = world.entity(name)
                .set<Lu::Component::Transform2D>({.position={56.f,y}, .positionZ=10})
                .add<Lu::Component::Transform2DGpu>()
                .set(Lu::Component::UIButton{
                    .mesh=mesh, .meshPipeline=sdf, .mesh2=mesh, .meshPipeline2=softSdf,
                    
                    .font=font, .fontPipeline=textPipe,
                    .text=label, .color={0.10f,0.16f,0.24f,1.f}, .textColor={0.91f,0.94f,0.98f,1.f},
                    .fontSize=19.f, .meshParams={10.f,2.f,0.f,0.08f},
                    .mesh2Param={18.f,12.f,9.f,0.f},.mesh2Color={0.4f,0.7f,1.f,0.f},
                    .textTransform={.position={114.f,16.f}, .positionZ=2},
                    .selected=selected, .mode=mode, .radioGroup=group, .size=size,
                    .onClicked=std::move(onClicked), .onHover=std::move(onHover),
                    .onHoverEnter=std::move(onHoverEnter), .onHoverExit=std::move(onHoverExit),
                    .onRelease=std::move(onRelease)
                });
            return entity;
        };

        auto setStatus = [status, font, textPipe](const char* message) {
            status.set<Lu::Component::Text2D>({.text=message, .font=font, .pipeline=textPipe,
                .color={0.92f,0.88f,0.72f,1.f}, .size=22.f});
        };

        addButton("MomentaryButton", "Momentary", 104.f, Lu::Component::UIButtonMode::Momentary,
            [setStatus](flecs::entity e) { setStatus("Pressed: brief cyan flare"); style(e,{0.12f,0.62f,0.78f,1.f},-0.22f,{0.18f,0.84f,1.f,1.f},0.8f,3.f); },
            [](flecs::entity e) { style(e,{0.16f,0.28f,0.38f,1.f},0.18f,{0.22f,0.72f,0.95f,1.f},0.42f,-1.f); },
            [](flecs::entity e) {
                if (e.get<Lu::Component::UIButton>().pressed)
                    style(e,{0.12f,0.62f,0.78f,1.f},-0.22f,{0.18f,0.84f,1.f,1.f},0.8f,3.f);
                else style(e,{0.16f,0.28f,0.38f,1.f},0.14f,{0.22f,0.72f,0.95f,1.f},0.36f,-1.f);
            },
            [](flecs::entity e) {
                if (e.get<Lu::Component::UIButton>().pressed)
                    style(e,{0.12f,0.62f,0.78f,1.f},-0.22f,{0.18f,0.84f,1.f,1.f},0.8f,3.f);
                else style(e,{0.10f,0.16f,0.24f,1.f},0.08f,{0.22f,0.72f,0.95f,1.f},0.f,0.f);
            },
            [](flecs::entity e) { style(e,{0.10f,0.16f,0.24f,1.f},0.08f,{0.22f,0.72f,0.95f,1.f},0.f,0.f); });

        addButton("ToggleButton", "Toggle: spin", 176.f, Lu::Component::UIButtonMode::Toggle,
            [setStatus](flecs::entity e) {
                const auto& b=e.get<Lu::Component::UIButton>();
                setStatus(b.selected ? "Selected: mint halo, rotation on" : "Unselected: rotation off");
                style(e,b.selected ? glm::vec4{0.12f,0.42f,0.28f,1.f} : glm::vec4{0.10f,0.16f,0.24f,1.f},
                    b.selected ? -0.18f : 0.08f, {0.28f,1.f,0.62f,1.f}, b.selected ? 0.7f : 0.f, 0.f);
            },
            [](flecs::entity e) { style(e,{0.20f,0.28f,0.20f,1.f},0.18f,{0.55f,1.f,0.42f,1.f},0.38f,-1.f); },
            [](flecs::entity e) {
                const auto& b=e.get<Lu::Component::UIButton>();
                if (b.pressed) style(e,{0.12f,0.42f,0.28f,1.f},-0.28f,{0.35f,1.f,0.55f,1.f},0.82f,3.f);
                else if (b.selected) style(e,{0.12f,0.42f,0.28f,1.f},-0.18f,{0.28f,1.f,0.62f,1.f},0.7f,0.f);
                else style(e,{0.20f,0.28f,0.20f,1.f},0.14f,{0.55f,1.f,0.42f,1.f},0.3f,-1.f);
            },
            [](flecs::entity e) {
                const auto& b=e.get<Lu::Component::UIButton>();
                if (b.pressed) style(e,{0.12f,0.42f,0.28f,1.f},-0.28f,{0.35f,1.f,0.55f,1.f},0.82f,3.f);
                else style(e,b.selected ? glm::vec4{0.12f,0.42f,0.28f,1.f} : glm::vec4{0.10f,0.16f,0.24f,1.f},
                    b.selected ? -0.18f : 0.08f,{0.28f,1.f,0.62f,1.f},b.selected ? 0.7f : 0.f,0.f);
            },
            [](flecs::entity e) {
                const auto& b=e.get<Lu::Component::UIButton>();
                style(e,b.selected ? glm::vec4{0.12f,0.42f,0.28f,1.f} : glm::vec4{0.10f,0.16f,0.24f,1.f},
                    b.selected ? -0.18f : 0.08f,{0.28f,1.f,0.62f,1.f},b.selected ? 0.7f : 0.f,0.f);
            });

        auto radioUpdate = [&world](flecs::entity) {
            world.each([](flecs::entity e, const Lu::Component::UIButton& b) {
                if (b.mode != Lu::Component::UIButtonMode::Radio) return;
                if (b.pressed) style(e,{0.52f,0.30f,0.74f,1.f},-0.28f,{0.88f,0.62f,1.f,1.f},0.82f,3.f);
                else style(e,b.selected ? glm::vec4{0.38f,0.24f,0.58f,1.f} : glm::vec4{0.10f,0.16f,0.24f,1.f},
                    b.selected ? -0.15f : 0.08f,{0.76f,0.46f,1.f,1.f},b.selected ? 0.62f : 0.f,0.f);
            });
        };
        auto radioHoverEnter = [](flecs::entity e) {
            const auto& b=e.get<Lu::Component::UIButton>();
            style(e,b.selected ? glm::vec4{0.38f,0.24f,0.58f,1.f} : glm::vec4{0.27f,0.20f,0.38f,1.f},
                b.selected ? -0.15f : 0.18f,{0.80f,0.58f,1.f,1.f},b.selected ? 0.7f : 0.42f,b.pressed ? 3.f : -1.f);
        };
        auto radioHover = [](flecs::entity e) {
            const auto& b=e.get<Lu::Component::UIButton>();
            if (b.pressed) style(e,{0.52f,0.30f,0.74f,1.f},-0.28f,{0.88f,0.62f,1.f,1.f},0.82f,3.f);
            else style(e,b.selected ? glm::vec4{0.38f,0.24f,0.58f,1.f} : glm::vec4{0.27f,0.20f,0.38f,1.f},
                b.selected ? -0.15f : 0.14f,{0.80f,0.58f,1.f,1.f},b.selected ? 0.62f : 0.36f,0.f);
        };
        auto radioReset = [](flecs::entity e) {
            const auto& b=e.get<Lu::Component::UIButton>();
            if (b.pressed) style(e,{0.52f,0.30f,0.74f,1.f},-0.28f,{0.88f,0.62f,1.f,1.f},0.82f,3.f);
            else style(e,b.selected ? glm::vec4{0.38f,0.24f,0.58f,1.f} : glm::vec4{0.10f,0.16f,0.24f,1.f},
                b.selected ? -0.15f : 0.08f,{0.76f,0.46f,1.f,1.f},b.selected ? 0.62f : 0.f,0.f);
        };
        addButton("RadioOne", "Radio option A", 248.f, Lu::Component::UIButtonMode::Radio,
            [radioUpdate,setStatus](flecs::entity e) { radioUpdate(e); setStatus("Selected: violet radio A"); },
            radioHoverEnter,radioHover,radioReset,radioReset,true,1);
        addButton("RadioTwo", "Radio option B", 320.f, Lu::Component::UIButtonMode::Radio,
            [radioUpdate,setStatus](flecs::entity e) { radioUpdate(e); setStatus("Selected: violet radio B"); },
            radioHoverEnter,radioHover,radioReset,radioReset,false,1);
        radioUpdate(world.lookup("RadioOne"));

        addButton("HoldButton", "Hold to press", 392.f, Lu::Component::UIButtonMode::Hold,
            [setStatus](flecs::entity e) { setStatus("Held: amber bevel stays down"); style(e,{0.68f,0.34f,0.08f,1.f},-0.28f,{1.f,0.48f,0.08f,1.f},0.8f,4.f); },
            [](flecs::entity e) { style(e,{0.34f,0.23f,0.12f,1.f},0.18f,{1.f,0.63f,0.18f,1.f},0.4f,-1.f); },
            [](flecs::entity e) {
                if (e.get<Lu::Component::UIButton>().pressed)
                    style(e,{0.68f,0.34f,0.08f,1.f},-0.28f,{1.f,0.48f,0.08f,1.f},0.8f,4.f);
                else style(e,{0.34f,0.23f,0.12f,1.f},0.14f,{1.f,0.63f,0.18f,1.f},0.35f,-1.f);
            },
            [](flecs::entity e) {
                if (e.get<Lu::Component::UIButton>().pressed)
                    style(e,{0.68f,0.34f,0.08f,1.f},-0.28f,{1.f,0.48f,0.08f,1.f},0.8f,4.f);
                else style(e,{0.10f,0.16f,0.24f,1.f},0.08f,{1.f,0.63f,0.18f,1.f},0.f,0.f);
            },
            [](flecs::entity e) { style(e,{0.10f,0.16f,0.24f,1.f},0.08f,{1.f,0.63f,0.18f,1.f},0.f,0.f); });

        world.system<Lu::Component::UIButton>("Rotate while toggle selected")
            .each([rotating](const Lu::Component::UIButton& button) {
                if (button.mode != Lu::Component::UIButtonMode::Toggle) return;
                auto& transform = rotating.get_mut<Lu::Component::Transform2D>();
                transform.setAngle(transform.getAngle() + (button.selected ? 0.025f : 0.f));
                rotating.modified<Lu::Component::Transform2D>();
            });
    }
};
// A UIScrollList whose elements are UIButtons. The list does not know what its elements are: for every index it
// creates a slot entity and calls createItem, which puts a UIButton on the slot. Everything below the list is clipped
// to the list rectangle (Clip2D), so the pipelines have to support clipping (mesh2d_ui.* and font_clip.*).
struct ScrollListDemo {
    explicit ScrollListDemo(flecs::world& world) {
        const auto mesh = world.lookup("Mesh2D::Button");
        const auto sdfClip = world.lookup("Mesh2DPipe::SDFClip");
        const auto font = world.lookup("Font::Main");
        const auto clipFont = world.lookup("FontPipe::Clip");
        const auto textPipe = world.lookup("FontPipe::Basic");
        auto status = world.lookup("Demo::ButtonDemo::StatusText");

        auto setStatus = [status, font, textPipe](const std::string& message) {
            status.set<Lu::Component::Text2D>({.text=message, .font=font, .pipeline=textPipe,
                .color={0.92f,0.88f,0.72f,1.f}, .size=22.f});
        };

        auto hintTransform = world.entity("ScrollHintTransform")
            .set<Lu::Component::Transform2D>({.position={310.f,532.f}, .positionZ=3})
            .add<Lu::Component::Transform2DGpu>();
        world.entity("ScrollHint").set(flecs::Parent{hintTransform})
            .set(Lu::Component::Text2D{.text="Mouse wheel or drag the scrollbar", .font=font,
                .pipeline=textPipe, .color={0.62f,0.76f,0.86f,1.f}, .size=17.f});

        constexpr uint32_t itemCount = 30;
        auto rowIds = std::make_shared<std::vector<uint32_t>>();
        for (uint32_t i = 0; i < itemCount; ++i) rowIds->push_back(i + 1);
        auto scrollListEntity = world.entity("ScrollList")
            .set<Lu::Component::Transform2D>({.position={310.f,140.f}, .positionZ=10})
            .add<Lu::Component::Transform2DGpu>()
            .set(Lu::Component::UIScrollList{
                .size={236.f,380.f}, .itemCount=itemCount, .itemHeight=44.f, .spacing=8.f, .padding=8.f,
                .mesh=mesh, .meshPipeline=sdfClip,
                .backgroundColor={0.05f,0.08f,0.12f,1.f}, .backgroundParams={12.f,0.f,0.f,0.f},
                .scrollbarWidth=8.f,
                .trackColor={1.f,1.f,1.f,0.08f}, .trackParams={4.f,0.f,0.f,0.f},
                .thumbParams={4.f,0.f,0.f,0.f},
                // Any UI component can be the element: here every element is a UIButton.
                .createItem=[=](flecs::entity slot, uint32_t index, glm::vec2 itemSize) {
                    const glm::vec4 idle{0.10f,0.16f,0.24f,1.f};
                    const glm::vec4 hover{0.16f,0.28f,0.38f,1.f};
                    const glm::vec4 down{0.12f,0.62f,0.78f,1.f};
                    const std::string name = "List button " + std::to_string((*rowIds)[index]);
                    slot.set(Lu::Component::UIButton{
                        .mesh=mesh, .meshPipeline=sdfClip,
                        .font=font, .fontPipeline=clipFont,
                        .text=name, .color=idle, .textColor={0.91f,0.94f,0.98f,1.f},
                        .fontSize=18.f, .meshParams={10.f,2.f,0.f,0.08f},
                        .textTransform={.position={itemSize.x * 0.5f, 12.f}, .positionZ=2},
                        .size=itemSize,
                        .onClicked=[=](flecs::entity e) {
                            setStatus("Clicked: " + name);
                            style(e, down, -0.22f, {}, 0.f, 3.f);
                        },
                        .onHoverEnter=[=](flecs::entity e) { style(e, hover, 0.18f, {}, 0.f, -1.f); },
                        .onHoverExit=[=](flecs::entity e) { style(e, idle, 0.08f, {}, 0.f, 0.f); },
                        .onRelease=[=](flecs::entity e) { style(e, hover, 0.14f, {}, 0.f, -1.f); }
                    });
                }
            });

        auto insertCount = std::make_shared<uint32_t>(0);
        auto random = std::make_shared<std::mt19937>(std::random_device{}());
        const glm::vec2 insertButtonSize{236.f, 44.f};
        world.entity("InsertRandomListItem")
            .set<Lu::Component::Transform2D>({.position={310.f,570.f}, .positionZ=10})
            .add<Lu::Component::Transform2DGpu>()
            .set(Lu::Component::UIButton{
                .mesh=mesh, .meshPipeline=sdfClip,
                .font=font, .fontPipeline=clipFont,
                .text="Insert random item", .color={0.10f,0.16f,0.24f,1.f},
                .textColor={0.91f,0.94f,0.98f,1.f}, .fontSize=17.f,
                .meshParams={10.f,2.f,0.f,0.08f},
                .textTransform={.position={insertButtonSize.x * 0.5f, 12.f}, .positionZ=2},
                .size=insertButtonSize,
                .onClicked=[scrollListEntity, insertCount, random, rowIds, setStatus, mesh, sdfClip, font, clipFont]
                    (flecs::entity button) {
                    auto list = scrollListEntity.get<Lu::Component::UIScrollList>();
                    if (const auto* state = scrollListEntity.try_get<Lu::Component::UIScrollListState>())
                        list.scroll = state->appliedScroll;

                    const uint32_t insertionIndex = std::uniform_int_distribution<uint32_t>(0, list.elementCount())(*random);
                    const uint32_t insertionNumber = ++*insertCount;
                    const std::string label = std::to_string(insertionNumber) + "-th insert at " +
                        std::to_string(insertionIndex);
                    rowIds->insert(rowIds->begin() + insertionIndex, 0);
                    list.addItem(insertionIndex, [=](flecs::entity slot, uint32_t, glm::vec2 itemSize) {
                        const glm::vec4 idle{0.10f,0.16f,0.24f,1.f};
                        const glm::vec4 hover{0.16f,0.28f,0.38f,1.f};
                        const glm::vec4 down{0.12f,0.62f,0.78f,1.f};
                        slot.set(Lu::Component::UIButton{
                            .mesh=mesh, .meshPipeline=sdfClip,
                            .font=font, .fontPipeline=clipFont,
                            .text=label, .color=idle, .textColor={0.91f,0.94f,0.98f,1.f},
                            .fontSize=18.f, .meshParams={10.f,2.f,0.f,0.08f},
                            .textTransform={.position={itemSize.x * 0.5f, 12.f}, .positionZ=2},
                            .size=itemSize,
                            .onClicked=[setStatus, label](flecs::entity e) { setStatus("Clicked: " + label); },
                            .onHoverEnter=[=](flecs::entity e) { style(e, hover, 0.18f, {}, 0.f, -1.f); },
                            .onHoverExit=[=](flecs::entity e) { style(e, idle, 0.08f, {}, 0.f, 0.f); },
                            .onRelease=[=](flecs::entity e) { style(e, hover, 0.14f, {}, 0.f, -1.f); }
                        });
                    });
                    scrollListEntity.set<Lu::Component::UIScrollList>(list);
                    setStatus("Inserted: " + label);
                    style(button, {0.16f,0.28f,0.38f,1.f}, 0.14f, {}, 0.f, -1.f);
                }
            });

        world.entity("RemoveRandomListItem")
            .set<Lu::Component::Transform2D>({.position={560.f,570.f}, .positionZ=10})
            .add<Lu::Component::Transform2DGpu>()
            .set(Lu::Component::UIButton{
                .mesh=mesh, .meshPipeline=sdfClip,
                .font=font, .fontPipeline=clipFont,
                .text="Remove random item", .color={0.10f,0.16f,0.24f,1.f},
                .textColor={0.91f,0.94f,0.98f,1.f}, .fontSize=17.f,
                .meshParams={10.f,2.f,0.f,0.08f},
                .textTransform={.position={insertButtonSize.x * 0.5f, 12.f}, .positionZ=2},
                .size=insertButtonSize,
                .onClicked=[scrollListEntity, random, rowIds, setStatus](flecs::entity button) {
                    auto list = scrollListEntity.get<Lu::Component::UIScrollList>();
                    const uint32_t count = list.elementCount();
                    if (count == 0) {
                        setStatus("The scroll list is empty");
                        return;
                    }

                    if (const auto* state = scrollListEntity.try_get<Lu::Component::UIScrollListState>())
                        list.scroll = state->appliedScroll;
                    const uint32_t removalIndex = std::uniform_int_distribution<uint32_t>(0, count - 1)(*random);
                    const uint32_t removedRowId = (*rowIds)[removalIndex];

                    rowIds->erase(rowIds->begin() + removalIndex);
                    list.removeItem(removalIndex);
                    scrollListEntity.set<Lu::Component::UIScrollList>(list);
                    const std::string removedLabel = removedRowId == 0
                        ? "inserted item" : "List button " + std::to_string(removedRowId);
                    setStatus("Removed " + removedLabel + " at " + std::to_string(removalIndex));
                    style(button, {0.16f,0.28f,0.38f,1.f}, 0.14f, {}, 0.f, -1.f);
                }
            });
    }
};
}

int main() {
    Lu::App("LuEngine Modular Buttons", false, 960, 640)
        .registerShape2D<Lu::Shape2D::Rectangle>("Mesh2D::Button", glm::vec2(1.f,1.f))
        .registerMesh2DPipe(Lu::GraphicsPipelineConfig{
            .name="Mesh2DPipe::SDFButton", .capacity=256,
            .vertexShader="shader/mesh2d_ui.vert.spv", .fragmentShader="shader/mesh2d_button.frag.spv",
            .depthCompareOp=VK_COMPARE_OP_LESS_OR_EQUAL, .blendEnable=false, .alphaToCoverageEnable=true
        })
        .registerMesh2DPipe(Lu::GraphicsPipelineConfig{
            .name="Mesh2DPipe::SDFBackground", .capacity=256,
            .vertexShader="shader/mesh2d_ui.vert.spv", .fragmentShader="shader/mesh2d_background.frag.spv",
            .cullMode=VK_CULL_MODE_NONE,
            .depthWriteEnable=false, .depthCompareOp=VK_COMPARE_OP_LESS_OR_EQUAL,
            .blendEnable=true
        })
        // Clipping pipelines used by the scroll list elements (Clip2D is inherited from the list).
        .registerMesh2DPipe(Lu::GraphicsPipelineConfig{
            .name="Mesh2DPipe::SDFClip", .capacity=1024,
            .vertexShader="shader/mesh2d_ui.vert.spv", .fragmentShader="shader/mesh2d_ui.frag.spv",
            .depthCompareOp=VK_COMPARE_OP_LESS_OR_EQUAL, .blendEnable=false, .alphaToCoverageEnable=true
        })
        .registerFont("Font::Main", Demo::findFont(), 48)
        .registerFontPipe(Lu::GraphicsPipelineConfig{
            .name="FontPipe::Basic", .capacity=1024, .vertexShader="shader/font_basic.vert.spv",
            .fragmentShader="shader/font_basic.frag.spv", .cullMode=VK_CULL_MODE_NONE,
            .depthCompareOp=VK_COMPARE_OP_LESS_OR_EQUAL
        })
        .registerFontPipe(Lu::GraphicsPipelineConfig{
            .name="FontPipe::Clip", .capacity=1024, .vertexShader="shader/font_clip.vert.spv",
            .fragmentShader="shader/font_clip.frag.spv", .cullMode=VK_CULL_MODE_NONE,
            .depthCompareOp=VK_COMPARE_OP_LESS_OR_EQUAL
        })
        .importModule<Lu::Module::UIButton>()
        .importModule<Lu::Module::UIScrollList>()
        .importModule<Demo::ButtonDemo>()
        .importModule<Demo::ScrollListDemo>()
        .run();
}
