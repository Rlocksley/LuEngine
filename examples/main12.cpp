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
#include <string>

using Lu::Component::InputState;
using Lu::Component::Transform;
using Lu::Component::FlyingCamera;
using Lu::Component::BodyKind;
using Lu::Component::PhysicsRay;
using Lu::Component::PhysicsRayCast;
using Lu::Component::PhysicsRayHit;
using Lu::Component::RigidBodyDesc;
using Lu::Component::ShapeKind;

namespace {
constexpr float kCubeHalfExtent = 0.45f;
constexpr float kCubeSize = kCubeHalfExtent * 2.0f;
constexpr float kFloorHalfWidth = 18.0f;
constexpr float kFloorHalfHeight = 0.5f;
struct GhostTag {};
struct CubeSpawner {
    flecs::entity mesh;
    flecs::entity pipeline;
    uint32_t nextCube{0};
};

template <typename T> const T& componentRef(const T* value) { return *value; }
template <typename T> const T& componentRef(const T& value) { return value; }

Lu::Component::Material cubeMaterial(const glm::vec3& color) {
    Lu::Component::Material material{};
    material.albedo = glm::vec4(color, 1.0f);
    material.ambient = glm::vec4(color * 0.08f, 1.0f);
    material.roughness = 0.28f;
    material.metallic = 0.18f;
    return material;
}

flecs::entity spawnCube(const flecs::world& world, const std::string& name,
    const glm::vec3& position, const glm::vec3& color,
    const flecs::entity& cubeMesh, const flecs::entity& pipeline,
    BodyKind kind = BodyKind::Dynamic,
    const glm::vec3& visualScale = glm::vec3(kCubeSize),
    const glm::vec3& halfExtents = glm::vec3(kCubeHalfExtent)) {
    Transform transform{};
    transform.position = position;
    transform.scale = visualScale;
    const auto body = world.entity(name.c_str())
        .set<Transform>(transform)
        .set<RigidBodyDesc>({
            .shape = ShapeKind::Box,
            .kind = kind,
            .halfExtents = halfExtents,
            .mass = 1.0f,
            .friction = 0.72f,
            .restitution = 0.08f,
            .layer = kind == BodyKind::Static
                ? Lu::Layers::STATIC_ENVIRONMENT : Lu::Layers::DYNAMIC_ENVIRONMENT
        })
        .add<Lu::Component::TransformGpu>();
    world.entity().set(flecs::Parent{body})
        .set<Lu::Component::Mesh>({.mesh = cubeMesh, .pipeline = pipeline})
        .set<Lu::Component::Material>(cubeMaterial(color));
    return body;
}

flecs::entity addVisualCube(const flecs::world& world, const glm::vec3& position,
    const glm::vec3& scale, const Lu::Component::Material& material,
    const flecs::entity& cubeMesh, const flecs::entity& pipeline) {
    Transform transform{};
    transform.position = position;
    transform.scale = scale;
    const auto root = world.entity().set<Transform>(transform).add<Lu::Component::TransformGpu>();
    world.entity().set(flecs::Parent{root})
        .set<Lu::Component::Mesh>({.mesh = cubeMesh, .pipeline = pipeline})
        .set<Lu::Component::Material>(material);
    return root;
}

struct PhysicsCubeSculpture {
    explicit PhysicsCubeSculpture(flecs::world& world) {
        world.import<Lu::Module::FlyingCamera>();
        world.import<Lu::Module::PhysicsModule>();

        const auto cubeMesh = world.lookup("Mesh::SculptureCube");
        const auto solidPipeline = world.lookup("MeshPipe::SculptureCubes");
        const auto ghostPipeline = world.lookup("MeshPipe::SculptureGhost");

        constexpr glm::vec3 cameraPosition(18.0f, 16.0f, 24.0f);
        const auto cameraEntity = world.entity("SculptureCamera")
            .set(FlyingCamera{
                .position = cameraPosition,
                .angle = FlyingCamera::lookAt(cameraPosition, glm::vec3(0.0f, 2.0f, 0.0f)),
                .speed = 10.0f,
                .rotationSpeed = 0.005f,
                .fov = glm::radians(60.0f),
                .nearClip = 0.1f,
                .farClip = 250.0f,
                .direction = glm::normalize(glm::vec3(0.0f, 2.0f, 0.0f) - cameraPosition)
            }).add<Transform>();

        // One persistent ECS ray request drives both the preview and spawning.
        Transform rayTransform{};
        const auto aim = world.entity("SculptureAim")
            .set<Transform>(rayTransform)
            .set<PhysicsRay>({})
            .set<PhysicsRayHit>({})
            .add<PhysicsRayCast>();
        (void)aim;
        world.component<GhostTag>();
        world.component<CubeSpawner>().add(flecs::Singleton);
        world.set<CubeSpawner>({cubeMesh, solidPipeline, 0});

        // A permanent translucent preview follows the cursor ray, even before
        // the first cube is spawned. It has no collider of its own.
        Lu::Component::Material ghostMaterial{};
        ghostMaterial.albedo = glm::vec4(0.22f, 0.75f, 1.0f, 0.42f);
        ghostMaterial.ambient = glm::vec4(0.18f, 0.32f, 0.4f, 1.0f);
        ghostMaterial.emission = glm::vec4(0.02f, 0.09f, 0.14f, 0.0f);
        ghostMaterial.roughness = 0.45f;
        addVisualCube(world, glm::vec3(0.0f), glm::vec3(0.0f), ghostMaterial,
            cubeMesh, ghostPipeline).add<GhostTag>();

        // Large static floor plus a small starting stack to create an active
        // physics sculpture playground.
        spawnCube(world, "SculptureFloor", glm::vec3(0.0f, -kFloorHalfHeight, 0.0f),
            glm::vec3(0.25f, 0.31f, 0.38f), cubeMesh, solidPipeline, BodyKind::Static,
            glm::vec3(kFloorHalfWidth * 2.0f, kFloorHalfHeight * 2.0f, kFloorHalfWidth * 2.0f),
            glm::vec3(kFloorHalfWidth, kFloorHalfHeight, kFloorHalfWidth));
        uint32_t starter = 0;
        for (int layer = 0; layer < 3; ++layer) {
            for (int z = 0; z < 4; ++z) {
                for (int x = 0; x < 4; ++x) {
                    const glm::vec3 color(
                        0.35f + 0.5f * static_cast<float>(x) / 3.0f,
                        0.35f + 0.5f * static_cast<float>(layer) / 2.0f,
                        0.4f + 0.45f * static_cast<float>(z) / 3.0f);
                    spawnCube(world, "StarterCube" + std::to_string(starter++),
                        glm::vec3((x - 1.5f) * 1.02f,
                            kFloorHalfHeight + kCubeHalfExtent + layer * 0.92f,
                            (z - 1.5f) * 1.02f), color, cubeMesh, solidPipeline);
                }
            }
        }

        Transform lightTransform{};
        lightTransform.position = glm::vec3(0.0f, 45.0f, 0.0f);
        lightTransform.rotation = glm::angleAxis((0.8f - 3.14f) / 2.0f, glm::vec3(1.0f, 0.0f, 0.0f));
        const auto lightRoot = world.entity("SculptureSunTransform")
            .set<Transform>(lightTransform).add<Lu::Component::TransformGpu>();
        world.entity("SculptureSun").set(flecs::Parent{lightRoot}).set<Lu::Component::Light>({
            .type = Lu::LightType::Directional,
            .color = glm::vec3(1.0f, 0.94f, 0.82f),
            .intensity = 2.0f,
            .radius = 180.0f
        });

        // Construct a world ray through the current mouse pixel. This runs
        // before physics so PhysicsModule can publish its result post-step.
        world.system<PhysicsRay, const InputState>("BuildSculptureAim")
            .with<PhysicsRayCast>().kind(flecs::PreUpdate)
            .each([cameraEntity](PhysicsRay& ray, const InputState& input) {
                const float width = static_cast<float>(Lu::Core::windowWidth);
                const float height = static_cast<float>(Lu::Core::windowHeight);
                if (width < 1.0f || height < 1.0f) return;
                const FlyingCamera& camera = componentRef(cameraEntity.get<FlyingCamera>());
                const glm::vec3 forward = glm::normalize(camera.direction);
                const glm::vec3 right = glm::normalize(glm::cross(forward, glm::vec3(0.0f, 1.0f, 0.0f)));
                const glm::vec3 up = glm::cross(right, forward);
                const float ndcX = 2.0f * input.cursor.position.x / width - 1.0f;
                const float ndcY = 1.0f - 2.0f * input.cursor.position.y / height;
                const float tangent = std::tan(camera.fov * 0.5f);
                ray.origin = camera.position;
                ray.direction = glm::normalize(forward + right * (ndcX * (width / height) * tangent)
                    + up * (ndcY * tangent));
                ray.maxDistance = camera.farClip;
            });

        world.system<Transform>("UpdateSculptureGhost")
            .with<GhostTag>()
            .each([aim](Transform& transform) {
                const PhysicsRayHit& hit = componentRef(aim.get<PhysicsRayHit>());
                if (!hit.hit) {
                    transform.scale = glm::vec3(0.0f);
                    return;
                }
                transform.position = hit.point + hit.normal * (kCubeHalfExtent + 0.015f);
                transform.scale = glm::vec3(kCubeSize * 0.98f);
            });

        world.system<CubeSpawner, const InputState>("SpawnPhysicsSculptureCube")
            .each([aim](flecs::iter& it, size_t, CubeSpawner& spawner,
                const InputState& input) {
                const PhysicsRayHit& hit = componentRef(aim.get<PhysicsRayHit>());
                if (!hit.hit || !input.wasMouseButtonPressedThisFrame(Lu::MouseButton::mouseLeft)) return;
                const glm::vec3 position = hit.point + hit.normal * (kCubeHalfExtent + 0.025f);
                const float hue = static_cast<float>(spawner.nextCube++ % 7) / 7.0f;
                const glm::vec3 color(0.95f - hue * 0.25f, 0.38f + hue * 0.45f, 0.18f + hue * 0.55f);
                spawnCube(it.world(), "SculptureCube" + std::to_string(spawner.nextCube),
                    position, color, spawner.mesh, spawner.pipeline);
            });
    }
};
}

int main() {
    Lu::App("LuEngine Physics Cube Sculpture", true)
        .registerMeshPipe(Lu::GraphicsPipelineConfig{
            .name = "MeshPipe::SculptureCubes",
            .capacity = 2048,
            .vertexShader = "shader/mesh_basic.vert.spv",
            .fragmentShader = "shader/mesh_pbr.frag.spv"
        })
        .registerMeshPipe(Lu::GraphicsPipelineConfig{
            .name = "MeshPipe::SculptureGhost",
            .capacity = 4,
            .vertexShader = "shader/mesh_basic.vert.spv",
            .fragmentShader = "shader/mesh_basic.frag.spv",
            .depthWriteEnable = false,
            .blendEnable = true
        })
        .registerShape<Lu::Shape::Cube>("Mesh::SculptureCube", glm::vec3(0.5f),
            glm::vec3(0.0f), glm::vec4(1.0f))
        .importModule<PhysicsCubeSculpture>()
        .run();
}
