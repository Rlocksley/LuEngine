#pragma once

#include "Global.hpp"
#include "flecs.h"
#include "AssetManager.hpp"
#include "PipelineConfig.hpp"
#include "Vertex.hpp"
#include "PhysicsComponents.hpp"
#include "PhysicsModule.hpp"
#include "CharacterCapsule.hpp"
#include "../core/component/Material.hpp"
#include "../core/component/Mesh.hpp"
#include "../core/component/Transform.hpp"
#include "../core/component/TransformGpu.hpp"

#include <array>
#include <cmath>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

// ============================================================================
//  PhysicsDebug: renders the JoltPhysics shapes as wireframe meshes
//
//      Lu::App("Game")
//          ...
//          .importModule<Lu::Module::PhysicsDebug>()     // <- the only line you need
//          .run();
//
//  Every entity that owns a Jolt shape gets an additional debug root (+ one Mesh child) per visual part:
//
//      physics entity   Transform (+ RigidBodyDesc / CharacterCapsule)      <- Jolt writes this
//      debug root       Transform + TransformGpu + PhysicsDebugPart         <- WORLD transform of the Jolt shape (own root)
//        '- debug       Mesh + Material (wireframe line pipeline)           <- Transform parent = debug root
//
//  The debug root follows the physics entity every frame (PostPhysics phase) and is destroyed
//  together with it / when its Jolt shape disappears. Why a root with an own Transform instead of
//  putting the Mesh directly below the physics entity: the Transform::scale / MeshTransform of the
//  physics entity are *visual* scales (e.g. a floor scaled 80x1x80 or a character mesh scaled 0.6),
//  but the debug mesh has to show the real Jolt shape size.
//
//  Supported shapes:
//      RigidBodyDesc  ShapeKind::Box / Sphere / Capsule   (colour depends on Static/Kinematic/Dynamic/Sensor)
//      CharacterCapsule (Jolt CharacterVirtual, capsule lifted so that Transform = feet)
//  Meshes are unit meshes that are scaled by the debug root, so no geometry has to be created at
//  runtime (the renderer only accepts new geometry before App::run()). A capsule can not be
//  scaled non uniformly, so it is built from three parts: cylinder + upper + lower hemisphere.
//
//  Tuning (before App::run(), e.g. at the start of main):
//      Lu::GetPhysicsDebugConfig().capacity = 16384;      // max. number of debug meshes
//      Lu::GetPhysicsDebugConfig().depthTest = false;     // draw through walls
//  Disable at start / toggle at runtime:  .importModule<Lu::Module::PhysicsDebug>(Lu::ModulImportType::disabled)
// ============================================================================

namespace Lu {

struct PhysicsDebugConfig {
    uint32_t capacity{8192};          // size of the debug mesh pipeline, 1 box/sphere = 1 mesh, 1 capsule = 3 meshes
    bool depthTest{true};
    std::string vertexShader{"shader/mesh_basic.vert.spv"};
    std::string fragmentShader{"shader/mesh_basic.frag.spv"};
    uint32_t sphereSegments{20};      // longitudes of spheres / capsules
    uint32_t sphereRings{10};         // latitudes of a (half) sphere

    glm::vec4 staticColor{0.25f, 0.55f, 1.00f, 1.0f};
    glm::vec4 kinematicColor{1.00f, 0.85f, 0.20f, 1.0f};
    glm::vec4 dynamicColor{0.25f, 1.00f, 0.35f, 1.0f};
    glm::vec4 sensorColor{0.95f, 0.30f, 1.00f, 1.0f};
    glm::vec4 characterColor{1.00f, 0.50f, 0.15f, 1.0f};
};

inline PhysicsDebugConfig& GetPhysicsDebugConfig() {
    static PhysicsDebugConfig config;
    return config;
}

namespace Component {

// On the debug root entity: where it is relative to the physics entity that owns the Jolt shape.
struct PhysicsDebugPart {
    flecs::entity owner;
    glm::vec3 offset{0.0f};   // local to the owner's Transform (rotated with it)
    glm::vec3 scale{1.0f};    // scale of the unit mesh
};

// On the physics entity: the debug roots that were created for it (destroyed with it).
struct PhysicsDebugHandle {
    std::array<flecs::entity, 3> parts{};
    uint32_t count{0};
};

} // namespace Component

namespace PhysicsDebugDetail {

// flecs 3.x get<T>() returns T*, flecs 4.x returns const T& -> normalise
template <typename T> const T& ref(const T* p) { return *p; }
template <typename T> const T& ref(const T& r) { return r; }

struct Assets {
    flecs::entity pipeline;
    flecs::entity box;          // half size 0.5  -> scale = full size
    flecs::entity sphere;       // radius 1       -> scale = radius
    flecs::entity hemisphereUp; // radius 1, y >= 0
    flecs::entity hemisphereDown; // radius 1, y <= 0
    flecs::entity cylinder;     // radius 1, y in [-1, 1] without caps -> scale = (r, halfHeight, r)
};

struct MeshData {
    std::vector<Vertex::Mesh> vertices;
    std::vector<uint32_t> indices;
};

inline MeshData makeBox() {
    MeshData m;
    const glm::vec4 white(1.0f);
    for (int y : {-1, 1}) for (int z : {-1, 1}) for (int x : {-1, 1}) {
        const glm::vec3 p(0.5f * x, 0.5f * y, 0.5f * z);
        m.vertices.push_back({p, glm::normalize(p), white, {0.0f, 0.0f}});
    }
    for (uint32_t i = 0; i < 8; ++i)
        for (uint32_t bit : {1u, 2u, 4u})
            if ((i & bit) == 0) { m.indices.push_back(i); m.indices.push_back(i | bit); }
    return m;
}

// Surface of revolution around +Y. Profile = rings from bottom to top, each (y, radius, normalY, normalRadius).
// The pipeline of the debug meshes does not cull, so the winding order does not matter.
struct ProfilePoint { float y, radius, ny, nr; };

inline MeshData makeLathe(const std::vector<ProfilePoint>& profile, uint32_t segments) {
    constexpr float kTwoPi = 6.28318530718f;
    MeshData m;
    const glm::vec4 white(1.0f);
    segments = std::max(segments, 3u);
    for (size_t r = 0; r < profile.size(); ++r) {
        for (uint32_t s = 0; s <= segments; ++s) {
            const float a = kTwoPi * static_cast<float>(s) / static_cast<float>(segments);
            const float c = std::cos(a), sn = std::sin(a);
            m.vertices.push_back({
                {profile[r].radius * c, profile[r].y, profile[r].radius * sn},
                glm::normalize(glm::vec3(profile[r].nr * c, profile[r].ny, profile[r].nr * sn)),
                white,
                {static_cast<float>(s) / segments, static_cast<float>(r) / std::max<size_t>(profile.size() - 1, 1)}});
        }
    }
    for (uint32_t r = 0; r + 1 < profile.size(); ++r) {
        for (uint32_t s = 0; s < segments; ++s) {
            const uint32_t a = r * (segments + 1) + s, b = a + segments + 1;
            for (uint32_t i : {a, b, a + 1, a + 1, b, b + 1}) m.indices.push_back(i);
        }
    }
    return m;
}

// Line-list version of a surface of revolution: draw each profile ring and each
// longitudinal meridian once, without the triangle diagonals from a solid mesh.
inline MeshData makeWireLathe(const std::vector<ProfilePoint>& profile, uint32_t segments) {
    constexpr float kTwoPi = 6.28318530718f;
    MeshData m;
    const glm::vec4 white(1.0f);
    segments = std::max(segments, 3u);
    for (size_t r = 0; r < profile.size(); ++r) {
        for (uint32_t s = 0; s < segments; ++s) {
            const float a = kTwoPi * static_cast<float>(s) / segments;
            const float c = std::cos(a), sn = std::sin(a);
            m.vertices.push_back({{profile[r].radius * c, profile[r].y, profile[r].radius * sn},
                glm::normalize(glm::vec3(profile[r].nr * c, profile[r].ny, profile[r].nr * sn)), white,
                {static_cast<float>(s) / segments, static_cast<float>(r) / std::max<size_t>(profile.size() - 1, 1)}});
        }
    }
    const uint32_t ringCount = static_cast<uint32_t>(profile.size());
    for (uint32_t r = 0; r < ringCount; ++r) {
        for (uint32_t s = 0; s < segments; ++s) {
            const uint32_t current = r * segments + s;
            m.indices.push_back(current); m.indices.push_back(r * segments + (s + 1) % segments);
            if (r + 1 < ringCount) {
                m.indices.push_back(current); m.indices.push_back((r + 1) * segments + s);
            }
        }
    }
    return m;
}

// Half sphere of radius 1: up = true -> y >= 0, otherwise y <= 0. full = whole sphere.
inline MeshData makeSphere(uint32_t rings, uint32_t segments, bool up, bool full) {
    constexpr float kHalfPi = 1.57079632679f;
    rings = std::max(rings, 2u);
    std::vector<ProfilePoint> profile;
    const uint32_t steps = full ? rings * 2 : rings;
    for (uint32_t i = 0; i <= steps; ++i) {
        // angle from the bottom pole (-pi/2) up to the top pole (+pi/2) for the full sphere
        float angle;
        if (full)          angle = -kHalfPi + kHalfPi * 2.0f * i / steps;
        else if (up)       angle = kHalfPi * i / steps;
        else               angle = -kHalfPi + kHalfPi * i / steps;
        const float y = std::sin(angle), r = std::cos(angle);
        profile.push_back({y, r, y, r});
    }
    return makeWireLathe(profile, segments);
}

inline MeshData makeCylinder(uint32_t segments) {
    return makeWireLathe({{-1.0f, 1.0f, 0.0f, 1.0f}, {1.0f, 1.0f, 0.0f, 1.0f}}, segments);
}

inline flecs::entity registerMesh(const std::string& name, const MeshData& m) {
    return Core::GetAssetManager().registerMeshGeometry(name, m.vertices, m.indices);
}

// ---- spawning -----------------------------------------------------------------------------------

struct PartSpec {
    flecs::entity mesh;
    glm::vec3 offset;
    glm::vec3 scale;
};

inline void spawnParts(flecs::entity owner, const Assets& assets, const PartSpec* specs, uint32_t count,
                       const glm::vec4& color) {
    using namespace Lu::Component;
    if (owner.has<PhysicsDebugHandle>()) return;   // already has a debug visual

    flecs::world world = owner.world();
    const Transform& ownerTransform = ref(owner.get<Transform>());

    Lu::Component::Material material{};
    material.albedo = color;
    material.ambient = glm::vec4(glm::vec3(color) * 0.5f, color.a);
    material.emission = glm::vec4(glm::vec3(color) * 0.25f, 0.0f);
    material.roughness = 1.0f;

    PhysicsDebugHandle handle;
    for (uint32_t i = 0; i < count && i < handle.parts.size(); ++i) {
        Transform t{};
        t.position = ownerTransform.position + ownerTransform.rotation * specs[i].offset;
        t.rotation = ownerTransform.rotation;
        t.scale = specs[i].scale;

        // debug root: Transform parent of the Mesh, has the (WORLD) transform of the Jolt shape.
        // It must NOT be a child of the owner: a child's Transform is local to its parent's Transform, but
        // `t` already is the world transform of the shape. The root is destroyed through PhysicsDebugHandle.
        flecs::entity root = world.entity()
            .set<Transform>(t)
            .add<TransformGpu>()
            .set<PhysicsDebugPart>({owner, specs[i].offset, specs[i].scale});

        // debug mesh: wireframe line pipeline
        world.entity()
            .set(flecs::Parent{root})
            .set<Lu::Component::Mesh>({.mesh = specs[i].mesh, .pipeline = assets.pipeline})
            .set<Lu::Component::Material>(material);

        handle.parts[handle.count++] = root;
    }
    owner.set<PhysicsDebugHandle>(handle);
}

inline void spawnRigidBody(flecs::entity e, const Assets& assets, const Lu::Component::RigidBodyDesc& d) {
    const auto& cfg = GetPhysicsDebugConfig();
    glm::vec4 color = d.kind == Lu::Component::BodyKind::Static    ? cfg.staticColor
                    : d.kind == Lu::Component::BodyKind::Kinematic ? cfg.kinematicColor
                                                                    : cfg.dynamicColor;
    if (d.isSensor) color = cfg.sensorColor;

    switch (d.shape) {
    case Lu::Component::ShapeKind::Box: {
        const PartSpec box{assets.box, glm::vec3(0.0f), d.halfExtents * 1.9f};
        spawnParts(e, assets, &box, 1, color);
        break;
    }
    case Lu::Component::ShapeKind::Sphere: {
        const PartSpec sphere{assets.sphere, glm::vec3(0.0f), glm::vec3(d.halfExtents.x * 0.9f)};
        spawnParts(e, assets, &sphere, 1, color);
        break;
    }
    case Lu::Component::ShapeKind::Capsule: {
        // RigidBodyDesc: x = radius, y = half height of the cylinder part (Jolt CapsuleShape, axis Y)
        const float r = d.halfExtents.x, h = d.halfExtents.y;
        const PartSpec parts[3] = {
            {assets.cylinder,       glm::vec3(0.0f),         glm::vec3(r, h, r)},
            {assets.hemisphereUp,   glm::vec3(0.0f,  h, 0.0f), glm::vec3(r)},
            {assets.hemisphereDown, glm::vec3(0.0f, -h, 0.0f), glm::vec3(r)},
        };
        spawnParts(e, assets, parts, 3, color);
        break;
    }
    }
}

inline void spawnCharacter(flecs::entity e, const Assets& assets, const Lu::Component::CharacterCapsule& c) {
    // Transform is at the feet; Jolt lifts the capsule by halfHeight + radius.
    const float lift = c.halfHeight + c.radius;
    const PartSpec parts[3] = {
        {assets.cylinder,       glm::vec3(0.0f, lift, 0.0f),                glm::vec3(c.radius, c.halfHeight, c.radius)},
        {assets.hemisphereUp,   glm::vec3(0.0f, lift + c.halfHeight, 0.0f), glm::vec3(c.radius)},
        {assets.hemisphereDown, glm::vec3(0.0f, lift - c.halfHeight, 0.0f), glm::vec3(c.radius)},
    };
    spawnParts(e, assets, parts, 3, GetPhysicsDebugConfig().characterColor);
}

} // namespace PhysicsDebugDetail

namespace Module {

struct PhysicsDebug {
    PhysicsDebug(flecs::world& world) {
        using namespace Lu::Component;
        using namespace PhysicsDebugDetail;
        using Lu::Component::CharacterCapsule;
        using Lu::Component::PhysicsBody;
        using Lu::Component::RigidBodyDesc;

        // the observers below need the Jolt bodies / phases (no-op when it is already imported)
        world.import<Lu::Module::PhysicsModule>();

        world.component<PhysicsDebugPart>();
        world.component<PhysicsDebugHandle>();

        // ---- shared assets: unit wire meshes + line-list pipeline ---------------------------------
        // (created now, the renderer does not accept new pipelines / geometry after App::run())
        const auto& cfg = GetPhysicsDebugConfig();
        auto assets = std::make_shared<Assets>();
        assets->box = registerMesh("Mesh::PhysicsDebugBox", makeBox());
        assets->sphere = registerMesh("Mesh::PhysicsDebugSphere", makeSphere(cfg.sphereRings, cfg.sphereSegments, true, true));
        assets->hemisphereUp = registerMesh("Mesh::PhysicsDebugHemisphereUp", makeSphere(cfg.sphereRings, cfg.sphereSegments, true, false));
        assets->hemisphereDown = registerMesh("Mesh::PhysicsDebugHemisphereDown", makeSphere(cfg.sphereRings, cfg.sphereSegments, false, false));
        assets->cylinder = registerMesh("Mesh::PhysicsDebugCylinder", makeCylinder(cfg.sphereSegments));
        assets->pipeline = Core::GetAssetManager().registerMeshPipe(Lu::GraphicsPipelineConfig{
            .name = "MeshPipe::PhysicsDebug", .capacity = cfg.capacity,
            .vertexShader = cfg.vertexShader, .fragmentShader = cfg.fragmentShader,
            .topology = VK_PRIMITIVE_TOPOLOGY_LINE_LIST,
            .cullMode = VK_CULL_MODE_NONE,
            .depthTestEnable = cfg.depthTest,
            // Lines are opaque (blendEnable = false), so they MUST write depth. Without it the lines of
            // different shapes never depth-sort against each other and the winner of every overlapping
            // pixel is whichever draw the GPU culling compute shader happened to append last
            // (atomicAdd order, different every frame) -> shimmering. (Depth writes are ignored by Vulkan
            // when the depth test is disabled, so this is also correct for depthTest = false.)
            .depthWriteEnable = cfg.depthTest,
            .blendEnable = false});

        // ---- destroy: the debug roots die with their Jolt shape (hook also fires when the owner is destructed) ----
        world.component<PhysicsDebugHandle>().on_remove([](flecs::entity owner, PhysicsDebugHandle& h) {
            if (ecs_is_fini(owner.world().c_ptr())) return;   // world shutdown destroys everything anyway
            for (uint32_t i = 0; i < h.count; ++i) {
                if (h.parts[i].is_alive()) h.parts[i].destruct();   // the Mesh child goes with its parent
            }
            h.count = 0;
        });

        // ---- create: RigidBodyDesc + Transform => PhysicsBody exists => debug mesh -----------------------
        world.observer<const RigidBodyDesc, const Lu::Component::Transform>("PhysicsDebugCreateRigidBody")
            .event(flecs::OnAdd).with<PhysicsBody>()
            .each([assets](flecs::entity e, const RigidBodyDesc& d, const Lu::Component::Transform&) {
                spawnRigidBody(e, *assets, d);
            });

        // ---- create: CharacterCapsule + Transform (Jolt CharacterVirtual) -----------------------------------
        world.observer<const CharacterCapsule, const Lu::Component::Transform>("PhysicsDebugCreateCharacter")
            .event(flecs::OnSet)
            .each([assets](flecs::entity e, const CharacterCapsule& c, const Lu::Component::Transform&) {
                spawnCharacter(e, *assets, c);
            });

        // ---- destroy: shape removed => debug children removed (on_remove hook of PhysicsDebugHandle) --------
        world.observer<>("PhysicsDebugDestroyRigidBody")
            .with<PhysicsBody>()
            .event(flecs::OnRemove)
            .each([](flecs::entity e) { e.remove<PhysicsDebugHandle>(); });

        world.observer<>("PhysicsDebugDestroyCharacter")
            .with<CharacterCapsule>()
            .event(flecs::OnRemove)
            .each([](flecs::entity e) { e.remove<PhysicsDebugHandle>(); });

        // ---- follow the Jolt shape: after Jolt wrote the Transforms (PullDynamicBodies, CharacterCapsuleUpdate) ----
        flecs::entity postPhysics = world.lookup("PostPhysics");
        world.system<Lu::Component::Transform, const PhysicsDebugPart>("PhysicsDebugFollowShape")
            .kind(postPhysics)
            .each([](Lu::Component::Transform& t, const PhysicsDebugPart& part) {
                if (!part.owner.is_alive() || !part.owner.has<Lu::Component::Transform>()) return;
                const auto& owner = ref(part.owner.get<Lu::Component::Transform>());
                t.position = owner.position + owner.rotation * part.offset;
                t.rotation = owner.rotation;
                t.scale = part.scale;
            });

        // ---- shapes that already exist (this module was imported after the scene was built) ---------------
        world.defer_begin();
        world.query<const RigidBodyDesc, const Lu::Component::Transform, const PhysicsBody>()
            .each([assets](flecs::entity e, const RigidBodyDesc& d, const Lu::Component::Transform&, const PhysicsBody&) {
                spawnRigidBody(e, *assets, d);
            });
        world.query<const CharacterCapsule, const Lu::Component::Transform>()
            .each([assets](flecs::entity e, const CharacterCapsule& c, const Lu::Component::Transform&) {
                spawnCharacter(e, *assets, c);
            });
        world.defer_end();
    }
};

} // namespace Module
} // namespace Lu
