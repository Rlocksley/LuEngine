#include "App.hpp"
#include "Shape.hpp"
#include "Shape2D.hpp"
#include "FlyingCamera.hpp"
#include "component/Mesh.hpp"
#include "component/Mesh2D.hpp"
#include "component/Video.hpp"
#include "component/Material.hpp"
#include "component/InputState.hpp"
#include "component/Transform.hpp"
#include "component/TransformGpu.hpp"
#include "component/Transform2D.hpp"
#include "component/Transform2DGpu.hpp"

// =============================================================================
// main_video: one live camera video stream, shown on a Mesh2D (HUD) AND on a 3D mesh.
//
//   App::registerVideoStream(name, deviceIndex) captures the camera stream in real time
//                                              via ccap and feeds it into the VideoDecoder
//   Component::VideoTexture                    add it to a Mesh2D (or Mesh) entity -> that surface shows the stream
//   Component::VideoPlayback                   on the video entity: playing / loop / speed (SPACE toggles playing below)
//
// Usage: lu_example_video [deviceIndex]        SPACE = pause / resume
// =============================================================================

struct SpinTag {};

struct VideoDemoModule {
    explicit VideoDemoModule(flecs::world& world) {
        const flecs::entity video = world.lookup("Video::Demo");

        // ---- 2D: a parent with Transform2D + Transform2DGpu, a child Mesh2D with a VideoTexture -------------------
        const auto screen2D = world.entity("Screen2D")
            .set<Lu::Component::Transform2D>({.position = {20.f, 20.f}, .positionZ = 1})
            .add<Lu::Component::Transform2DGpu>();

        world.entity("Screen2D::Surface")
            .child_of(screen2D)
            .set(Lu::Component::Mesh2D{
                .mesh = world.lookup("Mesh2D::Screen"),
                .pipeline = world.lookup("Mesh2DPipe::Basic")
            })
            .set(Lu::Component::VideoTexture{.video = video});      // <- that's all

        // ---- 3D: a cube, the video stream is its emissive texture --------------------------------------------------
        world.entity("Camera")
            .set(Lu::Component::FlyingCamera{
                .position = glm::vec3(0.0f, 0.0f, -4.0f),
                .angle = Lu::Component::FlyingCamera::lookAt(glm::vec3(0.0f, 0.0f, -4.0f), glm::vec3(0.0f)),
                .speed = 5.0f,
                .rotationSpeed = 0.005f,
                .fov = glm::radians(70.0f),
                .nearClip = 0.1f,
                .farClip = 100.0f
            })
            .add<Lu::Component::Transform>();

        const auto cube = world.entity("Cube")
            .add<SpinTag>()
            .set<Lu::Component::Transform>({glm::vec3(0.f, 0.f, 0.f), 3.14f, glm::vec3(1.f, 0.f, 0.f), glm::vec3(480.f, 270.f, 480.f)/480.f})
            .add<Lu::Component::TransformGpu>();

        world.entity("Cube::Mesh")
            .child_of(cube)
            .set(Lu::Component::Mesh{
                .mesh = world.lookup("Mesh::Cube"),
                .pipeline = world.lookup("MeshPipe::Basic")
            })
            .set(Lu::Component::Material{
                .albedo = glm::vec4(0.15f, 0.15f, 0.18f, 1.f),
                .ambient = glm::vec4(0.1f, 0.1f, 0.1f, 1.f)
            })
            .set(Lu::Component::VideoTexture{.video = video});      // <- that's all

        world.system<Lu::Component::Transform>("SpinCube")
            .with<SpinTag>()
            .each([](flecs::iter& it, size_t, Lu::Component::Transform& transform) {
                transform.rotation = glm::angleAxis(glm::radians(25.f) * it.delta_time(), glm::vec3(0.f, 1.f, 0.f)) * transform.rotation;
            });

        // ---- SPACE: pause / resume (Component::VideoPlayback of the video entity) -----------------------------------
        world.system<const Lu::Component::InputState>("TogglePlayback")
            .each([video](const Lu::Component::InputState& input) {
                if (input.wasKeyPressedThisFrame(Lu::KeyCode::keySpace)) {
                    auto playback = video.get<Lu::Component::VideoPlayback>();
                    video.set<Lu::Component::VideoPlayback>({.playing = !playback.playing, .loop = playback.loop, .speed = playback.speed});
                }
            });
    }
};

int main(int argc, char** argv) {
    int deviceIndex = 0;
    if (argc > 1) {
        try {
            deviceIndex = std::stoi(argv[1]);
        } catch (...) {}
    }

    Lu::App("LuEngine Video Stream", false, 1280, 800)
        .registerVideoStream("Video::Demo", deviceIndex)                          // 1) register camera video stream
        .registerShape2D<Lu::Shape2D::Rectangle>("Mesh2D::Screen", glm::vec2(480.f, 270.f))
        .registerMesh2DPipe(Lu::GraphicsPipelineConfig{
            .name = "Mesh2DPipe::Basic",
            .capacity = 16,
            .vertexShader = "shader/mesh2d_basic.vert.spv",
            .fragmentShader = "shader/mesh2d_basic.frag.spv",
            .depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL
        })
        .registerMeshPipe(Lu::GraphicsPipelineConfig{
            .name = "MeshPipe::Basic",
            .capacity = 4,
            .vertexShader = "shader/mesh_basic.vert.spv",
            .fragmentShader = "shader/mesh_basic.frag.spv"
        })
        .registerShape<Lu::Shape::Cube>("Mesh::Cube", glm::vec3(1.0f), glm::vec3(0.0f), glm::vec4(1.0f))
        .importModule<Lu::Module::FlyingCamera>()
        .importModule<VideoDemoModule>()                                          // 2) VideoTexture on Mesh2D / Mesh
        .run();
}
