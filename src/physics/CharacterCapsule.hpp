#pragma once

#include "Global.hpp"
#include "flecs.h"
#include "PhysicsComponents.hpp"
#include "Physics.hpp"
#include "../core/component/Transform.hpp"
#include "../core/Time.hpp"

#include <Jolt/Physics/Collision/Shape/CapsuleShape.h>
#include <Jolt/Physics/Collision/Shape/RotatedTranslatedShape.h>
#include <Jolt/Physics/Character/CharacterVirtual.h>

#include <algorithm>
#include <cmath>

// ============================================================================
//  CharacterCapsule: the ECS representation of a Jolt CharacterVirtual
//
//  You write (every frame, from your own controller system):   INPUT
//      capsule.setMove(worldDirection, run);   capsule.turn(radians);   capsule.jump();
//  The module writes (PhysicsStep phase, after PhysicsUpdate):  STATE + Transform
//      moveSpeed, velocity, onGround, isRunning, yaw       and  Transform.position/rotation
//
//  Transform.position is the position of the FEET, so a skeleton prefab parented to
//  the entity needs no offset. Forward is +Z rotated by yaw around +Y.
//
//      world.import<Lu::Module::PhysicsModule>();
//      world.import<Lu::Module::CharacterCapsuleModule>();
//
//      auto player = world.entity("Player")
//          .is_a(world.lookup("Skeleton::Hero"))
//          .set<Lu::Component::Transform>({glm::vec3(0, 2, 0), 0.f, glm::vec3(0,1,0), glm::vec3(1)})
//          .set<Lu::Component::CharacterCapsule>({.radius = 0.35f, .halfHeight = 0.55f})
//          .add<Lu::Component::PhysicsFocus>();
//
//      // custom controller: any flecs system
//      world.system<Lu::Component::CharacterCapsule, Lu::Component::AnimationStateMachine>()
//          .with<Player>().kind(PrePhysics)...
// ============================================================================

namespace Lu::Component {

struct CharacterCapsule {
    // ---- configuration (read when the character is created) -----------------------------
    float radius{0.35f};
    float halfHeight{0.55f};          // half height of the cylinder part; total height = 2*(halfHeight+radius)
    float maxSlopeDegrees{50.0f};
    float mass{70.0f};
    float maxStrength{100.0f};        // max force the character applies to bodies it pushes
    ::JPH::ObjectLayer layer{Layers::PLAYER_BODY};

    // ---- tuning (may be changed at any time) ----------------------------------------------
    float maxWalkSpeed{2.0f};
    float maxRunSpeed{5.5f};
    float acceleration{14.0f};
    float deceleration{18.0f};
    float jumpSpeed{6.0f};
    float airControl{0.3f};           // 0..1 fraction of acceleration while airborne
    float turnSpeed{3.0f};            // rad/s, used by faceMoveDirection
    bool  faceMoveDirection{false};   // rotate towards the move direction automatically
    float stepUpHeight{0.4f};
    float stickToFloorDistance{0.5f};

    // ---- input ---------------------------------------------------------------------------------
    glm::vec3 moveDirection{0.0f};    // world space, y ignored; zero = stand still
    bool wantsRun{false};
    bool wantsJump{false};
    float yaw{0.0f};                  // facing, radians around +Y

    void setMove(const glm::vec3& worldDirection, bool run = false) {
        moveDirection = glm::vec3(worldDirection.x, 0.0f, worldDirection.z);
        wantsRun = run;
    }
    void stop() { moveDirection = glm::vec3(0.0f); wantsRun = false; }
    void turn(float radians) { yaw += radians; }
    void jump() { wantsJump = true; }
    glm::vec3 forward() const { return glm::vec3(std::sin(yaw), 0.0f, std::cos(yaw)); }
    // walk/run forward along the facing direction
    void moveForward(bool run = false) { setMove(forward(), run); }
    void teleport(const glm::vec3& feetPosition) { teleportTarget = feetPosition; hasTeleport = true; }

    // ---- state (written by the module) -------------------------------------------------------
    float moveSpeed{0.0f};            // horizontal speed
    bool isRunning{false};
    bool onGround{false};
    glm::vec3 velocity{0.0f};

    // ---- runtime ----------------------------------------------------------------------------------
    ::JPH::Ref<::JPH::CharacterVirtual> character;
    float verticalSpeed{0.0f};
    glm::vec3 teleportTarget{0.0f};
    bool hasTeleport{false};
};

} // namespace Lu::Component

namespace Lu::Module {

struct CharacterCapsuleModule {
    CharacterCapsuleModule(flecs::world& world) {
        using namespace Component;
        world.component<CharacterCapsule>();

        // ---- create the Jolt character when Transform + CharacterCapsule are present ------
        world.observer<CharacterCapsule, const Lu::Component::Transform>("CreateCharacterCapsules")
            .event(flecs::OnSet)
            .each([](flecs::entity e, CharacterCapsule& c, const Lu::Component::Transform& transform) {
                if (c.character) return;
                auto& phys = Core::GetPhysics();
                if (!phys.isInitialized()) return;

                // the capsule is lifted by its half extent so the character origin is at the feet
                const float lift = c.halfHeight + c.radius;
                ::JPH::RefConst<::JPH::Shape> capsule = new ::JPH::CapsuleShape(c.halfHeight, c.radius);
                ::JPH::RefConst<::JPH::Shape> shape = ::JPH::RotatedTranslatedShapeSettings(
                    ::JPH::Vec3(0.0f, lift, 0.0f), ::JPH::Quat::sIdentity(), capsule).Create().Get();

                ::JPH::CharacterVirtualSettings settings;
                settings.mShape = shape;
                settings.mUp = ::JPH::Vec3::sAxisY();
                settings.mMaxSlopeAngle = ::JPH::DegreesToRadians(c.maxSlopeDegrees);
                settings.mMass = c.mass;
                settings.mMaxStrength = c.maxStrength;
                settings.mSupportingVolume = ::JPH::Plane(::JPH::Vec3::sAxisY(), -c.radius);

                c.character = new ::JPH::CharacterVirtual(
                    &settings,
                    ::JPH::RVec3(transform.position.x, transform.position.y, transform.position.z),
                    Core::toJoltQuat(glm::angleAxis(c.yaw, glm::vec3(0.0f, 1.0f, 0.0f))),
                    (::JPH::uint64)e.id(),
                    &phys.getPhysicsSystem());
                c.character->SetListener(phys.getCharacterListener());
            });

        // ---- simulate after the rigid body step ---------------------------------------------------------
        flecs::entity physicsStep = world.lookup("PhysicsStep");
        LU_ASSERT(physicsStep, "CharacterCapsuleModule", "CharacterCapsuleModule",
            "Import Lu::Module::PhysicsModule before CharacterCapsuleModule.")

        world.system<CharacterCapsule, Lu::Component::Transform>("CharacterCapsuleUpdate")
            .kind(physicsStep)
            .each([](flecs::iter& it, size_t, CharacterCapsule& c, Lu::Component::Transform& transform) {
                if (!c.character) return;
                auto& phys = Core::GetPhysics();
                auto& system = phys.getPhysicsSystem();
                const float dt = std::min(it.delta_time(), 0.1f);
                if (dt <= 0.0f) return;

                if (c.hasTeleport) {
                    c.character->SetPosition(::JPH::RVec3(c.teleportTarget.x, c.teleportTarget.y, c.teleportTarget.z));
                    c.character->SetLinearVelocity(::JPH::Vec3::sZero());
                    c.verticalSpeed = 0.0f;
                    c.hasTeleport = false;
                }

                // ---- facing --------------------------------------------------------------------------------
                const float inputLength = glm::length(c.moveDirection);
                const bool hasInput = inputLength > 1e-4f;
                glm::vec3 dir = hasInput ? c.moveDirection / inputLength : glm::vec3(0.0f);
                if (c.faceMoveDirection && hasInput) {
                    const float target = std::atan2(dir.x, dir.z);
                    float delta = std::remainder(target - c.yaw, 6.2831853f);
                    const float maxStep = c.turnSpeed * dt;
                    c.yaw += std::clamp(delta, -maxStep, maxStep);
                }

                // ---- horizontal speed: accelerate towards the gait speed, decelerate to zero --------
                const bool onGround = c.character->GetGroundState() == ::JPH::CharacterBase::EGroundState::OnGround;
                c.isRunning = hasInput && c.wantsRun;
                const float targetSpeed = hasInput ? (c.isRunning ? c.maxRunSpeed : c.maxWalkSpeed) * std::min(inputLength, 1.0f) : 0.0f;
                const float rate = (targetSpeed > c.moveSpeed ? c.acceleration : c.deceleration) * (onGround ? 1.0f : c.airControl);
                c.moveSpeed = c.moveSpeed < targetSpeed
                    ? std::min(c.moveSpeed + rate * dt, targetSpeed)
                    : std::max(c.moveSpeed - rate * dt, targetSpeed);

                // keep the last direction while slowing down
                glm::vec3 horizontal = hasInput ? dir : c.forward();
                if (!hasInput && c.moveSpeed < 1e-3f) horizontal = glm::vec3(0.0f);

                // ---- vertical -------------------------------------------------------------------------------------
                const ::JPH::Vec3 gravity = system.GetGravity();
                if (onGround) {
                    c.verticalSpeed = std::max(0.0f, c.character->GetGroundVelocity().GetY());
                    if (c.wantsJump) c.verticalSpeed = c.jumpSpeed;
                } else {
                    c.verticalSpeed += gravity.GetY() * dt;
                }
                c.wantsJump = false;

                const ::JPH::Vec3 velocity(horizontal.x * c.moveSpeed, c.verticalSpeed, horizontal.z * c.moveSpeed);
                c.character->SetLinearVelocity(velocity);
                c.character->SetRotation(Core::toJoltQuat(glm::angleAxis(c.yaw, glm::vec3(0.0f, 1.0f, 0.0f))));

                ::JPH::CharacterVirtual::ExtendedUpdateSettings update;
                update.mStickToFloorStepDown = ::JPH::Vec3(0.0f, -c.stickToFloorDistance, 0.0f);
                update.mWalkStairsStepUp = ::JPH::Vec3(0.0f, c.stepUpHeight, 0.0f);

                c.character->ExtendedUpdate(
                    dt, gravity, update,
                    system.GetDefaultBroadPhaseLayerFilter(c.layer),
                    system.GetDefaultLayerFilter(c.layer),
                    {}, {},
                    *phys.getTempAllocator());

                // ---- Jolt -> ECS -----------------------------------------------------------------------------------
                c.onGround = c.character->GetGroundState() == ::JPH::CharacterBase::EGroundState::OnGround;
                const ::JPH::Vec3 actual = c.character->GetLinearVelocity();
                c.velocity = glm::vec3(actual.GetX(), actual.GetY(), actual.GetZ());
                c.verticalSpeed = actual.GetY();
                transform.position = Core::toGlmVec3(c.character->GetPosition());
                transform.rotation = glm::angleAxis(c.yaw, glm::vec3(0.0f, 1.0f, 0.0f));
            });
    }
};

} // namespace Lu::Module
