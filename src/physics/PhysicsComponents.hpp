#pragma once
#include "Global.hpp"
#include "flecs.h"
#include "Physics.hpp"
#include <cstdint>
#include <unordered_map>
#include <vector>

// Physics objects use Lu::Component::Transform for their world position and
// rotation. RigidBodyDesc supplies the physics shape dimensions independently
// of Transform::scale.
namespace Lca::Component {

// ---- body description (what YOU set) ---------------------------------------
enum class ShapeKind : uint8_t { Box, Sphere, Capsule };
enum class BodyKind  : uint8_t { Static, Kinematic, Dynamic };

struct RigidBodyDesc {
    ShapeKind shape{ShapeKind::Box};
    BodyKind  kind{BodyKind::Dynamic};
    glm::vec3 halfExtents{0.5f};   // Box: xyz | Sphere: x = radius | Capsule: x = radius, y = half cylinder height
    float mass{1.f};
    float friction{0.5f};
    float restitution{0.f};
    bool  isSensor{false};         // sensors produce (Overlapping, other) instead of (Touching, other)
    ::JPH::ObjectLayer layer{Layers::DESTRUCTIBLE};
    glm::vec3 linearVelocity{0.f};
};

// ---- runtime handle (what the MODULE sets) ----------------------------------
struct PhysicsBody { ::JPH::BodyID id; };

// ---- lifecycle tags ---------------------------------------------------------
// Active   => body is in the Jolt world.
// Inactive => body still exists (state kept!) but is removed from the Jolt world.
// Adding one automatically removes the other.
struct PhysicsActive {};
struct PhysicsInactive {};

struct PhysicsFocus {};      // e.g. the player / camera: terrain near it gets activated
struct PhysicsDynamic {};    // added automatically from RigidBodyDesc.kind
struct PhysicsKinematic {};

// ---- terrain ----------------------------------------------------------------
struct TerrainChunk {
    glm::ivec2 coord{0};
    glm::vec3  origin{0.f};        // world position of sample (0,0) (a corner of the chunk)
    uint32_t   sampleCount{0};     // samples per axis, heights.size() == sampleCount^2
    float      cellSize{1.f};
    float      heightScale{1.f};
    std::vector<float> heights;    // kept in RAM so the chunk can be re-created / inspected
    float worldSize() const { return (float)(sampleCount - 1) * cellSize; }
};

// Command component: "take this body out of the physics world (without destroying it)".
// Set by the chunk deactivation (AABB query) or by you. Consumed by an observer.
struct Inactivate { flecs::entity_t chunk{0}; };

// Bodies frozen by chunk streaming (global, because a body on a chunk edge may only be
// thawed when ALL chunks it overlaps are active, no matter which chunk froze it).
struct FrozenRegistry { std::vector<flecs::entity_t> entities; };
inline FrozenRegistry& GetFrozenRegistry() { static FrozenRegistry r; return r; }

// ---- contacts (relations, persistent while touching) ------------------------
//   (Touching,    other)  : solid contact
//   (Overlapping, other)  : sensor overlap
//   (ContactInfo, other)  : data, normal points from THIS entity towards `other`
struct Touching {};
struct Overlapping {};
struct ContactInfo {
    glm::vec3 point{0.f};
    glm::vec3 normal{0.f};
    int  count{0};      // number of sub-shape contacts (refcount)
    bool sensor{false};
};

// CharacterVirtual hits: only valid for ONE frame, pair (CharacterHitInfo, other)
struct CharacterHitInfo { glm::vec3 point{0.f}; glm::vec3 normal{0.f}; };

// ---- global config / index (main thread only) -------------------------------
struct PhysicsStreamingConfig {
    float chunkWorldSize{128.f};     // set automatically by spawnTerrainChunk
    float activateRadius{192.f};     // chunk (distance to its XZ rectangle) < this => activate
    float deactivateRadius{256.f};   // chunk further than this => deactivate (hysteresis!)
    float updateInterval{0.1f};      // seconds
    float freezeAboveTerrain{500.f}; // AABB of a deactivated chunk: height above the highest sample
    float freezeBelowTerrain{100.f}; // ... and depth below the lowest sample
};
inline PhysicsStreamingConfig& GetStreamingConfig() { static PhysicsStreamingConfig c; return c; }

struct TerrainIndex {
    std::unordered_map<int64_t, flecs::entity_t> chunks;
    static int64_t key(glm::ivec2 c) { return ((int64_t)c.x << 32) ^ (int64_t)(uint32_t)c.y; }
};
inline TerrainIndex& GetTerrainIndex() { static TerrainIndex i; return i; }

} // namespace Lca::Component
