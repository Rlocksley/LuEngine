// Animated character on a Jolt CharacterVirtual, with a custom controller written as a plain flecs system.
//
//   WASD  : turn (A/D) / walk (W)      Shift : run      Space : jump      Left mouse : cast Spell1
//   Hold right mouse and move : orbit camera      Scroll : zoom
//
// Needs a skinned model with Idle / Walk / Run / Spell1 animations:
//   asset/wizard/Wizard.glb    (adjust the model, animation names and upper-body mask below)
#include "App.hpp"
#include "Shape.hpp"
#include "CharacterCamera.hpp"
#include "PhysicsModule.hpp"
#include "CharacterCapsule.hpp"
#include "component/AnimationStateMachine.hpp"
#include "component/Light.hpp"
#include "component/Material.hpp"
#include "component/Mesh.hpp"
#include "component/Skeleton.hpp"
#include "component/Transform.hpp"
#include "component/TransformGpu.hpp"

using Lu::Component::Transform;
using Lu::Component::InputState;
using Lu::Component::AnimationStateMachine;
using Lca::Component::CharacterCapsule;

constexpr const char* kModel = "Wizard";
constexpr const char* kModelFile = "asset\\king\\King.gltf";
constexpr const char* kUpperBodyRoot = "Torso";

struct Hero {};

struct CharacterDemo {
    explicit CharacterDemo(flecs::world& world) {
        world.import<Lu::Module::CharacterCamera>();
        world.import<Lca::Module::PhysicsModule>();
        world.import<Lca::Module::CharacterCapsuleModule>();

        // ---- animation graph: shared by every instance ------------------------------------------------
        auto& library = Lu::Core::GetAnimationLibrary();
        library.createMaskFromNode("Hero::UpperBody", kModel, kUpperBodyRoot);

        Lu::AnimationGraphBuilder("HeroGraph", kModel)
            .clip("Idle", "Idle")
            .clip("Walk", "Walk")
            .clip("Run",  "Run")
            .blend1D("Locomotion", "speed", {{"Idle", 0.0f}, {"Walk", 0.5f}, {"Run", 1.0f}})
            .clip("Wave", "Wave", /*looping*/ false)
            .layer("Interaction",/*top*/ "Wave", /*base*/ "Locomotion", "Hero::UpperBody",
                   /*blendIn*/ 0.1f, /*blendOut*/ 0.15f, /*onFinish*/ "Locomotion")
            .start("Locomotion")
            .build();

        // ---- the hero: prefab (skeleton + meshes) + capsule + state machine ---------------------------
        world.entity("Hero")
            .is_a(world.lookup((std::string("Skeleton::") + kModel).c_str()))
            .add<Hero>()
            .set<Transform>({glm::vec3(0.0f, 0.0f, 0.0f), 0.0f, glm::vec3(0, 1, 0), glm::vec3(0.6f)}) // GLB is ~3 m tall; capsule is 1.8 m
            .set<AnimationStateMachine>(AnimationStateMachine::create("HeroGraph"))
            .set<CharacterCapsule>({.radius = 0.35f, .halfHeight = 0.55f})
            .add<Lca::Component::PhysicsFocus>();

        // ---- custom character controller ------------------------------------------------------------------
        // Plain system: read input, steer the capsule, feed the animation. Cache ids once.
        const auto probe = AnimationStateMachine::create("HeroGraph");
        const Lu::Core::ParamId speedParam = probe.findParam("speed");
        const Lu::Core::StateId spellState = probe.findState("Interaction");

        flecs::entity prePhysics = world.lookup("PrePhysics");
        world.system<CharacterCapsule, AnimationStateMachine, const InputState>("HeroController")
            .with<Hero>()
            .kind(prePhysics)
            .each([=](CharacterCapsule& capsule, AnimationStateMachine& anim, const InputState& input) {
                using Lu::KeyCode;
                const float dt = 1.0f / 60.0f;
                if (input.isKeyDown(KeyCode::keyA)) capsule.turn(2.5f * dt);
                if (input.isKeyDown(KeyCode::keyD)) capsule.turn(-2.5f * dt);

                if (input.isKeyDown(KeyCode::keyW)) capsule.moveForward(input.isKeyDown(KeyCode::keyShift));
                else capsule.stop();

                if (input.wasKeyPressedThisFrame(KeyCode::keySpace)) capsule.jump();

                // gait -> blend space parameter (0 idle, 0.5 walk, 1 run)
                const float walk = capsule.maxWalkSpeed, run = capsule.maxRunSpeed;
                const float s = capsule.moveSpeed;
                anim.setParam(speedParam, s <= walk ? 0.5f * s / walk : 0.5f + 0.5f * (s - walk) / (run - walk));

                if (input.wasMouseButtonPressedThisFrame(Lu::MouseButton::mouseLeft) &&
                    anim.activeState() != spellState) {
                    anim.setState(spellState);
                }
            });

        // ---- environment -------------------------------------------------------------------------------------------
        const flecs::entity cube = world.lookup("Mesh::Floor");
        const flecs::entity pipe = world.lookup("MeshPipe::Floor");
        Lu::Component::Material floorMaterial{};
        floorMaterial.albedo = glm::vec4(0.3f, 0.35f, 0.4f, 1.0f);
        floorMaterial.ambient = glm::vec4(0.03f, 0.03f, 0.03f, 1.0f);
        floorMaterial.roughness = 0.8f;

        Transform floorTransform{};
        floorTransform.position = glm::vec3(0.0f, -0.5f, 0.0f);
        floorTransform.scale = glm::vec3(60.0f, 1.0f, 60.0f);
        const flecs::entity floor = world.entity("Floor")
            .set<Transform>(floorTransform)
            .set<Lca::Component::RigidBodyDesc>({
                .shape = Lca::Component::ShapeKind::Box,
                .kind = Lca::Component::BodyKind::Static,
                .halfExtents = glm::vec3(30.0f, 0.5f, 30.0f),
                .layer = Lca::Layers::STATIC_ENVIRONMENT})
            .add<Lu::Component::TransformGpu>();
        world.entity().child_of(floor)
            .set<Lu::Component::Mesh>({.mesh = cube, .pipeline = pipe})
            .set<Lu::Component::Material>(floorMaterial);

        Transform sunTransform{};
        sunTransform.position = glm::vec3(0.0f, 30.0f, 0.0f);
        sunTransform.rotation = glm::angleAxis((0.8f - 3.14f) / 2.0f, glm::vec3(1.0f, 0.0f, 0.0f));
        const flecs::entity sun = world.entity("Sun").set<Transform>(sunTransform).add<Lu::Component::TransformGpu>();
        world.entity().child_of(sun).set<Lu::Component::Light>({
            .type = Lu::LightType::Directional, .color = glm::vec3(1.0f, 0.95f, 0.85f),
            .intensity = 2.0f, .radius = 120.0f});

        world.entity("Camera")
            .set<Lu::Component::CharacterCamera>({
                .target = world.lookup("Hero"),
                .yaw = 0.0f,
                .pitch = 0.15f,
                .distance = 7.0f,
                .farClip = 250.0f})
            .set<Transform>({});
    }
};

int main() {
    Lu::App("LuEngine Character", false)
        .threads(4)    // animation graphs are evaluated in parallel
        .registerMeshPipe(Lu::GraphicsPipelineConfig{
            .name = "MeshPipe::Floor", .capacity = 16,
            .vertexShader = "shader/mesh_basic.vert.spv", .fragmentShader = "shader/mesh_pbr.frag.spv"})
        .registerSkeletonPipe(Lu::GraphicsPipelineConfig{
            .name = "SkeletonPipe::Simple", .capacity = 4096,
            .vertexShader = "shader/skeleton_basic.vert.spv", .fragmentShader = "shader/skeleton_pbr.frag.spv"})
        .registerShape<Lu::Shape::Cube>("Mesh::Floor", glm::vec3(0.5f), glm::vec3(0.0f), glm::vec4(1.0f))
        .loadSkeletonModel(kModel, kModelFile, "Skeleton::", "SkeletonPipe::Simple", 30.0f)
        .importModule<CharacterDemo>()
        .run();
}
