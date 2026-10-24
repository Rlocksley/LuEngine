#pragma once

#include "Global.hpp"
#include "flecs.h"
#include "PhysicsComponents.hpp"
#include "Physics.hpp"
#include "CharacterCapsule.hpp"
#include "../core/component/Transform.hpp"

#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Collision/Shape/SphereShape.h>
#include <Jolt/Physics/Collision/Shape/CapsuleShape.h>
#include <Jolt/Physics/Collision/Shape/HeightFieldShape.h>
#include <Jolt/Physics/Collision/BroadPhase/BroadPhaseQuery.h>
#include <Jolt/Physics/Collision/RayCast.h>
#include <Jolt/Physics/Collision/CastResult.h>
#include <Jolt/Physics/Body/BodyLock.h>
#include <Jolt/Geometry/AABox.h>

#include <algorithm>
#include <cfloat>
#include <map>
#include <memory>
#include <vector>

/*
 * ============================ USAGE ==========================================
 *   world.import<Lca::Module::PhysicsModule>();
 *
 *   // player: the terrain around it is streamed into the physics world
 *   player.set<Lu::Component::Transform>({.position={...}}).add<PhysicsFocus>();
 *
 *   // crate: no extra tag. When the chunk below it is deactivated, an AABB query
 *   // finds it (inside OR intersecting), it gets `Inactivate` and is taken out of the
 *   // Jolt world (state kept). It is put back once every chunk it overlaps is active.
 *   world.entity().set<Lu::Component::Transform>({.position={0,10,0}})
 *        .set<RigidBodyDesc>({ShapeKind::Box, BodyKind::Dynamic, {0.5f,0.5f,0.5f}});
 *
 *   // loot sensor
 *   world.entity().set<Lu::Component::Transform>(...)
 *        .set<RigidBodyDesc>({.shape=ShapeKind::Sphere, .kind=BodyKind::Kinematic,
 *                             .halfExtents={1,0,0}, .isSensor=true, .layer=Layers::LOOT_PICKUP})
 *        .add<Loot>();
 *
 *   // terrain chunk (created inactive; the module activates it near a PhysicsFocus)
 *   Lca::Module::spawnTerrainChunk(world, {cx, cz}, heights, 65, 1.0f, 1.0f);
 *
 *   // --- reacting to contacts, 100% ECS side ---
 *   world.system<>("PlayerPickups").with<Player>().each([](flecs::entity player) {
 *       player.each<Component::Overlapping>([&](flecs::entity other) {
 *           if (other.has<Loot>()) { ...collect...; other.destruct(); }
 *       });
 *   });
 *   // "enter / exit" events: observe the relation itself
 *   world.observer<>("OnTouchBegin").event(flecs::OnAdd)
 *        .with<Component::Touching>(flecs::Wildcard)
 *        .each([](flecs::iter& it, size_t i) {
 *            flecs::entity self  = it.entity(i);
 *            flecs::entity other = it.pair(0).second();   // flecs 3.x: it.pair(1)
 *            const auto& ci = self.get<Component::ContactInfo>(other); // flecs 3.x: pointer
 *        });
 *   // use flecs::OnRemove for "end".
 * =============================================================================
 */

namespace Lca::Module {

namespace PhysicsDetail {

using namespace Lca::Component;

// flecs 3.x get<T>() returns T*, flecs 4.x returns const T& -> normalise
template <typename T> const T* asPtr(const T* p) { return p; }
template <typename T> const T* asPtr(const T& r) { return &r; }

inline void activate(flecs::entity e)   { e.remove<PhysicsInactive>(); e.add<PhysicsActive>(); }
inline void deactivate(flecs::entity e) { e.remove<PhysicsActive>();   e.add<PhysicsInactive>(); }

struct BodyIdCollector final : ::JPH::CollideShapeBodyCollector {
    std::vector<::JPH::BodyID> ids;
    void AddHit(const ::JPH::BodyID& id) override { ids.push_back(id); }
};

// Chunk is being DEACTIVATED: AABB query around it. Every non-static body that is inside
// OR merely intersects the chunk's XZ rectangle gets `Inactivate` (resolved BodyID -> entity
// through the body's user data = flecs::entity_t). Bodies on the edge are frozen too, so they
// cannot slide off the edge into the missing terrain.
inline void freezeBodiesOfChunk(flecs::entity chunk, const TerrainChunk& c) {
    auto& phys = Core::GetPhysics();
    if (!phys.isInitialized() || c.heights.empty()) return;
    const auto& cfg = GetStreamingConfig();

    const auto [mn, mx] = std::minmax_element(c.heights.begin(), c.heights.end());
    const float y0 = std::min(*mn * c.heightScale, *mx * c.heightScale);
    const float y1 = std::max(*mn * c.heightScale, *mx * c.heightScale);
    const float s  = c.worldSize();
    const float eps = 1e-3f;   // bodies that only touch the border line are not "intersecting"
    const glm::vec3 lo(c.origin.x + eps,     c.origin.y + y0 - cfg.freezeBelowTerrain, c.origin.z + eps);
    const glm::vec3 hi(c.origin.x + s - eps, c.origin.y + y1 + cfg.freezeAboveTerrain, c.origin.z + s - eps);

    BodyIdCollector hits;   // broadphase = bounding boxes that overlap OR lie inside the AABB
    phys.getPhysicsSystem().GetBroadPhaseQuery().CollideAABox(
        ::JPH::AABox(Core::toJoltVec3(lo), Core::toJoltVec3(hi)), hits);

    auto& bi = phys.getBodyInterface();
    flecs::world world = chunk.world();
    auto& frozen = GetFrozenRegistry().entities;

    world.defer_begin();
    for (const ::JPH::BodyID& id : hits.ids) {
        if (bi.GetMotionType(id) == ::JPH::EMotionType::Static) continue;   // other chunks, walls...
        flecs::entity collEntity(world, (flecs::entity_t)bi.GetUserData(id));
        if (!collEntity.is_alive() || collEntity == chunk || collEntity.has<PhysicsFocus>()) continue;
        collEntity.set<Inactivate>({chunk.id()});
        frozen.push_back(collEntity.id());
    }
    world.defer_end();
}

// Chunk is being ACTIVATED: frozen bodies are not in the broadphase any more, so we look at
// the frozen registry instead. A frozen body overlapping this chunk is thawed only if its
// whole XZ footprint is now covered by ACTIVE chunks:
//   - body completely inside this chunk      -> thawed now
//   - body on an edge (overlaps a neighbour) -> thawed as soon as the neighbour is active too
//   - neighbour missing / still inactive     -> stays frozen (no falling off the edge)
inline void thawBodiesAround(flecs::entity chunk, const TerrainChunk& c) {
    auto& frozen = GetFrozenRegistry().entities;
    auto& phys = Core::GetPhysics();
    if (frozen.empty() || !phys.isInitialized()) return;

    auto& bi = phys.getBodyInterface();
    const auto& index = GetTerrainIndex().chunks;
    const float cs  = GetStreamingConfig().chunkWorldSize;
    const float s   = c.worldSize();
    const float eps = 1e-3f;
    flecs::world world = chunk.world();

    world.defer_begin();
    frozen.erase(std::remove_if(frozen.begin(), frozen.end(), [&](flecs::entity_t id) -> bool {
        flecs::entity e(world, id);
        if (!e.is_alive() || !e.has<PhysicsBody>() || !e.has<PhysicsInactive>()) return true;  // stale entry

        const ::JPH::BodyID bid = asPtr(e.get<PhysicsBody>())->id;
        const ::JPH::AABox b = bi.GetTransformedShape(bid).GetWorldSpaceBounds();   // works outside the world
        const float minX = b.mMin.GetX(), maxX = b.mMax.GetX();
        const float minZ = b.mMin.GetZ(), maxZ = b.mMax.GetZ();

        // not related to this chunk -> keep waiting
        if (maxX < c.origin.x || minX > c.origin.x + s || maxZ < c.origin.z || minZ > c.origin.z + s) return false;

        // every chunk cell the body overlaps must exist and be active
        const int x0 = (int)std::floor(minX / cs), z0 = (int)std::floor(minZ / cs);
        const int x1 = std::max(x0, (int)std::floor((maxX - eps) / cs));
        const int z1 = std::max(z0, (int)std::floor((maxZ - eps) / cs));
        for (int x = x0; x <= x1; ++x)
            for (int z = z0; z <= z1; ++z) {
                auto it = index.find(Component::TerrainIndex::key(glm::ivec2(x, z)));
                if (it == index.end()) return false;
                flecs::entity ch(world, it->second);
                if (!ch.is_alive() || !ch.has<PhysicsActive>()) return false;
            }
        activate(e);
        return true;   // thawed -> remove from registry
    }), frozen.end());
    world.defer_end();
}

inline ::JPH::BodyID makeRigidBody(flecs::entity e, const RigidBodyDesc& d,
                                   const glm::vec3& pos, const glm::quat& rot) {
    ::JPH::ShapeRefC shape;
    switch (d.shape) {
    case ShapeKind::Box:     shape = new ::JPH::BoxShape(Core::toJoltVec3(d.halfExtents)); break;
    case ShapeKind::Sphere:  shape = new ::JPH::SphereShape(d.halfExtents.x); break;
    case ShapeKind::Capsule: shape = new ::JPH::CapsuleShape(d.halfExtents.y, d.halfExtents.x); break;
    }
    ::JPH::EMotionType motion = d.kind == BodyKind::Static    ? ::JPH::EMotionType::Static
                              : d.kind == BodyKind::Kinematic ? ::JPH::EMotionType::Kinematic
                                                              : ::JPH::EMotionType::Dynamic;
    ::JPH::BodyCreationSettings s(shape.GetPtr(), ::JPH::RVec3(pos.x, pos.y, pos.z),
                                  Core::toJoltQuat(rot), motion, d.layer);
    s.mFriction = d.friction;
    s.mRestitution = d.restitution;
    s.mIsSensor = d.isSensor;
    s.mLinearVelocity = Core::toJoltVec3(d.linearVelocity);
    if (d.kind == BodyKind::Dynamic) {
        s.mOverrideMassProperties = ::JPH::EOverrideMassProperties::CalculateInertia;
        s.mMassPropertiesOverride.mMass = d.mass;
    }
    return Core::GetPhysics().createBody(s, e.id());
}

inline ::JPH::BodyID makeHeightfieldBody(flecs::entity e, const TerrainChunk& c) {
    ::JPH::HeightFieldShapeSettings hf(c.heights.data(), ::JPH::Vec3::sZero(),
                                       ::JPH::Vec3(c.cellSize, c.heightScale, c.cellSize), c.sampleCount);
    auto result = hf.Create();
    if (result.HasError()) return ::JPH::BodyID();   // log result.GetError() if you like
    ::JPH::BodyCreationSettings s(result.Get(), ::JPH::RVec3(c.origin.x, c.origin.y, c.origin.z),
                                  ::JPH::Quat::sIdentity(), ::JPH::EMotionType::Static,
                                  Layers::STATIC_ENVIRONMENT);
    return Core::GetPhysics().createBody(s, e.id());
}

// Drain the Jolt event queue and turn it into ECS relations.
// Events of one frame are aggregated per entity pair first, because structural
// changes are deferred (several sub-shape contacts of the same pair in one frame).
inline void dispatchContacts(flecs::world world) {
    std::vector<Core::CollisionEvent> events;
    Core::collisionQueue.drain(events);
    if (events.empty()) return;

    struct Agg { int delta{0}; bool hasPoint{false}; bool sensor{false}; glm::vec3 point{0.f}, normal{0.f}; };
    std::map<std::pair<flecs::entity_t, flecs::entity_t>, Agg> aggs;

    for (const auto& ev : events) {
        using T = Core::CollisionEvent::Type;
        if (ev.type == T::CharacterHit) {
            flecs::entity c(world, ev.entity1), o(world, ev.entity2);
            if (c.is_alive() && o.is_alive()) c.set<CharacterHitInfo>(o, {ev.point, ev.normal});
            continue;
        }
        auto a = ev.entity1, b = ev.entity2;
        glm::vec3 n = ev.normal;
        if (a > b) { std::swap(a, b); n = -n; }   // canonical order, normal points a -> b
        Agg& g = aggs[{a, b}];
        if (ev.type == T::Begin) {
            g.delta++; g.hasPoint = true; g.point = ev.point; g.normal = n; g.sensor = ev.sensor;
        } else {
            g.delta--;
        }
    }

    for (auto& [key, g] : aggs) {
        flecs::entity a(world, key.first), b(world, key.second);
        if (!a.is_alive() || !b.is_alive()) continue;

        const bool had = a.has<ContactInfo>(b);
        ContactInfo prev{};
        if (had) prev = *asPtr(a.get<ContactInfo>(b));
        const int now = (had ? prev.count : 0) + g.delta;

        if (now > 0) {
            ContactInfo ia = prev;
            if (g.hasPoint) { ia.point = g.point; ia.normal = g.normal; ia.sensor = g.sensor; }
            ia.count = now;
            ContactInfo ib = ia; ib.normal = -ia.normal;
            a.set<ContactInfo>(b, ia);
            b.set<ContactInfo>(a, ib);
            if (!had) {   // contact BEGIN
                if (ia.sensor) { a.add<Overlapping>(b); b.add<Overlapping>(a); }
                else           { a.add<Touching>(b);    b.add<Touching>(a); }
            }
        } else if (had) { // contact END (relation removed first, so OnRemove observers can still read ContactInfo)
            if (prev.sensor) { a.remove<Overlapping>(b); b.remove<Overlapping>(a); }
            else             { a.remove<Touching>(b);    b.remove<Touching>(a); }
            a.remove<ContactInfo>(b);
            b.remove<ContactInfo>(a);
        }
        // had == false && now <= 0: begin+end inside one frame -> ignored
    }
}

} // namespace PhysicsDetail

// -----------------------------------------------------------------------------
// Create a heightfield chunk entity. Starts INACTIVE (body exists, but is not in
// the Jolt world). `heights` = sampleCount^2 floats, row-major. Neighbouring
// chunks must be spaced (sampleCount-1)*cellSize apart.
// -----------------------------------------------------------------------------
inline flecs::entity spawnTerrainChunk(flecs::world& world, glm::ivec2 coord, std::vector<float> heights,
                                       uint32_t sampleCount, float cellSize, float heightScale = 1.f,
                                       float baseY = 0.f, bool startActive = false) {
    using namespace Component;
    TerrainChunk c;
    c.coord = coord; c.sampleCount = sampleCount; c.cellSize = cellSize; c.heightScale = heightScale;
    c.heights = std::move(heights);
    const float size = c.worldSize();
    GetStreamingConfig().chunkWorldSize = size;
    c.origin = glm::vec3(coord.x * size, baseY, coord.y * size);

    auto& index = GetTerrainIndex();
    auto old = index.chunks.find(TerrainIndex::key(coord));
    if (old != index.chunks.end()) flecs::entity(world, old->second).destruct();   // replace

    flecs::entity e = world.entity();
    ::JPH::BodyID id = PhysicsDetail::makeHeightfieldBody(e, c);
    if (id.IsInvalid()) { e.destruct(); return flecs::entity(); }

    index.chunks[TerrainIndex::key(coord)] = e.id();
    e.set<TerrainChunk>(std::move(c));
    e.set<PhysicsBody>({id});
    if (startActive) e.add<PhysicsActive>(); else e.add<PhysicsInactive>();
    return e;
}

// -----------------------------------------------------------------------------
// Module
// -----------------------------------------------------------------------------
struct PhysicsModule {
    PhysicsModule(flecs::world& world) {
        using namespace Component;
        Core::GetPhysics().init();

        // NOTE: never capture the `world` constructor parameter by reference in the
        // lambdas below (it dangles after the constructor). Use it.world()/e.world().

        // ---- phases: PreUpdate -> PrePhysics -> PhysicsStep -> PostPhysics -> OnUpdate
        auto prePhysics  = world.entity("PrePhysics").add(flecs::Phase).depends_on(flecs::PreUpdate);
        auto physicsStep = world.entity("PhysicsStep").add(flecs::Phase).depends_on(prePhysics);
        auto postPhysics = world.entity("PostPhysics").add(flecs::Phase).depends_on(physicsStep);
        // Module entities are scoped under PhysicsModule. Publish aliases so
        // other modules can look up these phases from the world root.
        world.use(prePhysics, "PrePhysics");
        world.use(physicsStep, "PhysicsStep");
        world.use(postPhysics, "PostPhysics");
        world.entity(flecs::OnUpdate).depends_on(postPhysics);

        auto characterCapsules = world.query<CharacterCapsule>();

        // ---- resource cleanup via hooks (also fires on entity destruct) ------
        world.component<PhysicsBody>().on_remove([](flecs::entity, PhysicsBody& b) {
            Core::GetPhysics().destroyBody(b.id);
        });
        world.component<TerrainChunk>().on_remove([](flecs::entity e, TerrainChunk& c) {
            auto& idx = GetTerrainIndex().chunks;
            auto it = idx.find(TerrainIndex::key(c.coord));
            if (it != idx.end() && it->second == e.id()) idx.erase(it);
        });

        // ---- Active / Inactive: body in / out of the Jolt world ---------------
        world.observer<PhysicsBody>("PhysicsBodyActivate")
            .event(flecs::OnAdd).with<PhysicsActive>()
            .each([](flecs::entity e, PhysicsBody& b) {
                e.remove<PhysicsInactive>();
                Core::GetPhysics().addBody(b.id, true);
            });

        world.observer<PhysicsBody>("PhysicsBodyDeactivate")
            .event(flecs::OnAdd).with<PhysicsInactive>()
            .each([](flecs::entity e, PhysicsBody& b) {
                e.remove<PhysicsActive>();
                Core::GetPhysics().removeBody(b.id);   // NOT destroyed, state is kept
            });

        // ---- Inactivate: take the body out of the Jolt world, do NOT destroy it ---
        world.observer<PhysicsBody>("PhysicsBodyInactivate")
            .event(flecs::OnAdd).with<Inactivate>()
            .each([](flecs::entity e, PhysicsBody& b) {
                Core::GetPhysics().removeBody(b.id);
                e.remove<Inactivate>();                // command consumed
                e.remove<PhysicsActive>();
                e.add<PhysicsInactive>();              // later re-activation: add PhysicsActive
            });

        // ---- chunk (de)activation: freeze / thaw the bodies above it (AABB query) ----
        world.observer<const TerrainChunk>("ChunkActivated")
            .event(flecs::OnAdd).with<PhysicsActive>()
            .each([](flecs::entity chunk, const TerrainChunk& c) { PhysicsDetail::thawBodiesAround(chunk, c); });

        world.observer<const TerrainChunk>("ChunkDeactivated")
            .event(flecs::OnAdd).with<PhysicsInactive>()
            .each([](flecs::entity chunk, const TerrainChunk& c) { PhysicsDetail::freezeBodiesOfChunk(chunk, c); });

        // ---- RigidBodyDesc + Transform => Jolt body ---------------------------
        world.observer<const RigidBodyDesc, const Lu::Component::Transform>("CreateRigidBodies")
            .event(flecs::OnSet).without<PhysicsBody>()
            .each([](flecs::entity e, const RigidBodyDesc& d,
                    const Lu::Component::Transform& transform) {
                // Transform::scale is visual-only; RigidBodyDesc defines the collision shape size.
                ::JPH::BodyID id = PhysicsDetail::makeRigidBody(
                    e, d, transform.position, transform.rotation);
                if (id.IsInvalid()) return;
                e.set<PhysicsBody>({id});
                if (d.kind == BodyKind::Dynamic)   e.add<PhysicsDynamic>();
                if (d.kind == BodyKind::Kinematic) e.add<PhysicsKinematic>();
                if (!e.has<PhysicsActive>() && !e.has<PhysicsInactive>()) e.add<PhysicsActive>();
            });

        // ---- PrePhysics: streaming -------------------------------------------
        auto foci = std::make_shared<std::vector<glm::vec3>>();

        world.system("ClearStreamingFoci").kind(prePhysics)
            .run([foci](flecs::iter& it) { while (it.next()) foci->clear(); });
            
        world.system<const Lu::Component::Transform>("CollectStreamingFoci")
            .with<PhysicsFocus>().kind(prePhysics)
            .each([foci](const Lu::Component::Transform& transform) {
                foci->push_back(transform.position);
            });
            
        // chunk <-> focus distance with hysteresis
        world.system<const TerrainChunk>("TerrainStreaming").kind(prePhysics)
            .interval(GetStreamingConfig().updateInterval)
            .each([foci](flecs::entity e, const TerrainChunk& c) {
                if (foci->empty()) return;   // no focus (loading screen) => keep state
                const auto& cfg = GetStreamingConfig();
                const glm::vec2 mn(c.origin.x, c.origin.z), mx = mn + glm::vec2(c.worldSize());
                float best = FLT_MAX;
                for (const auto& f : *foci) {
                    glm::vec2 p(f.x, f.z);
                    best = std::min(best, glm::length(p - glm::clamp(p, mn, mx)));
                }
                const bool active = e.has<PhysicsActive>();
                if (!active && best <= cfg.activateRadius)        PhysicsDetail::activate(e);
                else if (active && best > cfg.deactivateRadius)   PhysicsDetail::deactivate(e);
            });
            
        // ECS -> Jolt for kinematic bodies
        world.system<const Lu::Component::Transform, const PhysicsBody>("PushKinematicBodies")
            .with<PhysicsKinematic>().with<PhysicsActive>().kind(prePhysics)
            .each([](const Lu::Component::Transform& transform, const PhysicsBody& b) {
                auto& phys = Core::GetPhysics();
                phys.getBodyInterface().MoveKinematic(
                    b.id,
                    ::JPH::RVec3(transform.position.x, transform.position.y, transform.position.z),
                    Core::toJoltQuat(transform.rotation), phys.fixedTimeStep);
            });
            
        // ---- PhysicsStep ------------------------------------------------------
        world.system("PhysicsUpdate").kind(physicsStep)
            .run([](flecs::iter& it) { while (it.next()) Core::GetPhysics().step(it.delta_time()); });
            
        // ---- PostPhysics ------------------------------------------------------
        // Jolt -> ECS for awake dynamic bodies
        world.system<Lu::Component::Transform, const PhysicsBody>("PullDynamicBodies")
            .with<PhysicsDynamic>().with<PhysicsActive>().kind(postPhysics)
            .each([](Lu::Component::Transform& transform, const PhysicsBody& b) {
                auto& bi = Core::GetPhysics().getBodyInterface();
                if (!bi.IsActive(b.id)) return;   // sleeping
                ::JPH::RVec3 pos; ::JPH::Quat rot;
                bi.GetPositionAndRotation(b.id, pos, rot);
                transform.position = Core::toGlmVec3(pos);
                transform.rotation = Core::toGlmQuat(rot);
            });
            
        world.system("DispatchContacts").kind(postPhysics)
            .run([](flecs::iter& it) { while (it.next()) PhysicsDetail::dispatchContacts(it.world()); });

        // ---- ECS ray requests -> Jolt narrow phase -> ECS hit results --------
        world.system<const PhysicsRay, PhysicsRayHit>("PhysicsRayCasts")
            .with<PhysicsRayCast>().kind(postPhysics)
            .each([characterCapsules](flecs::entity e, const PhysicsRay& request, PhysicsRayHit& hit) {
                hit = PhysicsRayHit{};
                const float directionLength = glm::length(request.direction);
                if (directionLength < 1e-6f || request.maxDistance <= 0.0f) return;

                const glm::vec3 direction = request.direction / directionLength;
                const ::JPH::RRayCast ray(
                    ::JPH::RVec3(request.origin.x, request.origin.y, request.origin.z),
                    Core::toJoltVec3(direction * request.maxDistance));
                ::JPH::RayCastResult result;
                auto& physics = Core::GetPhysics();
                if (physics.getPhysicsSystem().GetNarrowPhaseQuery().CastRay(ray, result)) {
                    hit.hit = true;
                    hit.bodyId = result.mBodyID;
                    hit.entity = physics.getBodyMap().get(result.mBodyID);
                    hit.distance = result.mFraction * request.maxDistance;
                    const ::JPH::RVec3 hitPoint = ray.GetPointOnRay(result.mFraction);
                    hit.point = Core::toGlmVec3(hitPoint);

                    ::JPH::BodyLockRead lock(physics.getPhysicsSystem().GetBodyLockInterface(), result.mBodyID);
                    if (lock.Succeeded()) {
                        hit.normal = Core::toGlmVec3(lock.GetBody().GetWorldSpaceSurfaceNormal(
                            result.mSubShapeID2, hitPoint));
                    }
                }

                // CharacterVirtual is intentionally absent from Jolt's rigid-body
                // broad phase. Cast against each character's Jolt transformed shape
                // and keep whichever Jolt raycast result is closest.
                characterCapsules.each([&](flecs::entity characterEntity, const CharacterCapsule& capsule) {
                    if (!capsule.character) return;
                    ::JPH::RayCastResult characterResult;
                    characterResult.mFraction = hit.hit
                        ? hit.distance / request.maxDistance
                        : 1.0f + FLT_EPSILON;
                    const auto transformedShape = capsule.character->GetTransformedShape();
                    if (!transformedShape.CastRay(ray, characterResult)) return;

                    hit.hit = true;
                    hit.entity = characterEntity.id();
                    hit.bodyId = ::JPH::BodyID();
                    hit.distance = characterResult.mFraction * request.maxDistance;
                    const ::JPH::RVec3 hitPoint = ray.GetPointOnRay(characterResult.mFraction);
                    hit.point = Core::toGlmVec3(hitPoint);
                    hit.normal = Core::toGlmVec3(transformedShape.GetWorldSpaceSurfaceNormal(
                        characterResult.mSubShapeID2, hitPoint));
                });
            });
            
        // character hits are valid for one frame only
        world.system("CleanupCharacterHits").kind(flecs::PostFrame).multi_threaded(false)
            .run([](flecs::iter& it) { while (it.next()) it.world().remove_all<CharacterHitInfo>(flecs::Wildcard); });
    }
};

} // namespace Lca::Module
