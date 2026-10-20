#pragma once
#include "Global.hpp"
#include "flecs.h"
#include <Jolt/Jolt.h>
#include <Jolt/Core/JobSystemThreadPool.h>
#include <Jolt/Core/TempAllocator.h>
#include <Jolt/Physics/PhysicsSystem.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Collision/ContactListener.h>
#include <Jolt/Physics/Character/CharacterVirtual.h>
#include <array>
#include <memory>
#include <mutex>
#include <unordered_map>
#include <vector>

namespace Lca {

namespace Layers {
    static constexpr ::JPH::ObjectLayer STATIC_ENVIRONMENT  = 0;
    static constexpr ::JPH::ObjectLayer DYNAMIC_ENVIRONMENT = 1;
    static constexpr ::JPH::ObjectLayer PLAYER_BODY         = 2;
    static constexpr ::JPH::ObjectLayer ENEMY_BODY          = 3;
    static constexpr ::JPH::ObjectLayer PLAYER_SPELL        = 4;
    static constexpr ::JPH::ObjectLayer ENEMY_SPELL         = 5;
    static constexpr ::JPH::ObjectLayer DESTRUCTIBLE        = 6;
    static constexpr ::JPH::ObjectLayer LOOT_PICKUP         = 7;
    static constexpr ::JPH::ObjectLayer NUM_OBJECT_LAYERS   = 8;
}

namespace BroadPhaseLayers {
    static constexpr ::JPH::BroadPhaseLayer NON_MOVING(0);
    static constexpr ::JPH::BroadPhaseLayer MOVING(1);
    static constexpr ::JPH::uint NUM_BROAD_PHASE_LAYERS = 2;
}

namespace Core {

// -----------------------------------------------------------------------------
// Collision matrix. Every rule is written ONCE and symmetrised at compile time,
// so ShouldCollide(a,b) == ShouldCollide(b,a) always holds (Jolt requires this).
// -----------------------------------------------------------------------------
namespace detail {
constexpr std::array<::JPH::uint32, Layers::NUM_OBJECT_LAYERS> makeCollisionMatrix() {
    using namespace Layers;
    std::array<::JPH::uint32, NUM_OBJECT_LAYERS> m{};
    auto link = [&m](::JPH::ObjectLayer a, ::JPH::ObjectLayer b) {
        m[a] |= (1u << b);
        m[b] |= (1u << a);
    };
    // environment
    link(STATIC_ENVIRONMENT, DYNAMIC_ENVIRONMENT);
    link(STATIC_ENVIRONMENT, PLAYER_BODY);
    link(STATIC_ENVIRONMENT, ENEMY_BODY);
    link(STATIC_ENVIRONMENT, PLAYER_SPELL);
    link(STATIC_ENVIRONMENT, ENEMY_SPELL);
    link(STATIC_ENVIRONMENT, DESTRUCTIBLE);
    link(DYNAMIC_ENVIRONMENT, DYNAMIC_ENVIRONMENT);
    link(DYNAMIC_ENVIRONMENT, PLAYER_BODY);
    link(DYNAMIC_ENVIRONMENT, ENEMY_BODY);
    link(DYNAMIC_ENVIRONMENT, PLAYER_SPELL);
    link(DYNAMIC_ENVIRONMENT, ENEMY_SPELL);
    link(DYNAMIC_ENVIRONMENT, DESTRUCTIBLE);
    // player
    link(PLAYER_BODY, ENEMY_BODY);
    link(PLAYER_BODY, ENEMY_SPELL);
    link(PLAYER_BODY, DESTRUCTIBLE);
    link(PLAYER_BODY, LOOT_PICKUP);
    // enemies (NO enemy<->enemy, NO enemy<->enemy spell)
    link(ENEMY_BODY, PLAYER_SPELL);
    link(ENEMY_BODY, DESTRUCTIBLE);
    // spells
    link(PLAYER_SPELL, DESTRUCTIBLE);
    link(ENEMY_SPELL, DESTRUCTIBLE);
    return m;
}
inline constexpr auto kCollisionMatrix = makeCollisionMatrix();
} // namespace detail

class BPLayerInterfaceImpl final : public ::JPH::BroadPhaseLayerInterface {
public:
    BPLayerInterfaceImpl() {
        for (auto& l : mObjectToBroadPhase) l = BroadPhaseLayers::MOVING;
        mObjectToBroadPhase[Layers::STATIC_ENVIRONMENT] = BroadPhaseLayers::NON_MOVING;
    }
    ::JPH::uint GetNumBroadPhaseLayers() const override { return BroadPhaseLayers::NUM_BROAD_PHASE_LAYERS; }
    ::JPH::BroadPhaseLayer GetBroadPhaseLayer(::JPH::ObjectLayer inLayer) const override {
        JPH_ASSERT(inLayer < Layers::NUM_OBJECT_LAYERS);
        return mObjectToBroadPhase[inLayer];
    }
#if defined(JPH_EXTERNAL_PROFILE) || defined(JPH_PROFILE_ENABLED)
    const char* GetBroadPhaseLayerName(::JPH::BroadPhaseLayer inLayer) const override {
        switch ((::JPH::BroadPhaseLayer::Type)inLayer) {
        case (::JPH::BroadPhaseLayer::Type)BroadPhaseLayers::NON_MOVING: return "NON_MOVING";
        case (::JPH::BroadPhaseLayer::Type)BroadPhaseLayers::MOVING:     return "MOVING";
        default: JPH_ASSERT(false); return "INVALID";
        }
    }
#endif
private:
    ::JPH::BroadPhaseLayer mObjectToBroadPhase[Layers::NUM_OBJECT_LAYERS];
};

class ObjectVsBroadPhaseLayerFilterImpl final : public ::JPH::ObjectVsBroadPhaseLayerFilter {
public:
    bool ShouldCollide(::JPH::ObjectLayer inLayer1, ::JPH::BroadPhaseLayer inLayer2) const override {
        if (inLayer1 == Layers::STATIC_ENVIRONMENT) return inLayer2 == BroadPhaseLayers::MOVING;
        return true;
    }
};

class ObjectLayerPairFilterImpl final : public ::JPH::ObjectLayerPairFilter {
public:
    bool ShouldCollide(::JPH::ObjectLayer a, ::JPH::ObjectLayer b) const override {
        JPH_ASSERT(a < Layers::NUM_OBJECT_LAYERS && b < Layers::NUM_OBJECT_LAYERS);
        return (detail::kCollisionMatrix[a] >> b) & 1u;
    }
};

// -----------------------------------------------------------------------------
// glm <-> Jolt helpers
// -----------------------------------------------------------------------------
inline ::JPH::Vec3 toJoltVec3(const glm::vec3& v) { return ::JPH::Vec3(v.x, v.y, v.z); }
inline glm::vec3 toGlmVec3(::JPH::RVec3Arg v) { return glm::vec3((float)v.GetX(), (float)v.GetY(), (float)v.GetZ()); }
inline ::JPH::Quat toJoltQuat(const glm::quat& q) { return ::JPH::Quat(q.x, q.y, q.z, q.w); }
inline glm::quat toGlmQuat(const ::JPH::Quat& q) { return glm::quat(q.GetW(), q.GetX(), q.GetY(), q.GetZ()); }

// -----------------------------------------------------------------------------
// Raw events produced on Jolt worker threads, consumed on the ECS thread.
// -----------------------------------------------------------------------------
struct CollisionEvent {
    enum class Type : uint8_t { Begin, End, CharacterHit };
    Type type{Type::Begin};
    flecs::entity_t entity1{0};
    flecs::entity_t entity2{0};
    glm::vec3 point{0.f};
    glm::vec3 normal{0.f};   // Jolt: direction to push body2 out of body1 (points from 1 to 2)
    bool sensor{false};
};

class CollisionQueue {
public:
    void push(const CollisionEvent& e) { std::lock_guard<std::mutex> l(mutex); queue.push_back(e); }
    void drain(std::vector<CollisionEvent>& out) {
        out.clear();
        std::lock_guard<std::mutex> l(mutex);
        out.swap(queue);
    }
private:
    std::mutex mutex;
    std::vector<CollisionEvent> queue;
};
inline CollisionQueue collisionQueue;

// BodyID -> entity. Written ONLY on the main thread while the simulation is not
// running, read from Jolt worker threads during Update => no locking needed.
class BodyEntityMap {
public:
    void set(::JPH::BodyID id, flecs::entity_t e) { map[id.GetIndexAndSequenceNumber()] = e; }
    void erase(::JPH::BodyID id) { map.erase(id.GetIndexAndSequenceNumber()); }
    void clear() { map.clear(); }
    flecs::entity_t get(::JPH::BodyID id) const {
        auto it = map.find(id.GetIndexAndSequenceNumber());
        return it == map.end() ? 0 : it->second;
    }
private:
    std::unordered_map<::JPH::uint32, flecs::entity_t> map;
};

class FlecsContactListener final : public ::JPH::ContactListener {
public:
    explicit FlecsContactListener(const BodyEntityMap& m) : map(m) {}

    void OnContactAdded(const ::JPH::Body& b1, const ::JPH::Body& b2,
                        const ::JPH::ContactManifold& m, ::JPH::ContactSettings&) override {
        auto e1 = (flecs::entity_t)b1.GetUserData();
        auto e2 = (flecs::entity_t)b2.GetUserData();
        if (!e1 || !e2) return;
        collisionQueue.push({CollisionEvent::Type::Begin, e1, e2,
                             toGlmVec3(m.GetWorldSpaceContactPointOn2(0)),
                             toGlmVec3(m.mWorldSpaceNormal),
                             b1.IsSensor() || b2.IsSensor()});
    }

    // Jolt gives no Body pointers here (they may already be removed) -> use the map.
    void OnContactRemoved(const ::JPH::SubShapeIDPair& p) override {
        auto e1 = map.get(p.GetBody1ID());
        auto e2 = map.get(p.GetBody2ID());
        if (!e1 || !e2) return;
        collisionQueue.push({CollisionEvent::Type::End, e1, e2, glm::vec3(0.f), glm::vec3(0.f), false});
    }
private:
    const BodyEntityMap& map;
};

// CharacterVirtual contacts are reported as CharacterContact records.
class FlecsCharacterContactListener final : public ::JPH::CharacterContactListener {
public:
    explicit FlecsCharacterContactListener(const BodyEntityMap& m) : map(m) {}

    void OnContactAdded(const ::JPH::CharacterVirtual* character,
                        const ::JPH::CharacterContact& contact,
                        ::JPH::CharacterContactSettings&) override {
        const auto e1 = (flecs::entity_t)character->GetUserData();
        const auto e2 = map.get(contact.mBodyB);
        if (!e1 || !e2) return;
        collisionQueue.push({CollisionEvent::Type::CharacterHit, e1, e2,
                             toGlmVec3(contact.mPosition), toGlmVec3(contact.mContactNormal),
                             contact.mIsSensorB});
    }
private:
    const BodyEntityMap& map;
};

// -----------------------------------------------------------------------------
// Physics manager
// -----------------------------------------------------------------------------
class Physics {
public:
    Physics() = default;
    ~Physics() { shutdown(); }
    Physics(const Physics&) = delete;
    Physics& operator=(const Physics&) = delete;

    void init();
    void shutdown();
    bool isInitialized() const { return physicsSystem != nullptr; }

    ::JPH::PhysicsSystem& getPhysicsSystem() { return *physicsSystem; }
    ::JPH::JobSystemThreadPool& getJobSystem() { return *jobSystemThreadPool; }
    ::JPH::TempAllocator* getTempAllocator() { return tempAllocator.get(); }
    ::JPH::BodyInterface& getBodyInterface() { return physicsSystem->GetBodyInterface(); }
    const BodyEntityMap& getBodyMap() const { return bodyMap; }
    // usage: characterVirtual->SetListener(Core::GetPhysics().getCharacterListener());
    ::JPH::CharacterContactListener* getCharacterListener() { return characterContactListener.get(); }

    // Creates the body WITHOUT adding it to the world (it keeps all its state
    // while it is outside the world). Returns an invalid ID on failure.
    ::JPH::BodyID createBody(const ::JPH::BodyCreationSettings& settings, flecs::entity_t owner);
    void addBody(::JPH::BodyID id, bool activate = true);   // no-op if already in world
    void removeBody(::JPH::BodyID id);                      // no-op if not in world, body is NOT destroyed
    void destroyBody(::JPH::BodyID id);                     // remove + destroy + unmap
    bool isInWorld(::JPH::BodyID id) const;
    void optimizeBroadPhase();                              // call after adding many static bodies

    // Fixed timestep with accumulator. Returns number of simulated sub-steps.
    int step(float deltaTime);

    float fixedTimeStep{1.0f / 60.0f};
    int   maxSubSteps{4};

private:
    unsigned int maxBodies{65536};
    unsigned int maxBodyPairs{65536};
    unsigned int maxContactConstraints{10240};
    unsigned int numBodyMutexes{0};
    unsigned int tempAllocatorSize{64u * 1024u * 1024u};
    float accumulator{0.f};

    BodyEntityMap bodyMap;
    std::unique_ptr<BPLayerInterfaceImpl> broadPhaseLayerInterface;
    std::unique_ptr<ObjectVsBroadPhaseLayerFilterImpl> objectVsBroadPhaseLayerFilter;
    std::unique_ptr<ObjectLayerPairFilterImpl> objectLayerPairFilter;
    std::unique_ptr<::JPH::PhysicsSystem> physicsSystem;
    std::unique_ptr<::JPH::JobSystemThreadPool> jobSystemThreadPool;
    std::unique_ptr<::JPH::TempAllocator> tempAllocator;
    std::unique_ptr<FlecsContactListener> contactListener;
    std::unique_ptr<FlecsCharacterContactListener> characterContactListener;
};

inline Physics& GetPhysics() {
    static Physics instance;
    return instance;
}

} // namespace Core
} // namespace Lca
