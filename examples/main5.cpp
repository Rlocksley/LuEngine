#include "App.hpp"
#include "Shape.hpp"
#include "FlyingCamera.hpp"
#include "component/Material.hpp"
#include "component/Mesh.hpp"
#include "component/MultiMesh.hpp"
#include "component/Transform.hpp"
#include "component/TransformGpu.hpp"

// =============================================================================
// main5: AAA style fire particle system built from four MultiMeshes.
//
// All four layers use the same geometry (Shape::Square), the same compute shader
// (multi_mesh_fire_move.comp) and the same vertex shader (multi_mesh_fire.vert);
// they differ in their particle `kind` and in their fragment shader:
//
//   Smoke  multi_mesh_fire_smoke.frag   dark puffs, lit orange from below
//   Flame  multi_mesh_fire.frag         SDF flame tongues, temperature colours
//   Spark  multi_mesh_fire_spark.frag   velocity-stretched, twinkling embers
//   Glow   multi_mesh_fire_glow.frag    a few huge faint halos (fake bloom)
//
// Draw order in the engine == registration order of the MultiMesh pipelines, so
// smoke is registered first (drawn first), the light emitting layers afterwards.
//
// Controls: WASD + hold right mouse button (FlyingCamera).
//
// The per particle state lives inside the MultiMeshInstance (the instance buffer
// is ping-ponged by the engine), see the slot table in multi_mesh_fire_move.comp.
// The CPU only fills in the constant slots:
//     material.texturesIds = (particle index, -1 => "spawn + pre-warm", kind, 0)
// =============================================================================

namespace FireKind {
    constexpr int Flame = 0;
    constexpr int Smoke = 1;
    constexpr int Spark = 2;
    constexpr int Glow  = 3;
}

struct FireModule {
    // Creates one fire layer (a MultiMesh child of `parent`).
    static void createLayer(flecs::world& world, flecs::entity parent, const char* name,
                            int kind, uint32_t count, const glm::vec4& cullSphere,
                            const char* graphicsPipe) {
        std::vector<Lu::Component::MultiMeshInstance> instances;
        instances.reserve(count);
        for (uint32_t i = 0; i < count; ++i) {
            Lu::Component::MultiMeshInstance instance{};
            instance.transform.position = glm::vec3(0.0f);
            instance.transform.scale = glm::vec3(0.001f);        // invisible until the first compute pass
            instance.material.texturesIds = glm::ivec4(static_cast<int>(i), -1, kind, 0);
            instance.material.albedo = glm::vec4(1.0f);
            instance.material.ambient = glm::vec4(0.0f);
            instance.material.emission = glm::vec4(0.0f);        // age = 0, life = 0, seed = 0, clock = 0
            instances.push_back(std::move(instance));
        }

        world.entity(name)
            .child_of(parent)
            .set(Lu::Component::MultiMesh{
                world.lookup("Mesh::FireSquare"),
                world.lookup("MultiMeshCompute::FireMove"),
                world.lookup(graphicsPipe),
                std::move(instances),
                cullSphere       // xyz = centre (parent local), w = radius: the whole layer is culled with it
            });
    }

    static void createLog(flecs::world& world, const std::string& name, float angle, float radius) {
        const glm::vec3 position(radius * std::cos(angle), 0.30f, radius * std::sin(angle));
        // long axis of the cube mesh is +Z: yaw it towards the centre and lean it up like a tepee
        const float yaw = std::atan2(-std::cos(angle), -std::sin(angle));
        const float lean = 0.85f;

        Lu::Component::Transform transform{};
        transform.position = position;
        transform.rotation = glm::angleAxis(yaw, glm::vec3(0, 1, 0)) * glm::angleAxis(-lean, glm::vec3(1, 0, 0));
        transform.scale = glm::vec3(0.16f, 0.16f, 1.25f);

        const auto parent = world.entity(name.c_str())
            .set<Lu::Component::Transform>(transform)
            .add<Lu::Component::TransformGpu>();

        world.entity((name + "_Mesh").c_str())
            .child_of(parent)
            .set(Lu::Component::Mesh{
                .mesh = world.lookup("Mesh::Cube"),
                .pipeline = world.lookup("MeshPipe::Basic")
            })
            .set(Lu::Component::Material{
                .albedo = glm::vec4(0.10f, 0.055f, 0.03f, 1.0f),
                .ambient = glm::vec4(0.01f, 0.008f, 0.006f, 1.0f),
                .emission = glm::vec4(0.22f, 0.045f, 0.008f, 1.0f)   // glowing embers on the wood
            });
    }

    explicit FireModule(flecs::world& world) {
        world.import<Lu::Module::FlyingCamera>();

        world.entity("Camera")
            .set(Lu::Component::FlyingCamera{
                .position = glm::vec3(0.0f, 1.6f, 5.0f),
                .angle = Lu::Component::FlyingCamera::lookAt(
                    glm::vec3(0.0f, 1.6f, 5.0f), glm::vec3(0.0f, 1.1f, 0.0f)),
                .speed = 4.0f,
                .rotationSpeed = 0.005f,
                .fov = glm::radians(65.0f),
                .nearClip = 0.05f,
                .farClip = 200.0f
            })
            .add<Lu::Component::Transform>();

        // ---- ground -----------------------------------------------------------
        {
            Lu::Component::Transform t{};
            t.position = glm::vec3(0.0f, -0.05f, 0.0f);
            t.scale = glm::vec3(30.0f, 0.1f, 30.0f);
            const auto ground = world.entity("Ground")
                .set<Lu::Component::Transform>(t)
                .add<Lu::Component::TransformGpu>();
            world.entity("GroundMesh")
                .child_of(ground)
                .set(Lu::Component::Mesh{
                    .mesh = world.lookup("Mesh::Cube"),
                    .pipeline = world.lookup("MeshPipe::Basic")
                })
                .set(Lu::Component::Material{
                    .albedo = glm::vec4(0.035f, 0.03f, 0.025f, 1.0f),
                    .ambient = glm::vec4(0.004f, 0.004f, 0.005f, 1.0f)
                });
        }

        // ---- logs -------------------------------------------------------------
        constexpr int logCount = 6;
        for (int i = 0; i < logCount; ++i) {
            createLog(world, "Log_" + std::to_string(i),
                      6.2831853f * static_cast<float>(i) / logCount + 0.3f, 0.55f);
        }

        // ---- the fire -----------------------------------------------------------
        // The parent places (and may move / rotate / scale) the whole fire. Noise is
        // evaluated in the parent's local space, so the flames travel with it.
        const auto fire = world.entity("Fire")
            .set<Lu::Component::Transform>({})
            .add<Lu::Component::TransformGpu>();

        //                                      kind              count  cullSphere (local)          pipeline
        createLayer(world, fire, "FireSmoke", FireKind::Smoke, 160, {0.0f, 2.5f, 0.0f, 5.0f}, "MultiMeshPipe::FireSmoke");
        createLayer(world, fire, "FireFlame", FireKind::Flame, 160, {0.0f, 1.0f, 0.0f, 2.6f}, "MultiMeshPipe::FireFlame");
        createLayer(world, fire, "FireSpark", FireKind::Spark,  50, {0.0f, 2.0f, 0.0f, 5.0f}, "MultiMeshPipe::FireSpark");
        createLayer(world, fire, "FireGlow",  FireKind::Glow,    14, {0.0f, 0.8f, 0.0f, 3.5f}, "MultiMeshPipe::FireGlow");
    }
};

// Premultiplied alpha blending (see GraphicsPipelineConfig):
//   result = src.rgb + dst.rgb * (1 - src.a)
// src.a = 0  -> purely additive light (sparks, glow)
// src.a = a  -> "over" with emitted light (flames, smoke)
// Depth is tested against the opaque scene but not written, quads are two sided.
static Lu::GraphicsPipelineConfig firePipe(const char* name, const char* fragmentShader) {
    Lu::GraphicsPipelineConfig config{};
    config.name = name;
    config.capacity = Lu::Core::MAX_MULTI_MESH_INSTANCES;
    config.vertexShader = "shader/multi_mesh_fire.vert.spv";
    config.fragmentShader = fragmentShader;
    config.cullMode = VK_CULL_MODE_NONE;
    config.depthTestEnable = true;
    config.depthWriteEnable = false;
    config.blendEnable = true;
    config.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
    config.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    return config;
}

int main() {
    Lu::App("LuEngine Fire", false, 1600, 1000)
        // opaque scene
        .registerMeshPipe(Lu::GraphicsPipelineConfig{
            .name = "MeshPipe::Basic",
            .capacity = 64,
            .vertexShader = "shader/mesh_basic.vert.spv",
            .fragmentShader = "shader/mesh_basic.frag.spv"
        })
        // fire layers: registration order == draw order (back to front)
        .registerMultiMeshPipe(firePipe("MultiMeshPipe::FireSmoke", "shader/multi_mesh_fire_smoke.frag.spv"))
        .registerMultiMeshPipe(firePipe("MultiMeshPipe::FireFlame", "shader/multi_mesh_fire.frag.spv"))
        .registerMultiMeshPipe(firePipe("MultiMeshPipe::FireSpark", "shader/multi_mesh_fire_spark.frag.spv"))
        .registerMultiMeshPipe(firePipe("MultiMeshPipe::FireGlow",  "shader/multi_mesh_fire_glow.frag.spv"))
        // one compute shader moves all four layers (particle kind is stored per instance)
        .registerMultiMeshComputePipe(Lu::ComputePipelineConfig{
            .name = "MultiMeshCompute::FireMove",
            .computeShader = "shader/multi_mesh_fire_move.comp.spv"
        })
        // geometry
        .registerShape<Lu::Shape::Square>("Mesh::FireSquare", 0.5f, glm::vec3(0.0f), glm::vec4(1.0f))
        .registerShape<Lu::Shape::Cube>("Mesh::Cube", glm::vec3(0.5f), glm::vec3(0.0f), glm::vec4(1.0f))
        .importModule<FireModule>()
        .run();
}
