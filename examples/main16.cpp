#include "App.hpp"
#include "Shape2D.hpp"
#include "component/Transform2D.hpp"
#include "component/Transform2DGpu.hpp"
#include "component/Mesh2D.hpp"
#include "component/Text.hpp"
#include <filesystem>

// =============================================================================
// main16: GUI building blocks = Mesh2D (+ Text) on Transform2D parents.
//
//   App::registerShape2D<Shape2D::...>   2D geometry (vertices in local pixels, origin = top-left for rectangles)
//   App::registerMesh2DPipe              GraphicsPipeline<Vertex2D>, set 0: Camera2D, Transform2D, Mesh2D, global texture array
//   Component::Transform2D (+ Transform2DGpu)  on the PARENT: position, positionZ (layer), rotation, scale
//   Component::Mesh2D                    on a CHILD of it: mesh, pipeline, color, optional texture / uvRect / params
//   Component::Text                      same parent rules, drawn after the meshes
//
// Layers: positionZ is shared by meshes and text, a higher layer is in front. A change of a Mesh2D
// (set / modified) is an EcsRequest::CreateMesh2D, a change of a Transform2D is picked up automatically.
//
// Usage: lu_example16 [path/to/font.ttf]
// =============================================================================

struct PulseTag {};
struct BlinkTag {};
struct SpinTag {};

static std::string findFont(int argc, char** argv) {
    std::vector<std::string> candidates;
    if (argc > 1) candidates.push_back(argv[1]);
    candidates.insert(candidates.end(), {
        "C:/Windows/Fonts/arial.ttf",
        "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
        "/usr/share/fonts/dejavu/DejaVuSans.ttf",
        "/usr/share/fonts/TTF/DejaVuSans.ttf",
        "/Library/Fonts/Arial.ttf",
        "/System/Library/Fonts/Supplemental/Arial.ttf"
    });
    for (const auto& path : candidates) {
        if (std::filesystem::exists(path)) return path;
    }
    LU_LOGE("main16", "findFont", "No font found. Pass a .ttf file as the first argument.");
    return {};
}

struct GuiDemoModule {
    // parent (Transform2D + Transform2DGpu) + child (Mesh2D)
    static flecs::entity createMesh2D(flecs::world& world, const char* name, const char* geometry,
                                      const Lu::Component::Transform2D& transform, const glm::vec4& color) {
        return world.entity((std::string(name) + "::Mesh").c_str())
            .set(flecs::Parent{
                world.entity(name)
                .set<Lu::Component::Transform2D>(transform)
                .add<Lu::Component::Transform2DGpu>()
            })
            .set(Lu::Component::Mesh2D{
                .mesh = world.lookup(geometry),
                .pipeline = world.lookup("Mesh2DPipe::Basic"),
                .color = color
            });
    }

    // parent (Transform2D + Transform2DGpu) + child (Text)
    static flecs::entity createText(flecs::world& world, const char* name, const std::string& string,
                                    const Lu::Component::Transform2D& transform, const glm::vec4& color,
                                    float size, Lu::TextAlign align = Lu::TextAlign::Left) {
        return world.entity((std::string(name) + "::Text").c_str())
            .set(flecs::Parent{
                world.entity(name)
                .set<Lu::Component::Transform2D>(transform)
                .add<Lu::Component::Transform2DGpu>()
            })
            .set(Lu::Component::Text{
                .text = string,
                .font = world.lookup("Font::Main"),
                .pipeline = world.lookup("FontPipe::Basic"),
                .color = color,
                .size = size,
                .align = align
            });
    }

    explicit GuiDemoModule(flecs::world& world) {
        // window panel: background (layer 1), title bar (layer 2), content on top (layers 3..4)
        createMesh2D(world, "Panel", "Mesh2D::Panel", {.position = {40.f, 40.f}, .positionZ = 1}, glm::vec4(0.12f, 0.13f, 0.17f, 1.f));
        createMesh2D(world, "TitleBar", "Mesh2D::TitleBar", {.position = {40.f, 40.f}, .positionZ = 2}, glm::vec4(0.20f, 0.35f, 0.75f, 1.f));
        createText(world, "Title", "Mesh2D GUI demo", {.position = {56.f, 48.f}, .positionZ = 4}, glm::vec4(1.f), 22.f);

        // three buttons: label = child of its own Transform2D, centered on the button
        const char* names[3] = {"Play", "Options", "Quit"};
        const glm::vec4 colors[3] = {glm::vec4(0.20f, 0.65f, 0.35f, 1.f), glm::vec4(0.35f, 0.40f, 0.55f, 1.f), glm::vec4(0.75f, 0.25f, 0.25f, 1.f)};
        for (int i = 0; i < 3; ++i) {
            const float y = 110.f + 64.f * static_cast<float>(i);
            const std::string buttonName = std::string("Button") + names[i];
            auto button = createMesh2D(world, buttonName.c_str(), "Mesh2D::Button", {.position = {60.f, y}, .positionZ = 3}, colors[i]);
            if (i == 0) button.add<PulseTag>();       // animated color
            createText(world, (buttonName + "Label").c_str(), names[i], {.position = {60.f + 90.f, y + 10.f}, .positionZ = 4},
                       glm::vec4(1.f), 24.f, Lu::TextAlign::Center);
        }
        world.system<Lu::Component::Mesh2D>("PulseButton")
            .with<PulseTag>()
            .interval(1.0f / 30.0f)
            .each([](flecs::entity e, Lu::Component::Mesh2D& mesh) {
                const float t = static_cast<float>(e.world().get_info()->world_time_total);
                const float k = 0.75f + 0.25f * std::sin(t * 3.0f);
                mesh.color = glm::vec4(0.20f * k, 0.65f * k, 0.35f * k, 1.f);
                e.modified<Lu::Component::Mesh2D>();      // -> EcsRequest::CreateMesh2D (update)
            });

        // status LED (circle) that blinks + a radio button (ring) and its label
        auto led = createMesh2D(world, "Led", "Mesh2D::Led", {.position = {300.f, 132.f}, .positionZ = 3}, glm::vec4(0.1f, 1.f, 0.2f, 1.f));
        led.add<BlinkTag>();
        createText(world, "LedLabel", "online", {.position = {318.f, 120.f}, .positionZ = 4}, glm::vec4(0.8f, 0.9f, 0.8f, 1.f), 20.f);
        world.system<Lu::Component::Mesh2D>("BlinkLed")
            .with<BlinkTag>()
            .interval(0.5f)
            .each([](flecs::entity e, Lu::Component::Mesh2D& mesh) {
                const bool on = mesh.color.g > 0.5f;
                mesh.color = on ? glm::vec4(0.1f, 0.25f, 0.1f, 1.f) : glm::vec4(0.1f, 1.f, 0.2f, 1.f);
                e.modified<Lu::Component::Mesh2D>();
            });

        createMesh2D(world, "Radio", "Mesh2D::Radio", {.position = {300.f, 180.f}, .positionZ = 3}, glm::vec4(0.9f, 0.9f, 0.3f, 1.f));
        createMesh2D(world, "RadioDot", "Mesh2D::RadioDot", {.position = {300.f, 180.f}, .positionZ = 3}, glm::vec4(0.9f, 0.9f, 0.3f, 1.f));
        createText(world, "RadioLabel", "option", {.position = {318.f, 168.f}, .positionZ = 4}, glm::vec4(0.8f, 0.9f, 0.8f, 1.f), 20.f);

        // a rounded square that rotates and is scaled: Transform2D changes are mirrored every frame (triple buffer)
        createMesh2D(world, "Spinner", "Mesh2D::Spinner", {.position = {330.f, 290.f}, .positionZ = 3, .scale = {1.2f, 1.2f}},
                     glm::vec4(0.95f, 0.6f, 0.15f, 1.f));
        world.lookup("Spinner").add<SpinTag>();       // the tag is on the entity that has the Transform2D (the parent)
        world.system<Lu::Component::Transform2D>("SpinSquare")
            .with<SpinTag>()
            .each([](flecs::iter& it, size_t, Lu::Component::Transform2D& transform) {
                transform.setAngle(transform.getAngle() + 1.2f * it.delta_time());
            });

        // layers: the order of creation does not matter, positionZ does. The back rectangle is created last.
        createMesh2D(world, "LayerFront", "Mesh2D::Layer", {.position = {560.f, 120.f}, .positionZ = 6}, glm::vec4(0.2f, 0.8f, 1.f, 1.f));
        createMesh2D(world, "LayerBack", "Mesh2D::Layer", {.position = {600.f, 150.f}, .positionZ = 5}, glm::vec4(1.f, 0.3f, 0.7f, 1.f));
        createText(world, "LayerText", "z = 7 (text above both)", {.position = {540.f, 190.f}, .positionZ = 7}, glm::vec4(1.f), 22.f);
    }
};

int main(int argc, char** argv) {
    const std::string fontPath = findFont(argc, argv);

    Lu::App("LuEngine Mesh2D GUI", false, 1280, 800)
        // 2D geometry: rectangles are described by size (+ top-left offset), round shapes by their center
        .registerShape2D<Lu::Shape2D::Rectangle>("Mesh2D::Panel", glm::vec2(420.f, 340.f))
        .registerShape2D<Lu::Shape2D::RoundedRectangle>("Mesh2D::TitleBar", glm::vec2(420.f, 36.f), 12.f, 8u)
        .registerShape2D<Lu::Shape2D::RoundedRectangle>("Mesh2D::Button", glm::vec2(180.f, 46.f), 12.f, 8u)
        .registerShape2D<Lu::Shape2D::Circle>("Mesh2D::Led", 9.f, 24u, glm::vec2(9.f, 9.f))        // center = (9, 9): top-left at the position
        .registerShape2D<Lu::Shape2D::Ring>("Mesh2D::Radio", 11.f, 8.f, 32u, glm::vec2(11.f, 11.f))
        .registerShape2D<Lu::Shape2D::Circle>("Mesh2D::RadioDot", 5.f, 16u, glm::vec2(11.f, 11.f))
        .registerShape2D<Lu::Shape2D::RoundedRectangle>("Mesh2D::Spinner", glm::vec2(60.f, 60.f), 14.f, 8u, glm::vec2(-30.f, -30.f))   // centered on its position
        .registerShape2D<Lu::Shape2D::Rectangle>("Mesh2D::Layer", glm::vec2(120.f, 70.f))
        .registerMesh2DPipe(Lu::GraphicsPipelineConfig{
            .name = "Mesh2DPipe::Basic",
            .capacity = 1024,                                   // max Mesh2D drawn per frame by this pipeline
            .vertexShader = "shader/mesh2d_basic.vert.spv",
            .fragmentShader = "shader/mesh2d_basic.frag.spv",
            .depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL
        })
        .registerFont("Font::Main", fontPath, 64)
        .registerFontPipe(Lu::GraphicsPipelineConfig{
            .name = "FontPipe::Basic",
            .capacity = 4096,
            .vertexShader = "shader/font_basic.vert.spv",
            .fragmentShader = "shader/font_basic.frag.spv",
            .cullMode = VK_CULL_MODE_NONE,
            .depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL
        })
        .importModule<GuiDemoModule>()
        .run();
}
