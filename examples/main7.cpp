#include "App.hpp"
#include "Shape.hpp"
#include "FlyingCamera.hpp"
#include "component/Light.hpp"
#include "component/Material.hpp"
#include "component/Mesh.hpp"
#include "component/Transform.hpp"
#include "component/TransformGpu.hpp"

#include <cmath>
#include <random>
#include <string>

namespace {

glm::vec3 randomColor(std::mt19937& _random, float saturationMin,
    float saturationMax, float valueMin, float valueMax) {
    std::uniform_real_distribution<float> hueDistribution(0.0f, 360.0f);
    std::uniform_real_distribution<float> saturationDistribution(saturationMin, saturationMax);
    std::uniform_real_distribution<float> valueDistribution(valueMin, valueMax);

    const float hueSector = hueDistribution(_random) / 60.0f;
    const float saturation = saturationDistribution(_random);
    const float value = valueDistribution(_random);
    const float chroma = value * saturation;
    const float secondary = chroma *
        (1.0f - std::fabs(std::fmod(hueSector, 2.0f) - 1.0f));
    glm::vec3 color;

    if (hueSector < 1.0f) {
        color = {chroma, secondary, 0.0f};
    } else if (hueSector < 2.0f) {
        color = {secondary, chroma, 0.0f};
    } else if (hueSector < 3.0f) {
        color = {0.0f, chroma, secondary};
    } else if (hueSector < 4.0f) {
        color = {0.0f, secondary, chroma};
    } else if (hueSector < 5.0f) {
        color = {secondary, 0.0f, chroma};
    } else {
        color = {chroma, 0.0f, secondary};
    }

    return color + glm::vec3(value - chroma);
}

void addCube(flecs::world& world, const std::string& name,
    const glm::vec3& position, const glm::vec3& scale,
    const glm::vec4& albedo, const flecs::entity& cubeMesh,
    const flecs::entity& meshPipeline) {
    Lu::Component::Transform transform{};
    transform.position = position;
    transform.scale = scale;

    const flecs::entity cubeTransform = world.entity((name + "Transform").c_str())
        .set<Lu::Component::Transform>(transform)
        .add<Lu::Component::TransformGpu>();

    world.entity((name + "Mesh").c_str())
        .set(flecs::Parent{cubeTransform})
        .set(Lu::Component::Mesh{.mesh = cubeMesh, .pipeline = meshPipeline})
        .set(Lu::Component::Material{
            .albedo = albedo,
            .ambient = glm::vec4(0.003f, 0.003f, 0.003f, 1.0f),
            .roughness = 0.7f,
            .metallic = 0.0f
        });
}

void addPointLight(flecs::world& world, const std::string& name,
    const glm::vec3& position, const glm::vec3& color, float intensity,
    float radius) {
    Lu::Component::Transform transform{};
    transform.position = position;

    const flecs::entity lightTransform = world.entity((name + "Transform").c_str())
        .set<Lu::Component::Transform>(transform)
        .add<Lu::Component::TransformGpu>();

    world.entity(name.c_str())
        .set(flecs::Parent{lightTransform})
        .set(Lu::Component::Light{
            .type = Lu::LightType::Point,
            .color = color,
            .intensity = intensity,
            .radius = radius
        });
}

struct LightCullingScene {
    explicit LightCullingScene(flecs::world& world) {
        world.import<Lu::Module::FlyingCamera>();

        std::mt19937 _random(std::random_device{}());
        std::uniform_real_distribution<float> cubeHeightDistribution(0.5f, 1.4f);
        std::uniform_real_distribution<float> lightOffsetDistribution(-0.38f, 0.38f);
        std::uniform_real_distribution<float> lightHeightDistribution(0.85f, 1.35f);
        std::uniform_real_distribution<float> lightIntensityDistribution(15.0f, 22.0f);
        std::uniform_real_distribution<float> lightRadiusDistribution(1.9f, 2.15f);

        const glm::vec3 cameraPosition(0.0f, 52.0f, 82.0f);
        world.entity("LightCullingCamera")
            .set(Lu::Component::FlyingCamera{
                .position = cameraPosition,
                .angle = Lu::Component::FlyingCamera::lookAt(
                    cameraPosition, glm::vec3(0.0f, 0.0f, 0.0f)),
                .speed = 16.0f,
                .rotationSpeed = 0.005f,
                .fov = glm::radians(65.0f),
                .nearClip = 0.1f,
                .farClip = 220.0f
            })
            .add<Lu::Component::Transform>();

        const flecs::entity cubeMesh = world.lookup("Mesh::LightCullingCube");
        const flecs::entity meshPipeline = world.lookup("MeshPipe::LightCulling");

        addCube(world, "LightCullingGround",
            glm::vec3(0.0f, -0.3f, 0.0f), glm::vec3(100.0f, 0.5f, 100.0f),
            glm::vec4(0.38f, 0.40f, 0.44f, 1.0f), cubeMesh, meshPipeline);

        constexpr uint32_t cubeRows = 64;
        constexpr float cubeSpacing = 1.32f;
        constexpr float cubeOffset = (cubeRows - 1) * cubeSpacing * 0.5f;

        for (uint32_t z = 0; z < cubeRows; ++z) {
            for (uint32_t x = 0; x < cubeRows; ++x) {
                const uint32_t index = z * cubeRows + x;
                const float height = cubeHeightDistribution(_random);
                const glm::vec3 position(
                    static_cast<float>(x) * cubeSpacing - cubeOffset,
                    height * 0.5f,
                    static_cast<float>(z) * cubeSpacing - cubeOffset
                );
                const glm::vec3 scale(0.48f, height, 0.48f);
                const glm::vec3 color = randomColor(_random, 0.45f, 0.82f, 0.45f, 0.92f);

                addCube(world, "LightCullingCube" + std::to_string(index),
                    position, scale, glm::vec4(color, 1.0f),
                    cubeMesh, meshPipeline);
            }
        }

        constexpr uint32_t lightRows = 28;
        constexpr float lightSpacing = 3.05f;
        constexpr float lightOffset = (lightRows - 1) * lightSpacing * 0.5f;

        for (uint32_t z = 0; z < lightRows; ++z) {
            for (uint32_t x = 0; x < lightRows; ++x) {
                const uint32_t index = z * lightRows + x;
                const float y = lightHeightDistribution(_random);
                const glm::vec3 position(
                    static_cast<float>(x) * lightSpacing - lightOffset +
                        lightOffsetDistribution(_random),
                    y,
                    static_cast<float>(z) * lightSpacing - lightOffset +
                        lightOffsetDistribution(_random)
                );
                const glm::vec3 color = randomColor(_random, 0.82f, 1.0f, 0.9f, 1.0f);

                addPointLight(world, "LightCullingPoint" + std::to_string(index),
                    position, color, lightIntensityDistribution(_random),
                    lightRadiusDistribution(_random));
            }
        }
    }
};

}

int main() {
    Lu::App("LuEngine Tiled Light Culling Test", true)
        .registerMeshPipe(Lu::GraphicsPipelineConfig{
            .name = "MeshPipe::LightCulling",
            .capacity = 8192,
            .vertexShader = "shader/mesh_basic.vert.spv",
            .fragmentShader = "shader/mesh_basic.frag.spv"
        })
        .registerShape<Lu::Shape::Cube>(
            "Mesh::LightCullingCube",
            glm::vec3(0.5f),
            glm::vec3(0.0f),
            glm::vec4(1.0f)
        )
        .importModule<LightCullingScene>()
        .run();
}
