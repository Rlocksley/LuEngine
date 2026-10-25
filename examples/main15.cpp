#include "App.hpp"
#include "Shape.hpp"
#include "FlyingCamera.hpp"
#include "component/Material.hpp"
#include "component/Mesh.hpp"
#include "component/Transform.hpp"
#include "component/TransformGpu.hpp"
#include "component/Transform2D.hpp"
#include "component/Camera2D.hpp"
#include "component/Text.hpp"
#include <filesystem>

// =============================================================================
// main15: 2D text on top of a 3D scene.
//
//   App::registerFont       FreeType -> signed distance field atlas -> global texture array
//   App::registerFontPipe   font_basic.vert/.frag, one indirect draw command per glyph
//   Component::Transform2D  on the PARENT entity: position (pixels), positionZ (layer), rotation, scale
//   Component::Text         on a CHILD of it: string, font, pipeline, color, size, alignment, word wrapping (maxWidth)
//   Component::Camera2D     optional, the default is "world pixel == screen pixel"
//
// The depth image is cleared before the text is drawn, so the text is always on top of the 3D cube
// and the glyphs only depth test against each other (higher positionZ == in front).
//
// Usage: lu_example15 [path/to/font.ttf]      Controls: WASD + hold right mouse button.
// =============================================================================

struct ClockTag {};
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
    LU_LOGE("main15", "findFont", "No font found. Pass a .ttf file as the first argument.");
    return {};
}

struct TextDemoModule {
    // parent (Transform2D) + child (Text)
    static flecs::entity createText(flecs::world& world, const char* name, const std::string& string,
                                    const Lu::Component::Transform2D& transform, const glm::vec4& color,
                                    float size, Lu::TextAlign align = Lu::TextAlign::Left,
                                    float lineSpacing = 1.0f, float maxWidth = 0.0f) {
        const auto parent = world.entity(name).set<Lu::Component::Transform2D>(transform);
        return world.entity((std::string(name) + "::Text").c_str())
            .child_of(parent)
            .set(Lu::Component::Text{
                .text = string,
                .font = world.lookup("Font::Main"),
                .pipeline = world.lookup("FontPipe::Basic"),
                .color = color,
                .size = size,
                .lineSpacing = lineSpacing,
                .align = align,
                .maxWidth = maxWidth
            });
    }

    explicit TextDemoModule(flecs::world& world) {
        // ---- 3D scene: a rotating cube -------------------------------------------------------
        world.import<Lu::Module::FlyingCamera>();
        world.entity("Camera")
            .set(Lu::Component::FlyingCamera{
                .position = glm::vec3(0.0f, 0.0f, 5.0f),
                .angle = Lu::Component::FlyingCamera::lookAt(glm::vec3(0.0f, 0.0f, 5.0f), glm::vec3(0.0f)),
                .speed = 5.0f,
                .rotationSpeed = 0.005f,
                .fov = glm::radians(70.0f),
                .nearClip = 0.1f,
                .farClip = 100.0f
            })
            .add<Lu::Component::Transform>();

        const auto cube = world.entity("Cube")
            .set<Lu::Component::Transform>({
                glm::vec3(0.0f), 
                0.0f, glm::vec3(0.0f, 1.0f, 0.0f), 
                glm::vec3(2.0f)})
            .add<Lu::Component::TransformGpu>();
        world.entity("CubeMesh")
            .child_of(cube)
            .set(Lu::Component::Mesh{ 
                .mesh = world.lookup("Mesh::Cube"), 
                .pipeline = world.lookup("MeshPipe::Basic") 
            })
            .set(Lu::Component::Material{ 
                .albedo = glm::vec4(0.25f, 0.7f, 0.95f, 1.0f), 
                .ambient = glm::vec4(0.3f) 
            });
        
        world.system<Lu::Component::Transform>("RotateCube")
            .with<Lu::Component::TransformGpu>()
            .each([](flecs::iter& it, size_t, Lu::Component::Transform& transform) {
                transform.rotation = 
                glm::angleAxis(
                    glm::radians(30.0f) * it.delta_time(), 
                    glm::vec3(0.0f, 1.0f, 0.0f)
                ) * transform.rotation;
            });

        // ---- 2D ---------------------------------------------------------------------------------
        // Optional. Position = world point shown at `pivot` (fraction of the window), zoom/rotation around the pivot.
        world.entity("Camera2D").set<Lu::Component::Camera2D>({});

        // static title, top left
        createText(
            world, 
            "Title", 
            "LuEngine Text", 
            {.position = {24.0f, 16.0f}, .positionZ = 10},
            glm::vec4(1.0f, 0.9f, 0.3f, 1.0f), 
            56.0f
        );

        // a string that changes: every change is a new EcsRequest::CreateText (same length -> same FontBuffer range)
        createText(
            world, 
            "Clock", 
            "Time:   0.0 s", 
            {.position = {24.0f, 84.0f}, .positionZ = 10},
            glm::vec4(0.8f, 0.9f, 1.0f, 1.0f), 
            28.0f
        )
        .add<ClockTag>();

        world.system<Lu::Component::Text>("UpdateClock")
            .with<ClockTag>()
            .interval(0.1f)
            .each([](flecs::entity e, Lu::Component::Text& text) {
                char buffer[64];
                std::snprintf(buffer, sizeof(buffer), "Time: %5.1f s", e.world().get_info()->world_time_total);
                text.text = buffer;
                e.modified<Lu::Component::Text>();
            });

        // UTF-8 + multiple lines + line spacing
        createText(
            world, 
            "Multi", 
            "Gr\xC3\xBC\xC3\x9F" "e, \xC3\x84pfel \xE2\x82\xAC 1\xC2\xBD \xC2\xA9\nsecond line\n  third line, spacing 1.3",
            {.position = {24.0f, 130.0f}, .positionZ = 10}, 
            glm::vec4(1.0f), 
            24.0f, 
            Lu::TextAlign::Left, 
            1.3f
        );

        // word wrapping: maxWidth = width of the paragraph box in pixels. The same text, left / centered / right aligned.
        // For Center / Right the Transform2D position is the center / right edge of the box.
        const std::string paragraph =
            "Word wrapping breaks a line at the last space that fits into maxWidth. "
            "Verylongwordsthatdonotfitintotheboxarebrokenbetweencharacters.\nAn explicit newline still works.";
        createText(world, "WrapLeft", paragraph, {.position = {24.0f, 280.0f}, .positionZ = 10},
                   glm::vec4(0.7f, 1.0f, 0.7f, 1.0f), 20.0f, Lu::TextAlign::Left, 1.0f, 300.0f);
        createText(world, "WrapCenter", paragraph, {.position = {24.0f + 150.0f, 480.0f}, .positionZ = 10},
                   glm::vec4(0.7f, 1.0f, 0.7f, 1.0f), 20.0f, Lu::TextAlign::Center, 1.0f, 300.0f);
        createText(world, "WrapRight", paragraph, {.position = {24.0f + 300.0f, 680.0f}, .positionZ = 10},
                   glm::vec4(0.7f, 1.0f, 0.7f, 1.0f), 20.0f, Lu::TextAlign::Right, 1.0f, 300.0f);

        // centered text that rotates and is scaled; the pivot of the rotation is the Transform2D position
        createText(
            world, 
            "Spin", 
            "Rotating & scaled", 
            {.position = {640.0f, 400.0f}, .positionZ = 20, .scale = {1.4f, 1.4f}},
            glm::vec4(1.0f, 0.4f, 0.4f, 1.0f), 
            48.0f, 
            Lu::TextAlign::Center
        );

        world.lookup("Spin").add<SpinTag>();
        world.system<Lu::Component::Transform2D>("SpinText")
            .with<SpinTag>()
            .each([](flecs::iter& it, size_t, Lu::Component::Transform2D& transform) {
                transform.setAngle(transform.getAngle() + 0.6f * it.delta_time());   // picked up by the Transform2D triple buffer
            });

        // two overlapping texts: positionZ decides which one is in front, independent of creation order
        createText(
            world, 
            "LayerFront", 
            "FRONT (z = 2)", 
            {.position = {900.0f, 640.0f}, .positionZ = 2},
            glm::vec4(0.1f, 1.0f, 0.3f, 1.0f), 
            56.0f
        );

        createText(
            world, 
            "LayerBack", 
            "BACK (z = 1)", 
            {.position = {920.0f, 660.0f}, .positionZ = 1},
            glm::vec4(1.0f, 0.2f, 0.9f, 1.0f), 
            56.0f
        );

        // right aligned, bottom left
        createText(
            world, 
            "Right", 
            "right aligned", 
            {.position = {1256.0f, 16.0f}, .positionZ = 10},
            glm::vec4(1.0f), 
            28.0f, 
            Lu::TextAlign::Right
        );
    }
};

int main(int argc, char** argv) {
    const std::string fontPath = findFont(argc, argv);

    Lu::App("LuEngine Text", false, 1280, 800)
        .registerMeshPipe(Lu::GraphicsPipelineConfig{
            .name = "MeshPipe::Basic",
            .capacity = 4,
            .vertexShader = "shader/mesh_basic.vert.spv",
            .fragmentShader = "shader/mesh_basic.frag.spv"
        })
        .registerShape<Lu::Shape::Cube>(
            "Mesh::Cube", 
            glm::vec3(0.5f), 
            glm::vec3(0.0f), 
            glm::vec4(1.0f)
        )
        // glyphs are rasterized at 64 px into a signed distance field atlas, so they stay sharp at any size / rotation
        .registerFont(
            "Font::Main", 
            fontPath, 
            64
        )
        .registerFontPipe(Lu::GraphicsPipelineConfig{
            .name = "FontPipe::Basic",
            .capacity = 4096,                                  // max glyphs drawn per frame by this pipeline
            .vertexShader = "shader/font_basic.vert.spv",
            .fragmentShader = "shader/font_basic.frag.spv",
            .cullMode = VK_CULL_MODE_NONE,
            .depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL      // glyphs of the same layer overlap slightly
        })
        .importModule<TextDemoModule>()
        .run();
}
