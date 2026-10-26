#include "App.hpp"
#include "Shape.hpp"
#include "FlyingCamera.hpp"
#include "PhysicsModule.hpp"
#include "component/Light.hpp"
#include "component/Material.hpp"
#include "component/Mesh.hpp"
#include "component/Transform.hpp"
#include "component/TransformGpu.hpp"

#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Collision/CollideShape.h>
#include <Jolt/Physics/Collision/CollisionCollectorImpl.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <string>
#include <vector>

using Lu::Component::InputState;
using Lu::Component::Transform;
using Lu::Component::FlyingCamera;
using Lca::Component::BodyKind;
using Lca::Component::PhysicsBody;
using Lca::Component::PhysicsRay;
using Lca::Component::PhysicsRayCast;
using Lca::Component::PhysicsRayHit;
using Lca::Component::RigidBodyDesc;
using Lca::Component::ShapeKind;

namespace {

constexpr float kFloorHalfWidth  = 80.0f;
constexpr float kFloorHalfHeight = 1.0f;
constexpr float kFloorTopY       = 0.0f;
constexpr float kDebrisLifetime  = 2.0f;

template <typename T> const T& componentRef(const T* value) { return *value; }
template <typename T> const T& componentRef(const T& value) { return value; }

// Material helpers for normal opaque, vibrant selected, ghost valid, and ghost invalid (red) states
Lu::Component::Material makeNormalMaterial(const glm::vec3& color, float roughness = 0.45f, float metallic = 0.10f) {
    Lu::Component::Material mat{};
    mat.albedo    = glm::vec4(color, 1.0f);
    mat.ambient   = glm::vec4(color * 0.18f, 1.0f);
    mat.emission  = glm::vec4(0.0f);
    mat.roughness = roughness;
    mat.metallic  = metallic;
    return mat;
}

Lu::Component::Material makeVibrantMaterial(const glm::vec3& vibrantColor, float roughness = 0.22f, float metallic = 0.25f) {
    Lu::Component::Material mat{};
    mat.albedo    = glm::vec4(vibrantColor, 1.0f);
    mat.ambient   = glm::vec4(vibrantColor * 0.42f, 1.0f);
    mat.emission  = glm::vec4(vibrantColor * 0.38f, 1.0f);
    mat.roughness = roughness;
    mat.metallic  = metallic;
    return mat;
}

Lu::Component::Material makeGhostMaterial(const glm::vec3& color) {
    Lu::Component::Material mat{};
    mat.albedo    = glm::vec4(color, 0.44f);
    mat.ambient   = glm::vec4(color * 0.45f, 1.0f);
    mat.emission  = glm::vec4(color * 0.15f, 0.0f);
    mat.roughness = 0.35f;
    mat.metallic  = 0.05f;
    return mat;
}

Lu::Component::Material makeRedGhostMaterial() {
    Lu::Component::Material mat{};
    mat.albedo    = glm::vec4(1.0f, 0.12f, 0.12f, 0.52f);
    mat.ambient   = glm::vec4(0.55f, 0.05f, 0.05f, 1.0f);
    mat.emission  = glm::vec4(0.45f, 0.02f, 0.02f, 0.0f);
    mat.roughness = 0.35f;
    mat.metallic  = 0.05f;
    return mat;
}

// Single primitive shape building block within a house or street blueprint
struct BlueprintPart {
    flecs::entity meshEntity;
    ShapeKind     physicsShape{ShapeKind::Box};
    glm::vec3     localOffset{0.0f};   // relative to ground base (y = 0 at ground level)
    glm::quat     localRotation{1.0f, 0.0f, 0.0f, 0.0f};
    glm::vec3     scale{1.0f};         // half-extents for unit shapes (1x1x1 halfSize)
    glm::vec3     baseColor{1.0f};
    glm::vec3     vibrantColor{1.0f};
    float         roughness{0.45f};
    float         metallic{0.10f};
};

// Complete definition of a placeable house or street type
struct Blueprint {
    std::string                name;
    glm::vec3                  boundingHalfExtents{1.0f}; // surrounding Jolt physics cube half-extents
    std::vector<BlueprintPart> parts;
};

// Visual entity pair: root holds Transform + TransformGpu, child holds Mesh + Material
struct VisualPartInstance {
    flecs::entity transformEntity;
    flecs::entity meshEntity;
    glm::vec3     baseColor{1.0f};
    glm::vec3     vibrantColor{1.0f};
    float         roughness{0.45f};
    float         metallic{0.10f};
};

inline void destroyVisualPart(VisualPartInstance& part) {
    if (part.meshEntity.is_alive()) {
        part.meshEntity.remove<Lu::Component::Mesh>();
        part.meshEntity.destruct();
    }
    if (part.transformEntity.is_alive()) {
        part.transformEntity.destruct();
    }
}

inline VisualPartInstance spawnVisualPart(
    flecs::world& world,
    const glm::vec3& position,
    const glm::quat& rotation,
    const glm::vec3& scale,
    flecs::entity shapeMesh,
    flecs::entity pipeline,
    const Lu::Component::Material& material,
    const glm::vec3& baseColor,
    const glm::vec3& vibrantColor,
    float roughness,
    float metallic)
{
    Transform transform{};
    transform.position = position;
    transform.rotation = rotation;
    transform.scale    = scale;

    flecs::entity root = world.entity()
        .set<Transform>(transform)
        .add<Lu::Component::TransformGpu>();

    flecs::entity child = world.entity()
        .set(flecs::Parent{root})
        .set<Lu::Component::Mesh>({.mesh = shapeMesh, .pipeline = pipeline})
        .set<Lu::Component::Material>(material);

    return VisualPartInstance{
        .transformEntity = root,
        .meshEntity      = child,
        .baseColor       = baseColor,
        .vibrantColor    = vibrantColor,
        .roughness       = roughness,
        .metallic        = metallic
    };
}

// Component attached to every placed building/street root entity (which also owns the surrounding Jolt physics cube)
struct PlacedObject {
    uint32_t                        blueprintIndex{0};
    glm::vec3                       basePosition{0.0f};
    float                           yaw{0.0f};
    glm::quat                       rotation{1.0f, 0.0f, 0.0f, 0.0f};
    glm::vec3                       boxCenter{0.0f};
    glm::vec3                       boxHalfExtents{1.0f};
    bool                            isVibrant{false};
    std::vector<VisualPartInstance> visualParts;
};

// Wrap an angle in radians to the interval [-pi, +pi]
inline float wrapAnglePi(float angle) {
    constexpr float kPi    = 3.14159265358979323846f;
    constexpr float kTwoPi = 2.0f * kPi;
    angle = std::fmod(angle + kPi, kTwoPi);
    if (angle < 0.0f) angle += kTwoPi;
    return angle - kPi;
}

// Component attached to dynamic primitive shapes spawned when a placed object is destroyed with 'X'
struct DebrisPiece {
    flecs::entity meshChild;
    float         remainingSeconds{kDebrisLifetime};
};

// 3D Oriented Bounding Box (OBB) Separating Axis Theorem overlap check
bool obbIntersectsObb(
    const glm::vec3& centerA, const glm::vec3& halfA, const glm::quat& rotA,
    const glm::vec3& centerB, const glm::vec3& halfB, const glm::quat& rotB)
{
    const glm::mat3 axesA = glm::mat3_cast(rotA);
    const glm::mat3 axesB = glm::mat3_cast(rotB);
    const glm::vec3 diff  = centerB - centerA;

    const glm::vec3 aAxes[3] = { axesA[0], axesA[1], axesA[2] };
    const glm::vec3 bAxes[3] = { axesB[0], axesB[1], axesB[2] };

    auto overlapOnAxis = [&](const glm::vec3& axis) -> bool {
        const float lenSq = glm::dot(axis, axis);
        if (lenSq < 1e-6f) return true;
        const glm::vec3 n = axis / std::sqrt(lenSq);
        const float projA =
            halfA.x * std::abs(glm::dot(n, aAxes[0])) +
            halfA.y * std::abs(glm::dot(n, aAxes[1])) +
            halfA.z * std::abs(glm::dot(n, aAxes[2]));
        const float projB =
            halfB.x * std::abs(glm::dot(n, bAxes[0])) +
            halfB.y * std::abs(glm::dot(n, bAxes[1])) +
            halfB.z * std::abs(glm::dot(n, bAxes[2]));
        const float dist = std::abs(glm::dot(diff, n));
        return dist <= (projA + projB);
    };

    for (int i = 0; i < 3; ++i) {
        if (!overlapOnAxis(aAxes[i])) return false;
        if (!overlapOnAxis(bAxes[i])) return false;
    }
    for (int i = 0; i < 3; ++i) {
        for (int j = 0; j < 3; ++j) {
            if (!overlapOnAxis(glm::cross(aAxes[i], bAxes[j]))) return false;
        }
    }
    return true;
}

// Jolt BodyFilter that ignores the static floor body when testing the ghost's surrounding physics box
class IgnoreFloorBodyFilter final : public ::JPH::BodyFilter {
public:
    explicit IgnoreFloorBodyFilter(::JPH::BodyID floorId) : mFloorId(floorId) {}
    bool ShouldCollide(const ::JPH::BodyID& inBodyID) const override {
        return inBodyID != mFloorId;
    }
private:
    ::JPH::BodyID mFloorId;
};

// Build all 6 city-builder blueprints (houses & streets) out of primitive shapes
std::vector<Blueprint> createCityBlueprints(flecs::world& world) {
    const auto cubeMesh     = world.lookup("Mesh::UnitCube");
    const auto sphereMesh   = world.lookup("Mesh::UnitSphere");
    const auto gableMesh    = world.lookup("Mesh::UnitGableRoof");
    const auto pyramidMesh  = world.lookup("Mesh::UnitPyramid");
    const auto cylinderMesh = world.lookup("Mesh::UnitCylinder");
    const auto coneMesh     = world.lookup("Mesh::UnitCone");

    std::vector<Blueprint> blueprints;

    // -------------------------------------------------------------------------
    // 1. House 1: Cozy Country Cottage (walls, pitched roof, chimney, door, windows, porch)
    // -------------------------------------------------------------------------
    {
        Blueprint b;
        b.name = "House 1 (Cozy Cottage)";
        b.boundingHalfExtents = glm::vec3(1.65f, 1.55f, 1.55f);

        // Stone foundation base
        b.parts.push_back({cubeMesh, ShapeKind::Box, {0.0f, 0.15f, 0.0f}, {1,0,0,0}, {1.45f, 0.15f, 1.25f},
            {0.52f, 0.50f, 0.48f}, {0.85f, 0.78f, 0.35f}, 0.65f, 0.05f});
        // Warm cream cottage walls
        b.parts.push_back({cubeMesh, ShapeKind::Box, {0.0f, 1.05f, 0.0f}, {1,0,0,0}, {1.35f, 0.75f, 1.15f},
            {0.93f, 0.87f, 0.76f}, {1.00f, 0.92f, 0.32f}, 0.50f, 0.05f});
        // Pitched terracotta gable roof
        b.parts.push_back({gableMesh, ShapeKind::Box, {0.0f, 2.35f, 0.0f}, {1,0,0,0}, {1.55f, 0.58f, 1.32f},
            {0.78f, 0.28f, 0.18f}, {1.00f, 0.22f, 0.45f}, 0.42f, 0.10f});
        // Brick chimney on right roof slope
        b.parts.push_back({cubeMesh, ShapeKind::Box, {0.85f, 2.55f, -0.45f}, {1,0,0,0}, {0.22f, 0.55f, 0.22f},
            {0.58f, 0.22f, 0.18f}, {1.00f, 0.35f, 0.15f}, 0.55f, 0.05f});
        // Wooden front door (facing +Z toward camera)
        b.parts.push_back({cubeMesh, ShapeKind::Box, {0.0f, 0.72f, 1.16f}, {1,0,0,0}, {0.28f, 0.44f, 0.06f},
            {0.42f, 0.24f, 0.12f}, {0.95f, 0.55f, 0.10f}, 0.48f, 0.08f});
        // Front porch step
        b.parts.push_back({cubeMesh, ShapeKind::Box, {0.0f, 0.12f, 1.38f}, {1,0,0,0}, {0.46f, 0.12f, 0.20f},
            {0.60f, 0.58f, 0.55f}, {0.95f, 0.85f, 0.35f}, 0.60f, 0.05f});
        // Left & right front windows (sky-blue glass)
        b.parts.push_back({cubeMesh, ShapeKind::Box, {-0.75f, 1.08f, 1.16f}, {1,0,0,0}, {0.26f, 0.26f, 0.05f},
            {0.35f, 0.72f, 0.92f}, {0.15f, 0.95f, 1.00f}, 0.15f, 0.45f});
        b.parts.push_back({cubeMesh, ShapeKind::Box, {0.75f, 1.08f, 1.16f}, {1,0,0,0}, {0.26f, 0.26f, 0.05f},
            {0.35f, 0.72f, 0.92f}, {0.15f, 0.95f, 1.00f}, 0.15f, 0.45f});

        blueprints.push_back(std::move(b));
    }

    // -------------------------------------------------------------------------
    // 2. House 2: Two-Story Villa with Side Wing & Pine Tree
    // -------------------------------------------------------------------------
    {
        Blueprint b;
        b.name = "House 2 (Two-Story Villa)";
        b.boundingHalfExtents = glm::vec3(2.25f, 2.05f, 1.75f);

        // Villa stone foundation
        b.parts.push_back({cubeMesh, ShapeKind::Box, {0.0f, 0.15f, 0.0f}, {1,0,0,0}, {2.10f, 0.15f, 1.50f},
            {0.55f, 0.53f, 0.50f}, {0.85f, 0.75f, 0.35f}, 0.60f, 0.05f});
        // Main two-story sandstone body
        b.parts.push_back({cubeMesh, ShapeKind::Box, {-0.45f, 1.45f, 0.0f}, {1,0,0,0}, {1.25f, 1.15f, 1.25f},
            {0.92f, 0.82f, 0.65f}, {1.00f, 0.75f, 0.25f}, 0.46f, 0.08f});
        // Pyramid hip roof over main two-story block
        b.parts.push_back({pyramidMesh, ShapeKind::Box, {-0.45f, 3.25f, 0.0f}, {1,0,0,0}, {1.42f, 0.65f, 1.42f},
            {0.24f, 0.34f, 0.48f}, {0.25f, 0.70f, 1.00f}, 0.38f, 0.20f});
        // Right-side single-story sun wing
        b.parts.push_back({cubeMesh, ShapeKind::Box, {1.25f, 0.95f, -0.10f}, {1,0,0,0}, {0.72f, 0.65f, 1.00f},
            {0.95f, 0.89f, 0.78f}, {1.00f, 0.88f, 0.35f}, 0.48f, 0.08f});
        // Pitched gable roof on side wing
        b.parts.push_back({gableMesh, ShapeKind::Box, {1.25f, 1.98f, -0.10f}, {1,0,0,0}, {0.84f, 0.38f, 1.12f},
            {0.28f, 0.38f, 0.52f}, {0.30f, 0.80f, 1.00f}, 0.38f, 0.20f});
        // Front door
        b.parts.push_back({cubeMesh, ShapeKind::Box, {-0.45f, 0.75f, 1.26f}, {1,0,0,0}, {0.30f, 0.46f, 0.06f},
            {0.38f, 0.20f, 0.10f}, {1.00f, 0.48f, 0.12f}, 0.45f, 0.10f});
        // Upper floor balcony windows
        b.parts.push_back({cubeMesh, ShapeKind::Box, {-0.95f, 1.95f, 1.26f}, {1,0,0,0}, {0.28f, 0.28f, 0.05f},
            {0.40f, 0.78f, 0.96f}, {0.20f, 1.00f, 0.95f}, 0.15f, 0.50f});
        b.parts.push_back({cubeMesh, ShapeKind::Box, {0.05f, 1.95f, 1.26f}, {1,0,0,0}, {0.28f, 0.28f, 0.05f},
            {0.40f, 0.78f, 0.96f}, {0.20f, 1.00f, 0.95f}, 0.15f, 0.50f});
        // Garden pine tree trunk (Cylinder) + two Cone foliage tiers
        b.parts.push_back({cylinderMesh, ShapeKind::Box, {1.55f, 0.65f, 1.12f}, {1,0,0,0}, {0.16f, 0.38f, 0.16f},
            {0.45f, 0.28f, 0.14f}, {0.85f, 0.52f, 0.18f}, 0.65f, 0.02f});
        b.parts.push_back({coneMesh, ShapeKind::Box, {1.55f, 1.45f, 1.12f}, {1,0,0,0}, {0.55f, 0.55f, 0.55f},
            {0.18f, 0.52f, 0.24f}, {0.15f, 1.00f, 0.42f}, 0.50f, 0.05f});
        b.parts.push_back({coneMesh, ShapeKind::Box, {1.55f, 2.15f, 1.12f}, {1,0,0,0}, {0.42f, 0.48f, 0.42f},
            {0.22f, 0.62f, 0.28f}, {0.30f, 1.00f, 0.50f}, 0.50f, 0.05f});

        blueprints.push_back(std::move(b));
    }

    // -------------------------------------------------------------------------
    // 3. House 3: Civic Clock Tower / Town Hall
    // -------------------------------------------------------------------------
    {
        Blueprint b;
        b.name = "House 3 (Clock Tower Hall)";
        b.boundingHalfExtents = glm::vec3(1.85f, 2.95f, 1.75f);

        // Stepped granite base
        b.parts.push_back({cubeMesh, ShapeKind::Box, {0.0f, 0.20f, 0.0f}, {1,0,0,0}, {1.70f, 0.20f, 1.55f},
            {0.58f, 0.58f, 0.62f}, {0.80f, 0.70f, 1.00f}, 0.55f, 0.10f});
        // Lower civic hall block
        b.parts.push_back({cubeMesh, ShapeKind::Box, {0.0f, 1.15f, 0.0f}, {1,0,0,0}, {1.50f, 0.75f, 1.30f},
            {0.86f, 0.74f, 0.62f}, {1.00f, 0.62f, 0.28f}, 0.48f, 0.08f});
        // Gable roof over civic hall
        b.parts.push_back({gableMesh, ShapeKind::Box, {0.0f, 2.25f, 0.0f}, {1,0,0,0}, {1.65f, 0.42f, 1.42f},
            {0.22f, 0.56f, 0.48f}, {0.10f, 1.00f, 0.78f}, 0.35f, 0.25f});
        // Central tall clock tower shaft
        b.parts.push_back({cubeMesh, ShapeKind::Box, {0.0f, 3.25f, -0.10f}, {1,0,0,0}, {0.72f, 1.35f, 0.72f},
            {0.92f, 0.88f, 0.80f}, {1.00f, 0.85f, 0.40f}, 0.45f, 0.10f});
        // Two front marble columns flanking the entrance (Cylinder)
        b.parts.push_back({cylinderMesh, ShapeKind::Box, {-0.62f, 1.10f, 1.36f}, {1,0,0,0}, {0.14f, 0.72f, 0.14f},
            {0.95f, 0.95f, 0.96f}, {0.90f, 0.95f, 1.00f}, 0.30f, 0.15f});
        b.parts.push_back({cylinderMesh, ShapeKind::Box, {0.62f, 1.10f, 1.36f}, {1,0,0,0}, {0.14f, 0.72f, 0.14f},
            {0.95f, 0.95f, 0.96f}, {0.90f, 0.95f, 1.00f}, 0.30f, 0.15f});
        // Golden clock disc on the tower front (+Z)
        b.parts.push_back({sphereMesh, ShapeKind::Sphere, {0.0f, 3.85f, 0.64f}, {1,0,0,0}, {0.32f, 0.32f, 0.10f},
            {0.95f, 0.82f, 0.28f}, {1.00f, 0.95f, 0.20f}, 0.20f, 0.65f});
        // Copper spire pyramid roof on top of tower
        b.parts.push_back({pyramidMesh, ShapeKind::Box, {0.0f, 5.15f, -0.10f}, {1,0,0,0}, {0.88f, 0.60f, 0.88f},
            {0.20f, 0.64f, 0.54f}, {0.15f, 1.00f, 0.85f}, 0.32f, 0.30f});
        // Golden finial sphere at peak
        b.parts.push_back({sphereMesh, ShapeKind::Sphere, {0.0f, 5.82f, -0.10f}, {1,0,0,0}, {0.16f, 0.16f, 0.16f},
            {0.98f, 0.84f, 0.22f}, {1.00f, 0.96f, 0.30f}, 0.18f, 0.80f});

        blueprints.push_back(std::move(b));
    }

    // -------------------------------------------------------------------------
    // 4. Street: Straight Road Segment (asphalt, sidewalks, dashed lane stripes)
    // -------------------------------------------------------------------------
    {
        Blueprint b;
        b.name = "Street (Straight Road)";
        b.boundingHalfExtents = glm::vec3(2.00f, 0.22f, 2.00f);

        // Dark asphalt roadbed
        b.parts.push_back({cubeMesh, ShapeKind::Box, {0.0f, 0.08f, 0.0f}, {1,0,0,0}, {1.98f, 0.08f, 1.98f},
            {0.20f, 0.21f, 0.24f}, {0.35f, 0.42f, 0.78f}, 0.75f, 0.05f});
        // Left raised stone curb / sidewalk
        b.parts.push_back({cubeMesh, ShapeKind::Box, {-1.68f, 0.16f, 0.0f}, {1,0,0,0}, {0.30f, 0.16f, 1.98f},
            {0.70f, 0.72f, 0.74f}, {0.30f, 0.95f, 0.90f}, 0.55f, 0.08f});
        // Right raised stone curb / sidewalk
        b.parts.push_back({cubeMesh, ShapeKind::Box, {1.68f, 0.16f, 0.0f}, {1,0,0,0}, {0.30f, 0.16f, 1.98f},
            {0.70f, 0.72f, 0.74f}, {0.30f, 0.95f, 0.90f}, 0.55f, 0.08f});
        // Three dashed center lane markings
        for (int i = -1; i <= 1; ++i) {
            b.parts.push_back({cubeMesh, ShapeKind::Box, {0.0f, 0.17f, static_cast<float>(i) * 1.20f}, {1,0,0,0},
                {0.09f, 0.03f, 0.34f},
                {0.95f, 0.92f, 0.78f}, {1.00f, 0.96f, 0.25f}, 0.35f, 0.10f});
        }

        blueprints.push_back(std::move(b));
    }

    // -------------------------------------------------------------------------
    // 5. Street Corner: 90-Degree Turn with Streetlamp
    // -------------------------------------------------------------------------
    {
        Blueprint b;
        b.name = "Street Corner (90-Deg Turn)";
        b.boundingHalfExtents = glm::vec3(2.00f, 1.15f, 2.00f);

        // Dark asphalt base
        b.parts.push_back({cubeMesh, ShapeKind::Box, {0.0f, 0.08f, 0.0f}, {1,0,0,0}, {1.98f, 0.08f, 1.98f},
            {0.20f, 0.21f, 0.24f}, {0.35f, 0.42f, 0.78f}, 0.75f, 0.05f});
        // Outer left curb (along -X edge)
        b.parts.push_back({cubeMesh, ShapeKind::Box, {-1.68f, 0.16f, 0.0f}, {1,0,0,0}, {0.30f, 0.16f, 1.98f},
            {0.70f, 0.72f, 0.74f}, {0.30f, 0.95f, 0.90f}, 0.55f, 0.08f});
        // Outer back curb (along -Z edge, forming 90-deg turn from +Z to +X)
        b.parts.push_back({cubeMesh, ShapeKind::Box, {0.30f, 0.16f, -1.68f}, {1,0,0,0}, {1.68f, 0.16f, 0.30f},
            {0.70f, 0.72f, 0.74f}, {0.30f, 0.95f, 0.90f}, 0.55f, 0.08f});
        // Inner corner curb block (+X, +Z corner)
        b.parts.push_back({cubeMesh, ShapeKind::Box, {1.68f, 0.16f, 1.68f}, {1,0,0,0}, {0.30f, 0.16f, 0.30f},
            {0.70f, 0.72f, 0.74f}, {0.30f, 0.95f, 0.90f}, 0.55f, 0.08f});
        // L-shaped turning center lane dashes
        b.parts.push_back({cubeMesh, ShapeKind::Box, {0.0f, 0.17f, 1.05f}, {1,0,0,0}, {0.09f, 0.03f, 0.42f},
            {0.95f, 0.92f, 0.78f}, {1.00f, 0.96f, 0.25f}, 0.35f, 0.10f});
        b.parts.push_back({cubeMesh, ShapeKind::Box, {1.05f, 0.17f, 0.0f}, {1,0,0,0}, {0.42f, 0.03f, 0.09f},
            {0.95f, 0.92f, 0.78f}, {1.00f, 0.96f, 0.25f}, 0.35f, 0.10f});
        // Streetlamp post on outer corner (-X, -Z) + glowing lantern sphere
        b.parts.push_back({cylinderMesh, ShapeKind::Box, {-1.55f, 1.10f, -1.55f}, {1,0,0,0}, {0.09f, 0.80f, 0.09f},
            {0.25f, 0.28f, 0.30f}, {0.65f, 0.75f, 0.95f}, 0.35f, 0.60f});
        b.parts.push_back({sphereMesh, ShapeKind::Sphere, {-1.55f, 2.02f, -1.55f}, {1,0,0,0}, {0.22f, 0.22f, 0.22f},
            {1.00f, 0.92f, 0.55f}, {1.00f, 0.98f, 0.35f}, 0.15f, 0.10f});

        blueprints.push_back(std::move(b));
    }

    // -------------------------------------------------------------------------
    // 6. Street Crossroads: 4-Way Intersection with Crosswalks & Roundabout Bollard
    // -------------------------------------------------------------------------
    {
        Blueprint b;
        b.name = "Street Crossroads (4-Way)";
        b.boundingHalfExtents = glm::vec3(2.00f, 0.60f, 2.00f);

        // Dark asphalt intersection square
        b.parts.push_back({cubeMesh, ShapeKind::Box, {0.0f, 0.08f, 0.0f}, {1,0,0,0}, {1.98f, 0.08f, 1.98f},
            {0.20f, 0.21f, 0.24f}, {0.35f, 0.42f, 0.78f}, 0.75f, 0.05f});
        // Four corner sidewalk blocks
        const float cPos[2] = {-1.68f, 1.68f};
        for (float cx : cPos) {
            for (float cz : cPos) {
                b.parts.push_back({cubeMesh, ShapeKind::Box, {cx, 0.16f, cz}, {1,0,0,0}, {0.30f, 0.16f, 0.30f},
                    {0.70f, 0.72f, 0.74f}, {0.30f, 0.95f, 0.90f}, 0.55f, 0.08f});
            }
        }
        // Pedestrian crosswalk stripes on all 4 approaches
        for (int s = -1; s <= 1; ++s) {
            const float offset = static_cast<float>(s) * 0.55f;
            b.parts.push_back({cubeMesh, ShapeKind::Box, {offset, 0.17f,  1.45f}, {1,0,0,0}, {0.12f, 0.03f, 0.28f},
                {0.94f, 0.94f, 0.94f}, {1.00f, 0.96f, 0.30f}, 0.35f, 0.10f});
            b.parts.push_back({cubeMesh, ShapeKind::Box, {offset, 0.17f, -1.45f}, {1,0,0,0}, {0.12f, 0.03f, 0.28f},
                {0.94f, 0.94f, 0.94f}, {1.00f, 0.96f, 0.30f}, 0.35f, 0.10f});
        }
        // Center roundabout stone island (Cylinder) + glowing beacon (Sphere)
        b.parts.push_back({cylinderMesh, ShapeKind::Box, {0.0f, 0.26f, 0.0f}, {1,0,0,0}, {0.38f, 0.12f, 0.38f},
            {0.62f, 0.62f, 0.65f}, {0.40f, 0.90f, 1.00f}, 0.50f, 0.15f});
        b.parts.push_back({sphereMesh, ShapeKind::Sphere, {0.0f, 0.52f, 0.0f}, {1,0,0,0}, {0.18f, 0.18f, 0.18f},
            {0.98f, 0.75f, 0.20f}, {1.00f, 0.90f, 0.15f}, 0.20f, 0.40f});

        blueprints.push_back(std::move(b));
    }

    return blueprints;
}

// Singleton state for the City Builder
struct CityBuilderState {
    flecs::entity                     cameraEntity;
    flecs::entity                     aimEntity;
    flecs::entity                     floorEntity;
    flecs::entity                     solidPipeline;
    flecs::entity                     ghostPipeline;
    std::vector<Blueprint>            blueprints;
    uint32_t                          currentBlueprint{0};
    float                             scrollAccumulator{0.0f};
    float                             rotateScrollAccumulator{0.0f};
    float                             userYawRadians{0.0f};
    std::vector<VisualPartInstance>   ghostParts;
    bool                              ghostVisible{false};
    bool                              ghostSnapped{false};
    bool                              ghostBlocked{false};
    glm::vec3                         ghostBasePos{0.0f};
    float                             ghostYawRadians{0.0f};
    glm::quat                         ghostRotation{1.0f, 0.0f, 0.0f, 0.0f};
    flecs::entity                     hoveredPlacedEntity;
    flecs::entity                     clickedSelectedEntity;
};

void rebuildGhostPreview(flecs::world& world, CityBuilderState& state) {
    for (auto& part : state.ghostParts) {
        destroyVisualPart(part);
    }
    state.ghostParts.clear();

    const Blueprint& bp = state.blueprints[state.currentBlueprint];
    state.ghostParts.reserve(bp.parts.size());
    for (const BlueprintPart& part : bp.parts) {
        const Lu::Component::Material mat = state.ghostBlocked
            ? makeRedGhostMaterial()
            : makeGhostMaterial(part.baseColor);
        state.ghostParts.push_back(spawnVisualPart(
            world,
            glm::vec3(0.0f, -1000.0f, 0.0f),
            glm::quat(1.0f, 0.0f, 0.0f, 0.0f),
            glm::vec3(0.0f),
            part.meshEntity,
            state.ghostPipeline,
            mat,
            part.baseColor,
            part.vibrantColor,
            part.roughness,
            part.metallic));
    }
}

void applyPlacedObjectTheme(PlacedObject& placed, bool vibrant) {
    if (placed.isVibrant == vibrant) return;
    placed.isVibrant = vibrant;
    for (auto& part : placed.visualParts) {
        if (!part.meshEntity.is_alive()) continue;
        const Lu::Component::Material mat = vibrant
            ? makeVibrantMaterial(part.vibrantColor, part.roughness * 0.6f, std::min(1.0f, part.metallic + 0.15f))
            : makeNormalMaterial(part.baseColor, part.roughness, part.metallic);
        part.meshEntity.set<Lu::Component::Material>(mat);
    }
}

// Spawn a placed opaque building/street surrounded by a static Jolt physics box for raycasting & overlap checks
flecs::entity placeBuildingOrStreet(
    flecs::world& world,
    CityBuilderState& state,
    uint32_t blueprintIndex,
    const glm::vec3& basePos,
    float yawRadians)
{
    const Blueprint& bp = state.blueprints[blueprintIndex];
    const glm::quat rotation  = glm::angleAxis(yawRadians, glm::vec3(0.0f, 1.0f, 0.0f));
    const glm::vec3 boxCenter = basePos + glm::vec3(0.0f, bp.boundingHalfExtents.y, 0.0f);

    Transform boxTransform{};
    boxTransform.position = boxCenter;
    boxTransform.rotation = rotation;
    boxTransform.scale    = glm::vec3(1.0f);

    // Surrounding Jolt physics cube for raycasting and overlap detection
    flecs::entity buildingEntity = world.entity()
        .set<Transform>(boxTransform)
        .set<RigidBodyDesc>({
            .shape       = ShapeKind::Box,
            .kind        = BodyKind::Static,
            .halfExtents = bp.boundingHalfExtents,
            .friction    = 0.75f,
            .restitution = 0.10f,
            .layer       = Lca::Layers::STATIC_ENVIRONMENT
        });

    PlacedObject placed{};
    placed.blueprintIndex = blueprintIndex;
    placed.basePosition   = basePos;
    placed.yaw            = yawRadians;
    placed.rotation       = rotation;
    placed.boxCenter      = boxCenter;
    placed.boxHalfExtents = bp.boundingHalfExtents;
    placed.isVibrant      = false;
    placed.visualParts.reserve(bp.parts.size());

    for (const BlueprintPart& part : bp.parts) {
        const glm::vec3 worldPos = basePos + rotation * part.localOffset;
        const glm::quat worldRot = rotation * part.localRotation;
        const Lu::Component::Material mat = makeNormalMaterial(part.baseColor, part.roughness, part.metallic);
        placed.visualParts.push_back(spawnVisualPart(
            world,
            worldPos,
            worldRot,
            part.scale,
            part.meshEntity,
            state.solidPipeline,
            mat,
            part.baseColor,
            part.vibrantColor,
            part.roughness,
            part.metallic));
    }

    buildingEntity.set<PlacedObject>(std::move(placed));
    return buildingEntity;
}

// Destroy a placed object and collapse it into physics-based primitive shapes that disappear after 4 seconds
void collapsePlacedObjectIntoPhysicsShapes(
    flecs::world& world,
    CityBuilderState& state,
    flecs::entity targetEntity)
{
    if (!targetEntity.is_alive() || !targetEntity.has<PlacedObject>()) return;

    PlacedObject placed = componentRef(targetEntity.get<PlacedObject>());
    const Blueprint& bp = state.blueprints[placed.blueprintIndex];

    // 1. Destroy the static visual parts and the placed entity (which removes its surrounding static Jolt cube)
    for (auto& part : placed.visualParts) {
        destroyVisualPart(part);
    }
    targetEntity.destruct();

    // 2. Spawn each constituent primitive shape as an independent dynamic Jolt physics body
    uint32_t partIdx = 0;
    for (const BlueprintPart& part : bp.parts) {
        const glm::vec3 worldPos = placed.basePosition + placed.rotation * part.localOffset;
        const glm::quat worldRot = placed.rotation * part.localRotation;

        // Outward + slight upward impulse so the collapsing structure tumbles apart naturally
        glm::vec3 radial = worldPos - (placed.basePosition + glm::vec3(0.0f, 0.3f, 0.0f));
        if (glm::length(radial) < 0.05f) {
            radial = glm::vec3(0.0f, 1.0f, 0.0f);
        } else {
            radial = glm::normalize(radial);
        }
        const float jitter = static_cast<float>((partIdx * 37u) % 11u) * 0.12f;
        const glm::vec3 initialVelocity = radial * (1.6f + jitter) + glm::vec3(0.0f, 2.2f + jitter * 0.5f, 0.0f);

        // Jolt convex radius requires halfExtents >= 0.06f on every axis
        const glm::vec3 colliderHalfExtents = glm::max(part.scale, glm::vec3(0.08f));

        Transform debrisTransform{};
        debrisTransform.position = worldPos;
        debrisTransform.rotation = worldRot;
        debrisTransform.scale    = part.scale;

        flecs::entity debrisRoot = world.entity()
            .set<Transform>(debrisTransform)
            .set<RigidBodyDesc>({
                .shape          = part.physicsShape,
                .kind           = BodyKind::Dynamic,
                .halfExtents    = colliderHalfExtents,
                .mass           = 2.0f,
                .friction       = 0.65f,
                .restitution    = 0.25f,
                .layer          = Lca::Layers::DYNAMIC_ENVIRONMENT,
                .linearVelocity = initialVelocity
            })
            .add<Lu::Component::TransformGpu>();

        // Vibrant-tinted material on collapsing pieces so the destruction feels energetic
        const Lu::Component::Material mat = makeNormalMaterial(part.baseColor, part.roughness, part.metallic);
        flecs::entity meshChild = world.entity()
            .set(flecs::Parent{debrisRoot})
            .set<Lu::Component::Mesh>({.mesh = part.meshEntity, .pipeline = state.solidPipeline})
            .set<Lu::Component::Material>(mat);

        debrisRoot.set<DebrisPiece>({
            .meshChild        = meshChild,
            .remainingSeconds = kDebrisLifetime
        });
        ++partIdx;
    }
}

struct CityBuilderModule {
    explicit CityBuilderModule(flecs::world& world) {
        world.import<Lu::Module::FlyingCamera>();
        world.import<Lca::Module::PhysicsModule>();

        world.component<PlacedObject>();
        world.component<DebrisPiece>();
        world.component<CityBuilderState>().add(flecs::Singleton);

        const auto cubeMesh      = world.lookup("Mesh::UnitCube");
        const auto solidPipeline = world.lookup("MeshPipe::CitySolid");
        const auto ghostPipeline = world.lookup("MeshPipe::CityGhost");

        // Flying camera overlooking the green city floor
        constexpr glm::vec3 kInitialCamPos(0.0f, 16.0f, 26.0f);
        constexpr glm::vec3 kInitialLookTarget(0.0f, 1.0f, 0.0f);
        const auto cameraEntity = world.entity("CityFlyingCamera")
            .set(FlyingCamera{
                .position      = kInitialCamPos,
                .angle         = FlyingCamera::lookAt(kInitialCamPos, kInitialLookTarget),
                .speed         = 14.0f,
                .rotationSpeed = 0.005f,
                .fov           = glm::radians(60.0f),
                .nearClip      = 0.1f,
                .farClip       = 350.0f,
                .direction     = glm::normalize(kInitialLookTarget - kInitialCamPos)
            })
            .add<Transform>();

        // Persistent Jolt raycast entity for cursor picking & placement
        const auto aimEntity = world.entity("CityCursorAim")
            .set<Transform>({})
            .set<PhysicsRay>({})
            .set<PhysicsRayHit>({})
            .add<PhysicsRayCast>();

        // Big green Jolt physics cube and green cube mesh as the floor (top surface at y = 0)
        Transform floorTransform{};
        floorTransform.position = glm::vec3(0.0f, -kFloorHalfHeight, 0.0f);
        floorTransform.scale    = glm::vec3(kFloorHalfWidth, kFloorHalfHeight, kFloorHalfWidth);
        const auto floorEntity = world.entity("CityGreenFloor")
            .set<Transform>(floorTransform)
            .set<RigidBodyDesc>({
                .shape       = ShapeKind::Box,
                .kind        = BodyKind::Static,
                .halfExtents = glm::vec3(kFloorHalfWidth, kFloorHalfHeight, kFloorHalfWidth),
                .friction    = 0.85f,
                .restitution = 0.05f,
                .layer       = Lca::Layers::STATIC_ENVIRONMENT
            })
            .add<Lu::Component::TransformGpu>();

        world.entity("CityGreenFloorMesh")
            .set(flecs::Parent{floorEntity})
            .set<Lu::Component::Mesh>({.mesh = cubeMesh, .pipeline = solidPipeline})
            .set<Lu::Component::Material>(makeNormalMaterial(glm::vec3(0.24f, 0.58f, 0.26f), 0.78f, 0.02f));

        // Warm directional sunlight
        Transform sunTransform{};
        sunTransform.position = glm::vec3(30.0f, 60.0f, 30.0f);
        sunTransform.rotation = glm::angleAxis((0.85f - 3.14159f) * 0.5f, glm::vec3(1.0f, 0.25f, 0.0f));
        const auto sunRoot = world.entity("CitySunTransform")
            .set<Transform>(sunTransform)
            .add<Lu::Component::TransformGpu>();
        world.entity("CitySun")
            .set(flecs::Parent{sunRoot})
            .set<Lu::Component::Light>({
                .type      = Lu::LightType::Directional,
                .color     = glm::vec3(1.0f, 0.96f, 0.88f),
                .intensity = 2.2f,
                .radius    = 300.0f
            });

        // Initialize CityBuilderState singleton and build initial ghost preview
        CityBuilderState initialState{};
        initialState.cameraEntity     = cameraEntity;
        initialState.aimEntity        = aimEntity;
        initialState.floorEntity      = floorEntity;
        initialState.solidPipeline    = solidPipeline;
        initialState.ghostPipeline    = ghostPipeline;
        initialState.blueprints       = createCityBlueprints(world);
        initialState.currentBlueprint = 0;
        rebuildGhostPreview(world, initialState);

        // Place a small starter town center so the user immediately sees houses & streets
        placeBuildingOrStreet(world, initialState, 3, glm::vec3( 0.0f,  kFloorTopY,  0.0f), 0.0f);
        placeBuildingOrStreet(world, initialState, 4, glm::vec3( 0.0f,  kFloorTopY, -4.05f), 0.0f);
        placeBuildingOrStreet(world, initialState, 0, glm::vec3(-3.60f, kFloorTopY,  0.0f), glm::radians(90.0f));
        placeBuildingOrStreet(world, initialState, 1, glm::vec3( 3.80f, kFloorTopY,  0.0f), glm::radians(-90.0f));

        world.set<CityBuilderState>(std::move(initialState));

        // ---------------------------------------------------------------------
        // System 1 (PreUpdate): Build Jolt PhysicsRay under the mouse cursor
        // ---------------------------------------------------------------------
        world.system<PhysicsRay, const InputState>("BuildCityCursorRay")
            .with<PhysicsRayCast>()
            .kind(flecs::PreUpdate)
            .each([cameraEntity](PhysicsRay& ray, const InputState& input) {
                const float width  = static_cast<float>(Lu::Core::windowWidth);
                const float height = static_cast<float>(Lu::Core::windowHeight);
                if (width < 1.0f || height < 1.0f) return;

                const FlyingCamera& camera = componentRef(cameraEntity.get<FlyingCamera>());
                const glm::vec3 forward = glm::normalize(camera.direction);
                const glm::vec3 right   = glm::normalize(glm::cross(forward, glm::vec3(0.0f, 1.0f, 0.0f)));
                const glm::vec3 up      = glm::cross(right, forward);

                const float ndcX    = 2.0f * input.cursor.position.x / width - 1.0f;
                const float ndcY    = 1.0f - 2.0f * input.cursor.position.y / height;
                const float tangent = std::tan(camera.fov * 0.5f);

                ray.origin      = camera.position;
                ray.direction   = glm::normalize(
                    forward
                    + right * (ndcX * (width / height) * tangent)
                    + up * (ndcY * tangent));
                ray.maxDistance = camera.farClip;
            });

        // ---------------------------------------------------------------------
        // System 2 (OnUpdate): R+Scroll rotation, scroll blueprint switching,
        //                      smart side snapping + 90-deg alignment, overlap check,
        //                      vibrant selection theme, placement, and X destruction
        // ---------------------------------------------------------------------
        auto placedQuery = world.query<PlacedObject>();

        world.system<CityBuilderState, const InputState>("UpdateCityBuilder")
            .each([placedQuery](flecs::iter& it, size_t, CityBuilderState& state, const InputState& input) {
                flecs::world world = it.world();
                const uint32_t numBlueprints = static_cast<uint32_t>(state.blueprints.size());
                if (numBlueprints == 0) return;

                const bool holdingR = input.isKeyDown(Lu::KeyCode::keyR);

                // 1. Handle Scroll Input:
                //    - While holding 'R': rotate ghost around Y axis (blueprint switching via scroll is deactivated!)
                //    - When 'R' is NOT held: switch ghost blueprint via scroll
                bool switchedBlueprint = false;
                if (holdingR) {
                    state.scrollAccumulator = 0.0f; // Ensure blueprint switching is deactivated while holding R
                    if (std::abs(input.scroll.deltaScroll.y) > 0.005f) {
                        if (state.ghostSnapped) {
                            // When snapped beside a placed object (where orientation is aligned in 90-deg steps),
                            // step userYawRadians by 90 degrees per scroll notch so rotating while snapped is effortless
                            state.rotateScrollAccumulator += input.scroll.deltaScroll.y;
                            constexpr float kSnapRotateThreshold = 0.65f;
                            if (state.rotateScrollAccumulator >= kSnapRotateThreshold) {
                                state.userYawRadians = wrapAnglePi(state.userYawRadians + glm::half_pi<float>());
                                state.rotateScrollAccumulator = 0.0f;
                            } else if (state.rotateScrollAccumulator <= -kSnapRotateThreshold) {
                                state.userYawRadians = wrapAnglePi(state.userYawRadians - glm::half_pi<float>());
                                state.rotateScrollAccumulator = 0.0f;
                            }
                        } else {
                            // In free space, rotate smoothly around the Y axis
                            state.rotateScrollAccumulator = 0.0f;
                            state.userYawRadians = wrapAnglePi(
                                state.userYawRadians + input.scroll.deltaScroll.y * glm::radians(15.0f));
                        }
                    }
                } else {
                    state.rotateScrollAccumulator = 0.0f;
                    if (std::abs(input.scroll.deltaScroll.y) > 0.01f) {
                        state.scrollAccumulator += input.scroll.deltaScroll.y;
                        if (state.scrollAccumulator >= 0.85f) {
                            state.currentBlueprint = (state.currentBlueprint + 1) % numBlueprints;
                            state.scrollAccumulator = 0.0f;
                            switchedBlueprint = true;
                        } else if (state.scrollAccumulator <= -0.85f) {
                            state.currentBlueprint = (state.currentBlueprint + numBlueprints - 1) % numBlueprints;
                            state.scrollAccumulator = 0.0f;
                            switchedBlueprint = true;
                        }
                    }
                }

                // Number keys 1..6 also switch blueprints when R is not held
                if (!holdingR) {
                    const Lu::KeyCode numberKeys[6] = {
                        Lu::KeyCode::key1, Lu::KeyCode::key2, Lu::KeyCode::key3,
                        Lu::KeyCode::key4, Lu::KeyCode::key5, Lu::KeyCode::key6
                    };
                    for (uint32_t k = 0; k < std::min<uint32_t>(6u, numBlueprints); ++k) {
                        if (input.wasKeyPressedThisFrame(numberKeys[k]) && state.currentBlueprint != k) {
                            state.currentBlueprint = k;
                            switchedBlueprint = true;
                        }
                    }
                }
                if (switchedBlueprint) {
                    rebuildGhostPreview(world, state);
                }

                // 2. Inspect Jolt PhysicsRayHit under cursor
                const PhysicsRayHit& hit = componentRef(state.aimEntity.get<PhysicsRayHit>());

                flecs::entity hitEntity = (hit.hit && hit.entity != 0)
                    ? flecs::entity(world, hit.entity)
                    : flecs::entity();

                const bool hitPlacedObject = hitEntity.is_alive() && hitEntity.has<PlacedObject>();
                state.hoveredPlacedEntity = hitPlacedObject ? hitEntity : flecs::entity();

                if (!hit.hit) {
                    for (auto& part : state.ghostParts) {
                        if (part.transformEntity.is_alive()) {
                            Transform t{};
                            t.position = glm::vec3(0.0f, -1000.0f, 0.0f);
                            t.scale    = glm::vec3(0.0f);
                            part.transformEntity.set<Transform>(t);
                        }
                    }
                    state.ghostVisible = false;
                    state.ghostSnapped = false;
                } else {
                    state.ghostVisible = true;
                    const Blueprint& bp = state.blueprints[state.currentBlueprint];
                    const glm::vec3 ghostHalfExtents = bp.boundingHalfExtents;

                    // Start with the cursor's raw position and the user's manual Y-axis rotation
                    const float baseY = hitPlacedObject ? std::max(kFloorTopY, hit.point.y) : kFloorTopY;
                    const glm::vec3 rawBasePos(hit.point.x, baseY, hit.point.z);

                    glm::vec3 chosenBasePos = rawBasePos;
                    float     chosenYaw     = state.userYawRadians;
                    glm::quat chosenRot     = glm::angleAxis(chosenYaw, glm::vec3(0.0f, 1.0f, 0.0f));
                    bool      didSnap       = false;

                    // Helper to check if a candidate ghost box (at ground level) overlaps any placed object's box
                    auto overlapsAnyPlaced = [&](const glm::vec3& candidateBasePos, const glm::quat& candidateRot) -> bool {
                        const glm::vec3 candBoxCenter = glm::vec3(
                            candidateBasePos.x,
                            kFloorTopY + ghostHalfExtents.y,
                            candidateBasePos.z);
                        bool overlaps = false;
                        placedQuery.each([&](flecs::entity, PlacedObject& other) {
                            if (overlaps) return;
                            if (obbIntersectsObb(
                                    candBoxCenter, ghostHalfExtents, candidateRot,
                                    other.boxCenter, other.boxHalfExtents, other.rotation))
                            {
                                overlaps = true;
                            }
                        });
                        return overlaps;
                    };

                    // 2a. Smart Snapping & 90-Degree Alignment beside placed objects (when cursor is on the floor)
                    if (!hitPlacedObject) {
                        constexpr float kSnapClearance   = 0.05f; // Small gap between physics boxes so snapped objects don't collide
                        constexpr float kSnapMaxDistance = 1.45f; // Max horizontal distance from a side slot to trigger snapping

                        float     bestFreeDist = kSnapMaxDistance;
                        bool      foundFree    = false;
                        glm::vec3 bestFreePos  = rawBasePos;
                        float     bestFreeYaw  = chosenYaw;

                        placedQuery.each([&](flecs::entity, PlacedObject& placed) {
                            // Find the 90-degree Y-axis rotation relative to `placed` that is nearest to `state.userYawRadians`
                            int   bestK        = 0;
                            float bestAngleErr = 1e9f;
                            for (int k = 0; k < 4; ++k) {
                                const float candYaw = placed.yaw + static_cast<float>(k) * glm::half_pi<float>();
                                const float err     = std::abs(wrapAnglePi(state.userYawRadians - candYaw));
                                if (err < bestAngleErr) {
                                    bestAngleErr = err;
                                    bestK        = k;
                                }
                            }

                            const float alignedYaw = wrapAnglePi(placed.yaw + static_cast<float>(bestK) * glm::half_pi<float>());
                            const glm::quat alignedRot = glm::angleAxis(alignedYaw, glm::vec3(0.0f, 1.0f, 0.0f));

                            // Effective half-extents of the ghost in `placed`'s local X and Z axes
                            const float effGhostHalfX = (bestK % 2 == 0) ? ghostHalfExtents.x : ghostHalfExtents.z;
                            const float effGhostHalfZ = (bestK % 2 == 0) ? ghostHalfExtents.z : ghostHalfExtents.x;

                            // Do not snap if the cursor has been pushed deep inside `placed`'s border (let it overlap and turn red)
                            const glm::vec3 relRaw = rawBasePos - placed.basePosition;
                            const glm::vec3 rightAxis   = placed.rotation * glm::vec3(1.0f, 0.0f, 0.0f);
                            const glm::vec3 forwardAxis = placed.rotation * glm::vec3(0.0f, 0.0f, 1.0f);
                            const float localX = glm::dot(relRaw, rightAxis);
                            const float localZ = glm::dot(relRaw, forwardAxis);
                            if (std::abs(localX) < placed.boxHalfExtents.x + effGhostHalfX * 0.25f &&
                                std::abs(localZ) < placed.boxHalfExtents.z + effGhostHalfZ * 0.25f)
                            {
                                return;
                            }

                            // Four flush side-by-side alignment positions around `placed` (+X, -X, +Z, -Z)
                            const float distX = placed.boxHalfExtents.x + effGhostHalfX + kSnapClearance;
                            const float distZ = placed.boxHalfExtents.z + effGhostHalfZ + kSnapClearance;
                            const glm::vec3 sidePositions[4] = {
                                placed.basePosition + rightAxis * distX,
                                placed.basePosition - rightAxis * distX,
                                placed.basePosition + forwardAxis * distZ,
                                placed.basePosition - forwardAxis * distZ
                            };

                            for (const glm::vec3& candPos : sidePositions) {
                                const float dist = glm::length(glm::vec2(rawBasePos.x - candPos.x, rawBasePos.z - candPos.z));
                                if (dist < bestFreeDist && !overlapsAnyPlaced(candPos, alignedRot)) {
                                    bestFreeDist = dist;
                                    foundFree    = true;
                                    bestFreePos  = glm::vec3(candPos.x, kFloorTopY, candPos.z);
                                    bestFreeYaw  = alignedYaw;
                                }
                            }
                        });

                        if (foundFree) {
                            chosenBasePos = bestFreePos;
                            chosenYaw     = bestFreeYaw;
                            chosenRot     = glm::angleAxis(chosenYaw, glm::vec3(0.0f, 1.0f, 0.0f));
                            didSnap       = true;
                        }
                    }

                    state.ghostSnapped    = didSnap;
                    state.ghostBasePos    = chosenBasePos;
                    state.ghostYawRadians = chosenYaw;
                    state.ghostRotation   = chosenRot;

                    // 2b. Test if surrounding physics cube of ghost intersects any placed object's surrounding physics cube
                    const glm::vec3 ghostBoxCenterCurrent = state.ghostBasePos + glm::vec3(0.0f, ghostHalfExtents.y, 0.0f);
                    const glm::vec3 ghostBoxCenterGround  = glm::vec3(state.ghostBasePos.x, kFloorTopY + ghostHalfExtents.y, state.ghostBasePos.z);

                    bool intersectsPlaced = hitPlacedObject;

                    // Test 1: Jolt NarrowPhaseQuery::CollideShape with the ghost's surrounding JPH::BoxShape
                    auto& physics = Lca::Core::GetPhysics();
                    if (physics.isInitialized() && state.floorEntity.has<PhysicsBody>()) {
                        const ::JPH::BodyID floorBodyId = componentRef(state.floorEntity.get<PhysicsBody>()).id;
                        const glm::vec3 inflatedHalf = ghostHalfExtents + glm::vec3(0.01f);
                        ::JPH::BoxShape ghostJoltBox(Lca::Core::toJoltVec3(inflatedHalf));
                        ::JPH::CollideShapeSettings settings;
                        ::JPH::AllHitCollisionCollector<::JPH::CollideShapeCollector> collector;
                        IgnoreFloorBodyFilter bodyFilter(floorBodyId);

                        const ::JPH::RMat44 comTransform = ::JPH::RMat44::sRotationTranslation(
                            Lca::Core::toJoltQuat(state.ghostRotation),
                            ::JPH::RVec3(ghostBoxCenterGround.x, ghostBoxCenterGround.y, ghostBoxCenterGround.z));

                        physics.getPhysicsSystem().GetNarrowPhaseQuery().CollideShape(
                            &ghostJoltBox,
                            ::JPH::Vec3::sReplicate(1.0f),
                            comTransform,
                            settings,
                            ::JPH::RVec3::sZero(),
                            collector,
                            {},
                            {},
                            bodyFilter);

                        if (collector.HadHit()) {
                            intersectsPlaced = true;
                        }
                    }

                    // Test 2: Exact 3D OBB vs OBB against all placed objects' surrounding physics boxes
                    placedQuery.each([&](flecs::entity, PlacedObject& placed) {
                        if (intersectsPlaced) return;
                        if (obbIntersectsObb(
                                ghostBoxCenterGround, ghostHalfExtents, state.ghostRotation,
                                placed.boxCenter, placed.boxHalfExtents, placed.rotation) ||
                            obbIntersectsObb(
                                ghostBoxCenterCurrent, ghostHalfExtents + glm::vec3(0.0f, 0.05f, 0.0f), state.ghostRotation,
                                placed.boxCenter, placed.boxHalfExtents, placed.rotation))
                        {
                            intersectsPlaced = true;
                        }
                    });

                    // Update ghost materials if blocked state changed (red when intersecting, normal ghost when valid)
                    if (intersectsPlaced != state.ghostBlocked) {
                        state.ghostBlocked = intersectsPlaced;
                        for (size_t i = 0; i < state.ghostParts.size() && i < bp.parts.size(); ++i) {
                            if (!state.ghostParts[i].meshEntity.is_alive()) continue;
                            const Lu::Component::Material mat = state.ghostBlocked
                                ? makeRedGhostMaterial()
                                : makeGhostMaterial(bp.parts[i].baseColor);
                            state.ghostParts[i].meshEntity.set<Lu::Component::Material>(mat);
                        }
                    }

                    // Update transforms of all ghost primitive parts
                    for (size_t i = 0; i < state.ghostParts.size() && i < bp.parts.size(); ++i) {
                        if (!state.ghostParts[i].transformEntity.is_alive()) continue;
                        const BlueprintPart& part = bp.parts[i];
                        Transform t{};
                        t.position = state.ghostBasePos + state.ghostRotation * part.localOffset;
                        t.rotation = state.ghostRotation * part.localRotation;
                        t.scale    = part.scale;
                        state.ghostParts[i].transformEntity.set<Transform>(t);
                    }
                }

                // 3. Handle Left Click: Select an existing placed object OR place a new object if valid
                if (input.wasMouseButtonPressedThisFrame(Lu::MouseButton::mouseLeft)) {
                    if (hitPlacedObject) {
                        // Clicking a placed object selects it (toggle if already click-selected)
                        if (state.clickedSelectedEntity == hitEntity) {
                            state.clickedSelectedEntity = flecs::entity();
                        } else {
                            state.clickedSelectedEntity = hitEntity;
                        }
                    } else if (state.ghostVisible && !state.ghostBlocked) {
                        // Valid placement on the green floor! Keep the placed orientation as the new default user yaw
                        state.clickedSelectedEntity = flecs::entity();
                        state.userYawRadians        = state.ghostYawRadians;
                        placeBuildingOrStreet(
                            world,
                            state,
                            state.currentBlueprint,
                            state.ghostBasePos,
                            state.ghostYawRadians
                        );
                    }
                }

                // Determine active selected placed object (hovered placed object OR click-selected placed object)
                flecs::entity activeSelected = state.hoveredPlacedEntity.is_alive()
                    ? state.hoveredPlacedEntity
                    : (state.clickedSelectedEntity.is_alive() ? state.clickedSelectedEntity : flecs::entity());

                // 4. Update Vibrant Theme on all placed objects (vibrant when selected, normal otherwise)
                placedQuery.each([&](flecs::entity entity, PlacedObject& placed) {
                    const bool isSelected = (entity == state.hoveredPlacedEntity) || (entity == state.clickedSelectedEntity);
                    applyPlacedObjectTheme(placed, isSelected);
                });

                // 5. Handle 'X' Key: Destroy selected placed object into physics-based primitive shapes
                if (input.wasKeyPressedThisFrame(Lu::KeyCode::keyX) && activeSelected.is_alive()) {
                    if (state.hoveredPlacedEntity == activeSelected) {
                        state.hoveredPlacedEntity = flecs::entity();
                    }
                    if (state.clickedSelectedEntity == activeSelected) {
                        state.clickedSelectedEntity = flecs::entity();
                    }
                    collapsePlacedObjectIntoPhysicsShapes(world, state, activeSelected);
                }
            });

        // ---------------------------------------------------------------------
        // System 3 (OnUpdate): Count down 4-second lifetime on collapsed physics debris
        // ---------------------------------------------------------------------
        world.system<DebrisPiece>("UpdateCollapsedDebris")
            .each([](flecs::iter& it, size_t i, DebrisPiece& debris) {
                debris.remainingSeconds -= it.delta_time();
                if (debris.remainingSeconds <= 0.0f) {
                    if (debris.meshChild.is_alive()) {
                        debris.meshChild.remove<Lu::Component::Mesh>();
                        debris.meshChild.destruct();
                    }
                    it.entity(i).destruct();
                }
            });
    }
};

} // namespace

int main() {
    Lu::App("LuEngine City Builder (Example 15)", true)
        .registerMeshPipe(Lu::GraphicsPipelineConfig{
            .name           = "MeshPipe::CitySolid",
            .capacity       = 8192,
            .vertexShader   = "shader/mesh_basic.vert.spv",
            .fragmentShader = "shader/mesh_pbr.frag.spv",
            .cullMode       = VK_CULL_MODE_NONE
        })
        .registerMeshPipe(Lu::GraphicsPipelineConfig{
            .name           = "MeshPipe::CityGhost",
            .capacity       = 512,
            .vertexShader   = "shader/mesh_basic.vert.spv",
            .fragmentShader = "shader/mesh_basic.frag.spv",
            .cullMode       = VK_CULL_MODE_NONE,
            .depthTestEnable = false,
            .depthWriteEnable = false,
            .depthCompareOp = VK_COMPARE_OP_LESS,
            .blendEnable    = true
        })
        .registerShape<Lu::Shape::Cube>("Mesh::UnitCube", glm::vec3(1.0f), glm::vec3(0.0f), glm::vec4(1.0f))
        .registerShape<Lu::Shape::Sphere>("Mesh::UnitSphere", 1.0f, 18, 24, glm::vec3(0.0f), glm::vec4(1.0f))
        .registerShape<Lu::Shape::GableRoof>("Mesh::UnitGableRoof", glm::vec3(1.0f), glm::vec3(0.0f), glm::vec4(1.0f))
        .registerShape<Lu::Shape::Pyramid>("Mesh::UnitPyramid", glm::vec3(1.0f), glm::vec3(0.0f), glm::vec4(1.0f))
        .registerShape<Lu::Shape::Cylinder>("Mesh::UnitCylinder", 1.0f, 1.0f, 24, glm::vec3(0.0f), glm::vec4(1.0f))
        .registerShape<Lu::Shape::Cone>("Mesh::UnitCone", 1.0f, 1.0f, 24, glm::vec3(0.0f), glm::vec4(1.0f))
        .importModule<CityBuilderModule>()
        .run();
}
