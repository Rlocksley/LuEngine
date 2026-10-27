#include "App.hpp"
#include "Shape.hpp"
#include "FlyingCamera.hpp"
#include "PhysicsModule.hpp"
#include "component/Light.hpp"
#include "component/Material.hpp"
#include "component/Mesh.hpp"
#include "component/Transform.hpp"
#include "component/TransformGpu.hpp"

#include <cstdint>
#include <memory>
#include <string>

using Lu::Component::Transform;
using Lu::Component::InputState;
using Lu::Component::FlyingCamera;
using Lu::Component::BodyKind;
using Lu::Component::RigidBodyDesc;
using Lu::Component::ShapeKind;

constexpr int kCubeCount = 500;
constexpr int kCubesPerSide = 10;
constexpr int kCubeLayers = kCubeCount / (kCubesPerSide * kCubesPerSide);
constexpr float kCubeHalfExtent = 0.45f;
constexpr float kCubeSpacing = 1.0f;
constexpr float kFloorHalfWidth = 14.0f;
constexpr float kFloorHalfHeight = 0.5f;


Lu::Component::Material makeMaterial(const glm::vec3& color) {
    Lu::Component::Material material{};
    material.albedo = glm::vec4(color, 1.0f);
    material.ambient = glm::vec4(color * 0.08f, 1.0f);
    material.roughness = 0.1;//random(0.1,0.9);
    material.metallic = random(0.8,0.9);
    return material;
}

flecs::entity spawnPhysicsCube(flecs::world world, const char* name,
    const glm::vec3& position, const glm::vec3& visualScale,
    const glm::vec3& physicsHalfExtents, Lu::Component::BodyKind bodyKind,
    const glm::vec3& color, const flecs::entity& cubeMesh,
    const flecs::entity& meshPipeline,
    const glm::vec3& initialVelocity = glm::vec3(0.0f)) {

    Transform transform{};
    transform.position = position;
    transform.scale = visualScale;

    const flecs::entity body = world.entity(name)
        .set<Transform>(transform)
        .set<RigidBodyDesc>({
            .shape = ShapeKind::Box,
            .kind = bodyKind,
            .halfExtents = physicsHalfExtents,
            .mass = 1.0f,
            .friction = 0.72f,
            .restitution = 0.08f,
            .layer = bodyKind == BodyKind::Static
                ? Lu::Layers::STATIC_ENVIRONMENT
                : Lu::Layers::DYNAMIC_ENVIRONMENT,
            .linearVelocity = initialVelocity
        })
        .add<Lu::Component::TransformGpu>();

    world.entity()
        .child_of(body)
        .set<Lu::Component::Mesh>({.mesh = cubeMesh, .pipeline = meshPipeline})
        .set<Lu::Component::Material>(makeMaterial(color));

    return body;
}

struct PhysicsCubeScene {
    explicit PhysicsCubeScene(flecs::world& world) {
        world.import<Lu::Module::FlyingCamera>();
        world.import<Lu::Module::PhysicsModule>();

        init_random();

        const flecs::entity cubeMesh = world.lookup("Mesh::PhysicsCube");
        const flecs::entity meshPipeline = world.lookup("MeshPipe::PhysicsCubes");

        constexpr glm::vec3 cameraPosition(18.0f, 16.0f, 24.0f);
        world.entity("PhysicsCubeCamera")
            .set(FlyingCamera{
                .position = cameraPosition,
                .angle = Lu::Component::FlyingCamera::lookAt(
                    cameraPosition, glm::vec3(0.0f, 3.0f, 0.0f)),
                .speed = 10.0f,
                .rotationSpeed = 0.005f,
                .fov = glm::radians(60.0f),
                .nearClip = 0.1f,
                .farClip = 250.0f,
                .direction = glm::normalize(glm::vec3(0.0f, 3.0f, 0.0f) - cameraPosition)
            })
            .add<Transform>();

        // Click to launch a small dynamic cube along the camera's current view.
        auto nextShot = std::make_shared<uint32_t>(0);
        world.system<const InputState, const FlyingCamera>("ShootPhysicsCubes")
            .each([cubeMesh, meshPipeline, nextShot](flecs::iter& it, size_t,
                    const InputState& input, const FlyingCamera& camera) {
                if (!input.wasMouseButtonPressedThisFrame(Lu::MouseButton::mouseLeft)) {
                    return;
                }

                const glm::vec3 direction = glm::normalize(camera.direction);
                const glm::vec3 position = camera.position + direction * 1.0f;
                const glm::vec3 velocity = direction * 32.0f;
                const std::string name = "ShotPhysicsCube" + std::to_string((*nextShot)++);
                spawnPhysicsCube(it.world(), name.c_str(), position,
                    glm::vec3(0.7f), glm::vec3(0.35f), BodyKind::Dynamic,
                    glm::vec3(1.0f, 0.42f, 0.12f), cubeMesh, meshPipeline, velocity);
            });

        // Directional key light above the arena.
        Transform lightTransform{};
        lightTransform.position = glm::vec3(0.0f, 45.0f, 0.0f);
        lightTransform.rotation = glm::angleAxis(
            (0.8f - 3.14f) / 2.0f, glm::vec3(1.0f, 0.0f, 0.0f));
        const flecs::entity lightRoot = world.entity("PhysicsCubeSunTransform")
            .set<Transform>(lightTransform)
            .add<Lu::Component::TransformGpu>();
        world.entity("PhysicsCubeSun")
            .child_of(lightRoot)
            .set<Lu::Component::Light>({
                .type = Lu::LightType::Directional,
                .color = glm::vec3(1.0f, 0.94f, 0.82f),
                .intensity = 2.0f,
                .radius = 180.0f
            });

        // One large box collider and one scaled cube mesh make the static floor.
        spawnPhysicsCube(world, "PhysicsCubeFloor",
            glm::vec3(0.0f, -kFloorHalfHeight, 0.0f),
            glm::vec3(kFloorHalfWidth * 2.0f, kFloorHalfHeight * 2.0f,
                      kFloorHalfWidth * 2.0f),
            glm::vec3(kFloorHalfWidth, kFloorHalfHeight, kFloorHalfWidth),
            BodyKind::Static, glm::vec3(0.28f, 0.34f, 0.40f),
            cubeMesh, meshPipeline);

        // A 10 x 10 x 5 stack gives exactly 500 independently simulated cubes.
        // Small gaps let them settle into a compact pile instead of starting
        // with overlapping colliders.
        for (int layer = 0; layer < kCubeLayers; ++layer) {
            for (int z = 0; z < kCubesPerSide; ++z) {
                for (int x = 0; x < kCubesPerSide; ++x) {
                    const int index = layer * kCubesPerSide * kCubesPerSide
                        + z * kCubesPerSide + x;
                    const glm::vec3 position(
                        (x - (kCubesPerSide - 1) * 0.5f) * kCubeSpacing,
                        0.52f + layer * kCubeSpacing,
                        (z - (kCubesPerSide - 1) * 0.5f) * kCubeSpacing);

                    const glm::vec3 color(
                        0.35f + 0.55f * static_cast<float>((x + layer) % 5) / 4.0f,
                        0.30f + 0.55f * static_cast<float>((z + 2 * layer) % 5) / 4.0f,
                        0.42f + 0.48f * static_cast<float>((x + z + 3 * layer) % 5) / 4.0f);
                    const std::string name = "PhysicsCube" + std::to_string(index);
                    spawnPhysicsCube(world, name.c_str(), position,
                        glm::vec3(kCubeHalfExtent * 2.0f),
                        glm::vec3(kCubeHalfExtent), BodyKind::Dynamic,
                        color, cubeMesh, meshPipeline);
                }
            }
        }
    }
};


int main() {
    Lu::App("LuEngine Physics Cubes", true)
        .registerMeshPipe(Lu::GraphicsPipelineConfig{
            .name = "MeshPipe::PhysicsCubes",
            .capacity = 1024,
            .vertexShader = "shader/mesh_basic.vert.spv",
            .fragmentShader = "shader/mesh_pbr.frag.spv"
        })
        .registerShape<Lu::Shape::Cube>(
            "Mesh::PhysicsCube",
            glm::vec3(0.5f),
            glm::vec3(0.0f),
            glm::vec4(1.0f)
        )
        .importModule<PhysicsCubeScene>()
        .run();
}
