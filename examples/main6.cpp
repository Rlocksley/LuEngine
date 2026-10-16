#include "App.hpp"
#include "Shape.hpp"
#include "FlyingCamera.hpp"
#include "component/Light.hpp"
#include "component/Material.hpp"
#include "component/Mesh.hpp"
#include "component/Transform.hpp"
#include "component/TransformGpu.hpp"

namespace {

void addCube(flecs::world& world, const std::string& name,
    const glm::vec3& position, const glm::vec3& scale,
    const glm::vec4& albedo, const flecs::entity& mesh,
    const flecs::entity& pipeline) {
    Lu::Component::Transform transform{};
    transform.position = position;
    transform.scale = scale;

    const flecs::entity cube = world.entity((name + "Transform").c_str())
        .set<Lu::Component::Transform>(transform)
        .add<Lu::Component::TransformGpu>();

    world.entity((name + "Mesh").c_str())
        .set(flecs::Parent{cube})
        .set(Lu::Component::Mesh{.mesh = mesh, .pipeline = pipeline})
        .set(Lu::Component::Material{
            .albedo = albedo,
            .ambient = glm::vec4(0.025f, 0.025f, 0.025f, 1.0f),
            .roughness = 0.01f,
            .metallic = 0.01f
        });
}

void addLight(flecs::world& world, const char* name, Lu::LightType type,
    const glm::vec3& position, const glm::vec3& target,
    const glm::vec3& color, float intensity, float radius,
    float innerCutoff = 0.0f, float outerCutoff = 0.0f) {
    Lu::Component::Transform transform{};
    transform.position = position;
    if (type != Lu::LightType::Point) {
        const glm::vec3 direction = glm::normalize(target - position);
        transform.rotation = glm::rotation(glm::vec3(0.0f, 0.0f, -1.0f), direction);
    }

    const std::string transformName = std::string(name) + "Transform";
    const flecs::entity lightTransform = world.entity(transformName.c_str())
        .set<Lu::Component::Transform>(transform)
        .add<Lu::Component::TransformGpu>();

    world.entity(name)
        .set(flecs::Parent{lightTransform})
        .set(Lu::Component::Light{
            .type = type,
            .color = color,
            .intensity = intensity,
            .radius = radius,
            .innerCutoff = innerCutoff,
            .outerCutoff = outerCutoff
        });
}

struct LightingScene {
    explicit LightingScene(flecs::world& world) {
        world.import<Lu::Module::FlyingCamera>();

        constexpr glm::vec3 cameraPosition(9.0f, 9.0f, 13.0f);
        world.entity("LightingCamera")
            .set(Lu::Component::FlyingCamera{
                .position = cameraPosition,
                .angle = Lu::Component::FlyingCamera::lookAt(
                    cameraPosition, glm::vec3(0.0f, 0.4f, 0.0f)),
                .speed = 6.0f,
                .rotationSpeed = 0.005f,
                .fov = glm::radians(60.0f),
                .nearClip = 0.1f,
                .farClip = 100.0f
            })
            .add<Lu::Component::Transform>();

        const flecs::entity cubeMesh = world.lookup("Mesh::Cube");
        const flecs::entity meshPipeline = world.lookup("MeshPipe::Lighting");
        addCube(world, "LightingFloor",
            glm::vec3(0.0f, -0.25f, 0.0f), glm::vec3(12.0f, 0.5f, 10.0f),
            glm::vec4(0.62f, 0.64f, 0.66f, 1.0f), cubeMesh, meshPipeline);

        struct CubeSpec {
            const char* name;
            glm::vec3 position;
            glm::vec3 scale;
            glm::vec4 color;
        };
        const std::array<CubeSpec, 7> cubes{{
            {"Small",  glm::vec3(-4.2f, 0.35f, -2.6f), glm::vec3(0.7f),              glm::vec4(0.9f, 0.18f, 0.08f, 1.0f)},
            {"Medium", glm::vec3(-2.6f, 0.55f, -2.4f), glm::vec3(1.1f),              glm::vec4(0.08f, 0.55f, 0.9f, 1.0f)},
            {"Tall",   glm::vec3(-0.5f, 0.9f, -2.5f),  glm::vec3(0.8f, 1.8f, 0.8f), glm::vec4(0.2f, 0.8f, 0.24f, 1.0f)},
            {"Wide",   glm::vec3(-4.1f, 0.55f, 0.0f),  glm::vec3(1.5f, 1.1f, 1.1f), glm::vec4(0.95f, 0.65f, 0.08f, 1.0f)},
            {"Large",  glm::vec3(-1.8f, 0.7f, 0.0f),   glm::vec3(1.4f),              glm::vec4(0.65f, 0.18f, 0.8f, 1.0f)},
            {"Tall2",  glm::vec3(0.3f, 0.8f, 2.1f),     glm::vec3(1.1f, 1.6f, 1.1f), glm::vec4(0.08f, 0.75f, 0.68f, 1.0f)},
            {"Small2", glm::vec3(2.4f, 0.4f, 2.2f),     glm::vec3(0.8f),              glm::vec4(0.95f, 0.35f, 0.5f, 1.0f)}
        }};

        for (const CubeSpec& cube : cubes) {
            addCube(world, std::string("LightingCube") + cube.name,
                cube.position, cube.scale, cube.color, cubeMesh, meshPipeline);
        }

        addLight(world, "SubtleDirectionalLight", Lu::LightType::Directional,
            glm::vec3(-4.0f, 8.0f, 5.0f), glm::vec3(0.0f, 0.0f, 0.0f),
            glm::vec3(1.0f), 0.05f, 100.0f);

        addLight(world, "WarmPointLight", Lu::LightType::Point,
            glm::vec3(-3.2f, 2.8f, 1.4f), glm::vec3(0.0f),
            glm::vec3(1.0f, 0.58f, 0.28f), 10.0f, 7.0f);

        addLight(world, "CoolSpotLight", Lu::LightType::Spot,
            glm::vec3(3.5f, 6.0f, 0.5f), glm::vec3(3.5f, 0.0f, -1.0f),
            glm::vec3(0.55f, 0.72f, 1.0f), 12.5f, 10.0f,
            glm::radians(16.0f), glm::radians(24.0f));
    }
};

}

int main() {
    Lu::App("LuEngine Lighting Test")
        .registerMeshPipe(Lu::GraphicsPipelineConfig{
            .name = "MeshPipe::Lighting",
            .capacity = 128,
            .vertexShader = "shader/mesh_basic.vert.spv",
            .fragmentShader = "shader/mesh_pbr.frag.spv"
        })
        .registerShape<Lu::Shape::Cube>(
            "Mesh::Cube",
            glm::vec3(0.5f),
            glm::vec3(0.0f),
            glm::vec4(1.0f)
        )
        .importModule<LightingScene>()
        .run();
}
