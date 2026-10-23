// Click-to-command adventurers viewed through a free flying camera.
// Left click an adventurer to select it, then click the ground to send it there.
// WASD control the camera; hold right mouse and move to look around.
#include "App.hpp"
#include "Shape.hpp"
#include "FlyingCamera.hpp"
#include "PhysicsModule.hpp"
#include "CharacterCapsule.hpp"
#include "component/AnimationStateMachine.hpp"
#include "component/Light.hpp"
#include "component/Material.hpp"
#include "component/Mesh.hpp"
#include "component/Skeleton.hpp"
#include "component/Transform.hpp"
#include "component/TransformGpu.hpp"

#include <cmath>
#include <string>
#include <vector>

using Lu::Component::Transform;
using Lu::Component::MeshTransform;
using Lu::Component::InputState;
using Lu::Component::FlyingCamera;
using Lu::Component::AnimationStateMachine;
using Lca::Component::CharacterCapsule;
using Lca::Component::PhysicsRay;
using Lca::Component::PhysicsRayCast;
using Lca::Component::PhysicsRayHit;
using Lca::Component::RigidBodyDesc;
using Lca::Component::ShapeKind;
using Lca::Component::BodyKind;

namespace {
constexpr const char* kModel = "Adventurer14";
constexpr const char* kModelFile = "asset\\adventurer\\Adventurer.gltf";
constexpr const char* kUpperBodyRoot = "Torso";
constexpr float kMeshScale = 0.6f;
constexpr float kFloorHalfExtent = 40.0f;

struct Adventurer {};
struct DestinationGhost {};
struct MoveOrder {
    glm::vec3 destination{0.0f};
    bool active{false};
};
struct CommandState {
    flecs::entity selected;
    bool hasDestination{false};
    glm::vec3 destination{0.0f};
    flecs::entity ghost;
    flecs::entity selectionMarker;
    Lu::Core::ParamId speedParam{};
};

template <typename T> const T& componentRef(const T* value) { return *value; }
template <typename T> const T& componentRef(const T& value) { return value; }

Lu::Component::Material makeMaterial(const glm::vec3& color) {
    Lu::Component::Material m{};
    m.albedo = glm::vec4(color, 1.0f);
    m.ambient = glm::vec4(color * 0.08f, 1.0f);
    m.roughness = 0.72f;
    return m;
}

flecs::entity addCube(const flecs::world& world, const std::string& name,
    const glm::vec3& position, const glm::vec3& scale, const glm::vec3& color,
    const flecs::entity& mesh, const flecs::entity& pipeline,
    const glm::vec3& halfExtents, BodyKind kind) {
    Transform t{};
    t.position = position;
    t.scale = scale;
    auto body = world.entity(name.c_str())
        .set<Transform>(t)
        .set<RigidBodyDesc>({.shape = ShapeKind::Box, .kind = kind,
            .halfExtents = halfExtents, .mass = 1.0f, .friction = 0.8f,
            .layer = Lca::Layers::STATIC_ENVIRONMENT})
        .add<Lu::Component::TransformGpu>();
    world.entity().set(flecs::Parent{body})
        .set<Lu::Component::Mesh>({.mesh = mesh, .pipeline = pipeline})
        .set<Lu::Component::Material>(makeMaterial(color));
    return body;
}

struct AdventurerCommandDemo {
    explicit AdventurerCommandDemo(flecs::world& world) {
        world.import<Lu::Module::FlyingCamera>();
        world.import<Lca::Module::PhysicsModule>();
        world.import<Lca::Module::CharacterCapsuleModule>();

        auto& library = Lu::Core::GetAnimationLibrary();
        library.createMaskFromNode("Adventurer14::UpperBody", kModel, kUpperBodyRoot);
        Lu::AnimationGraphBuilder("Adventurer14Graph", kModel)
            .clip("Idle", "Idle")
            .clip("Run", "Run")
            .blend1D("Locomotion", "speed", {{"Idle", 0.0f}, {"Run", 1.0f}})
            .start("Locomotion")
            .build();
        const auto probe = AnimationStateMachine::create("Adventurer14Graph");
        const auto speedParam = probe.findParam("speed");

        // Instances share the loaded skeleton prefab and locomotion graph.
        const auto prefab = world.lookup((std::string("Skeleton::") + kModel).c_str());
        uint32_t index = 0;
        for (int z = 0; z < 3; ++z) {
            for (int x = 0; x < 4; ++x) {
                const glm::vec3 position((x - 1.5f) * 2.0f, 0.0f, (z - 1.0f) * 2.0f);
                world.entity(("Adventurer" + std::to_string(index++)).c_str())
                    .is_a(prefab)
                    .add<Adventurer>()
                    .set<Transform>({position, 0.0f, glm::vec3(0, 1, 0), glm::vec3(1.0f)})
                    .set<MeshTransform>(MeshTransform(glm::vec3(0.0f), glm::radians(180.0f),
                        glm::vec3(0, 1, 0), glm::vec3(kMeshScale)))
                    .set<AnimationStateMachine>(AnimationStateMachine::create("Adventurer14Graph"))
                    .set<CharacterCapsule>({.radius = 0.35f, .halfHeight = 0.55f,
                        .maxRunSpeed = 4.5f,
                        .faceMoveDirection = true})
                    .set<MoveOrder>({})
                    .add<Lca::Component::PhysicsFocus>();
            }
        }

        // Camera starts above and in front of the group. It is not attached to a character.
        const glm::vec3 cameraPosition(0.0f, 12.0f, 22.0f);
        world.entity("AdventurerCommandCamera")
            .set<FlyingCamera>({.position = cameraPosition,
                .angle = FlyingCamera::lookAt(cameraPosition, glm::vec3(0.0f, 0.0f, 0.0f)),
                .speed = 12.0f, .rotationSpeed = 0.005f,
                .fov = glm::radians(60.0f), .nearClip = 0.1f, .farClip = 300.0f,
                .direction = glm::normalize(glm::vec3(0.0f, 0.0f, 0.0f) - cameraPosition)})
            .add<Transform>();

        const auto aim = world.entity("AdventurerCommandAim")
            .set<Transform>({})
            .set<PhysicsRay>({})
            .set<PhysicsRayHit>({})
            .add<PhysicsRayCast>();
        auto adventurerQuery = world.query_builder<Adventurer, const Transform>().build();

        const auto cubeMesh = world.lookup("Mesh::AdventurerEnvironment");
        const auto solidPipeline = world.lookup("MeshPipe::AdventurerEnvironment");
        const auto ghostPipeline = world.lookup("MeshPipe::AdventurerGhost");
        auto ghostMaterial = makeMaterial(glm::vec3(0.25f, 0.85f, 0.55f));
        ghostMaterial.albedo.a = 0.4f;
        ghostMaterial.emission = glm::vec4(0.02f, 0.12f, 0.05f, 0.0f);
        Transform ghostTransform{};
        ghostTransform.scale = glm::vec3(0.0f);
        const auto ghostRoot = world.entity("AdventurerDestinationGhost")
            .set<Transform>(ghostTransform).add<Lu::Component::TransformGpu>();
        world.entity().set(flecs::Parent{ghostRoot})
            .set<Lu::Component::Mesh>({.mesh = cubeMesh, .pipeline = ghostPipeline})
            .set<Lu::Component::Material>(ghostMaterial);
        auto selectionMaterial = makeMaterial(glm::vec3(0.2f, 0.65f, 1.0f));
        selectionMaterial.albedo.a = 0.48f;
        selectionMaterial.emission = glm::vec4(0.01f, 0.07f, 0.16f, 0.0f);
        Transform selectionTransform{};
        selectionTransform.scale = glm::vec3(0.0f);
        const auto selectionMarker = world.entity("SelectedAdventurerMarker")
            .set<Transform>(selectionTransform).add<Lu::Component::TransformGpu>();
        world.entity().set(flecs::Parent{selectionMarker})
            .set<Lu::Component::Mesh>({.mesh = cubeMesh, .pipeline = ghostPipeline})
            .set<Lu::Component::Material>(selectionMaterial);

        // Broad, solid floor and scattered environment blocks.
        addCube(world, "AdventurerGround", glm::vec3(0.0f, -0.5f, 0.0f),
            glm::vec3(80.0f, 1.0f, 80.0f), glm::vec3(0.25f, 0.32f, 0.3f),
            cubeMesh, solidPipeline, glm::vec3(kFloorHalfExtent, 0.5f, kFloorHalfExtent), BodyKind::Static);
        const glm::vec3 blocks[] = {
            {-10, 1, -7}, {-7, 0.75f, -10}, {8, 1.5f, -8}, {12, 0.75f, -3},
            {-13, 0.75f, 5}, {-9, 2.0f, 10}, {6, 0.75f, 8}, {11, 1.5f, 12},
            {0, 1.0f, -14}, {2, 0.5f, 14}
        };
        for (uint32_t i = 0; i < sizeof(blocks) / sizeof(blocks[0]); ++i) {
            const float h = blocks[i].y;
            const glm::vec3 half(0.7f + 0.15f * (i % 3), h, 0.7f + 0.1f * (i % 2));
            addCube(world, "EnvironmentBlock" + std::to_string(i), blocks[i], half * 2.0f,
                glm::vec3(0.35f + 0.05f * (i % 4), 0.42f + 0.04f * (i % 3), 0.5f),
                cubeMesh, solidPipeline, half, BodyKind::Static);
        }

        Transform sunTransform{};
        sunTransform.position = glm::vec3(0.0f, 35.0f, 0.0f);
        sunTransform.rotation = glm::angleAxis((0.8f - 3.14f) / 2.0f, glm::vec3(1, 0, 0));
        const auto sun = world.entity("AdventurerCommandSun").set<Transform>(sunTransform)
            .add<Lu::Component::TransformGpu>();
        world.entity().set(flecs::Parent{sun}).set<Lu::Component::Light>({
            .type = Lu::LightType::Directional, .color = glm::vec3(1.0f, 0.95f, 0.84f),
            .intensity = 2.0f, .radius = 160.0f});

        world.component<CommandState>().add(flecs::Singleton);
        world.set<CommandState>({.selected = {}, .hasDestination = false,
            .destination = glm::vec3(0.0f), .ghost = ghostRoot,
            .selectionMarker = selectionMarker, .speedParam = speedParam});

        // Build a world-space mouse ray before the physics module runs its post-step cast.
        world.system<PhysicsRay, const InputState>("BuildAdventurerAim")
            .with<PhysicsRayCast>().kind(flecs::PreUpdate)
            .each([camera = world.lookup("AdventurerCommandCamera")](PhysicsRay& ray, const InputState& input) {
                const float width = static_cast<float>(Lu::Core::windowWidth);
                const float height = static_cast<float>(Lu::Core::windowHeight);
                if (width < 1.0f || height < 1.0f) return;
                const auto& cameraState = componentRef(camera.get<FlyingCamera>());
                const glm::vec3 forward = glm::normalize(cameraState.direction);
                const glm::vec3 right = glm::normalize(glm::cross(forward, glm::vec3(0, 1, 0)));
                const glm::vec3 up = glm::cross(right, forward);
                const float x = 2.0f * input.cursor.position.x / width - 1.0f;
                const float y = 1.0f - 2.0f * input.cursor.position.y / height;
                const float tangent = std::tan(cameraState.fov * 0.5f);
                ray.origin = cameraState.position;
                ray.direction = glm::normalize(forward + right * (x * width / height * tangent) + up * (y * tangent));
                ray.maxDistance = cameraState.farClip;
            });

        // Select an adventurer on the first click; subsequent ground clicks set its destination.
        world.system<CommandState, const InputState>("SelectAndCommandAdventurers")
            .kind(flecs::OnUpdate)
            .each([aim, adventurerQuery](flecs::iter& it, size_t, CommandState& state, const InputState& input) {
                if (!input.wasMouseButtonPressedThisFrame(Lu::MouseButton::mouseLeft)) return;
                const auto& hit = componentRef(aim.get<PhysicsRayHit>());
                if (!hit.hit) return;
                // CharacterVirtual capsules are not ordinary rigid bodies, so test
                // the ray against their visual/capsule bounds as well as physics hits.
                flecs::entity clicked{};
                float nearest = hit.distance;
                const auto& ray = componentRef(aim.get<PhysicsRay>());
                adventurerQuery.each([&](flecs::entity candidate,
                    Adventurer, const Transform& transform) {
                    const glm::vec3 toCenter = transform.position + glm::vec3(0.0f, 0.9f, 0.0f) - ray.origin;
                    const float along = glm::dot(toCenter, ray.direction);
                    if (along < 0.0f || along > nearest) return;
                    const glm::vec3 closest = ray.origin + ray.direction * along;
                    if (glm::length(closest - (transform.position + glm::vec3(0.0f, 0.9f, 0.0f))) < 0.65f) {
                        nearest = along;
                        clicked = candidate;
                    }
                });
                if (clicked.is_alive()) {
                    state.selected = clicked;
                    const auto& order = componentRef(clicked.get<MoveOrder>());
                    state.hasDestination = order.active;
                    if (order.active) state.destination = order.destination;
                    return;
                }
                if (state.selected.is_alive()) {
                    state.destination = glm::vec3(hit.point.x, 0.0f, hit.point.z);
                    state.hasDestination = true;
                    state.selected.set<MoveOrder>({.destination = state.destination, .active = true});
                }
            });

        // Every adventurer keeps executing its own move order, whether selected or not.
        flecs::entity prePhysics = world.lookup("PrePhysics");
        world.system<CharacterCapsule, AnimationStateMachine, const Transform, MoveOrder>("MoveAdventurerToCommand")
            .with<Adventurer>().kind(prePhysics)
            .each([speedParam](CharacterCapsule& capsule, AnimationStateMachine& anim,
                const Transform& transform, MoveOrder& order) {
                const float run = capsule.maxRunSpeed;
                if (!order.active) {
                    capsule.stop();
                } else {
                    const glm::vec3 delta(order.destination.x - transform.position.x, 0.0f,
                        order.destination.z - transform.position.z);
                    const float distance = glm::length(delta);
                    if (distance < 0.18f) {
                        capsule.stop();
                        order.active = false;
                    } else {
                        // Commands use run speed; the blend parameter follows the
                        // character's actual speed from Idle directly into Run.
                        capsule.setMove(delta / distance, true);
                        capsule.yaw = std::atan2(delta.x, delta.z);
                    }
                }
                anim.setParam(speedParam, glm::clamp(capsule.moveSpeed / (run * 0.8f), 0.0f, 1.0f));
            });

        world.system<CommandState>("UpdateAdventurerDestinationGhost")
            .kind(flecs::OnUpdate)
            .each([](CommandState& state) {
                auto ghost = state.ghost;
                auto& t = ghost.get_mut<Transform>();
                state.hasDestination = false;
                if (state.selected.is_alive()) {
                    const auto& selectedOrder = componentRef(state.selected.get<MoveOrder>());
                    state.hasDestination = selectedOrder.active;
                    if (state.hasDestination) state.destination = selectedOrder.destination;
                }
                if (!state.hasDestination) {
                    t.scale = glm::vec3(0.0f);
                } else {
                    t.position = state.destination + glm::vec3(0.0f, 0.035f, 0.0f);
                    t.scale = glm::vec3(0.9f, 0.07f, 0.9f);
                }

                auto marker = state.selectionMarker;
                auto& markerTransform = marker.get_mut<Transform>();
                if (!state.selected.is_alive()) {
                    markerTransform.scale = glm::vec3(0.0f);
                } else {
                    const auto& adventurerTransform = componentRef(state.selected.get<Transform>());
                    markerTransform.position = adventurerTransform.position + glm::vec3(0.0f, 0.035f, 0.0f);
                    markerTransform.scale = glm::vec3(1.25f, 0.07f, 1.25f);
                }
            });
    }
};
}

int main() {
    Lu::App("LuEngine Adventurer Commands", true)
        .registerMeshPipe(Lu::GraphicsPipelineConfig{
            .name = "MeshPipe::AdventurerEnvironment", .capacity = 64,
            .vertexShader = "shader/mesh_basic.vert.spv", 
            .fragmentShader = "shader/mesh_pbr.frag.spv"})
        .registerMeshPipe(Lu::GraphicsPipelineConfig{
            .name = "MeshPipe::AdventurerGhost", .capacity = 4,
            .vertexShader = "shader/mesh_basic.vert.spv", 
            .fragmentShader = "shader/mesh_basic.frag.spv",
            .depthWriteEnable = false, 
            .blendEnable = true})
        .registerSkeletonPipe(Lu::GraphicsPipelineConfig{
            .name = "SkeletonPipe::Adventurer14", .capacity = 4096,
            .vertexShader = "shader/skeleton_basic.vert.spv", 
            .fragmentShader = "shader/skeleton_pbr.frag.spv"})
        .registerShape<Lu::Shape::Cube>(
            "Mesh::AdventurerEnvironment", 
            glm::vec3(0.5f),
            glm::vec3(0.0f), 
            glm::vec4(1.0f))
        .loadSkeletonModel(
            kModel, 
            kModelFile, 
            "Skeleton::", 
            "SkeletonPipe::Adventurer14", 
            30.0f)
        .importModule<AdventurerCommandDemo>()
        .run();
}
