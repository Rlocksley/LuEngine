#include "App.hpp"
#include "Shape.hpp"
#include "FlyingCamera.hpp"
#include "component/Light.hpp"
#include "component/Material.hpp"
#include "component/Mesh.hpp"
#include "component/Transform.hpp"
#include "component/TransformGpu.hpp"

#include <algorithm>
#include <cmath>

// =============================================================================
// main10: Cube Placer (primitive strategy-game building mode)
//
//   C           toggle placement mode (a transparent ghost cube appears)
//   Left mouse  place a solid cube where the ghost is
//   WASD + hold right mouse button: FlyingCamera
//
// Everything (ground + placed cubes) is a unit cube. Each cube is a
// "Transform entity" (Transform + TransformGpu) with one child "mesh entity"
// (Mesh + Material + MeshSphere). The ray cast runs inside the ECS: a system
// iterates every MeshSphere entity, fetches the Transform of its parent
// (term .up()), tests the ray against the bounding sphere, and then refines
// the nearest candidates with a slab test to find the hit face.
// =============================================================================

namespace {

using Lu::Component::InputState;
using Lu::Component::Transform;

constexpr float kCubeHalf   = 0.5f;
constexpr float kCubeRadius = 0.8660254f; // sqrt(3) * half edge: sphere around a unit cube
constexpr int   kGroundHalf = 10;         // ground is (2*kGroundHalf)^2 cubes

// ----------------------------------------------------------------------------
// Components
// ----------------------------------------------------------------------------

// Bounding sphere radius (local space) of a mesh entity. Its center is the
// position of the parent's Transform.
struct MeshSphere {
    float radius{kCubeRadius};
};

// Marks the Transform entity of the transparent preview cube.
struct GhostTag {};

// Singleton: placement mode + the assets needed to spawn cubes.
struct PlacerState {
    bool active{false};
    flecs::entity cubeMesh;
    flecs::entity solidPipe;
};

// Singleton: mouse ray in world space (built from camera + cursor).
struct PlacerRay {
    glm::vec3 origin{0.0f};
    glm::vec3 direction{0.0f, 0.0f, -1.0f};
    bool valid{false};
};

// Singleton: nearest hit of the ray.
struct PlacerHit {
    bool hit{false};
    flecs::entity mesh{};
    float t{0.0f};
    glm::vec3 cellCenter{0.0f}; // center of the cube that was hit
    glm::vec3 normal{0.0f};     // axis aligned face normal that was hit
};

// ----------------------------------------------------------------------------
// Helper
// ----------------------------------------------------------------------------

flecs::entity spawnCube(const flecs::world& world, const glm::vec3& position,
    float scale, const flecs::entity& mesh, const flecs::entity& pipeline,
    const Lu::Component::Material& material, bool pickable) {

    Transform transform{};
    transform.position = position;
    transform.scale = glm::vec3(scale);

    const flecs::entity transformEntity = world.entity()
        .set<Transform>(transform)
        .add<Lu::Component::TransformGpu>();

    const flecs::entity meshEntity = world.entity()
        .set(flecs::Parent{transformEntity})
        .set(Lu::Component::Mesh{ .mesh = mesh, .pipeline = pipeline })
        .set(material);

    if (pickable) {
        meshEntity.set<MeshSphere>({kCubeRadius});
    }
    return transformEntity;
}

Lu::Component::Material solidMaterial(const glm::vec3& color) {
    Lu::Component::Material material{};
    material.albedo = glm::vec4(color, 1.0f);
    material.ambient = glm::vec4(0.32f, 0.32f, 0.34f, 1.0f);
    material.roughness = 0.8f;
    return material;
}

// ----------------------------------------------------------------------------
// Module
// ----------------------------------------------------------------------------

struct CubePlacerModule {
    explicit CubePlacerModule(flecs::world& world) {
        world.import<Lu::Module::FlyingCamera>();

        world.component<MeshSphere>();
        world.component<GhostTag>();
        world.component<PlacerState>().add(flecs::Singleton);
        world.component<PlacerRay>().add(flecs::Singleton);
        world.component<PlacerHit>().add(flecs::Singleton);

        const flecs::entity cubeMesh = world.lookup("Mesh::Cube");
        const flecs::entity solidPipe = world.lookup("MeshPipe::Solid");
        const flecs::entity ghostPipe = world.lookup("MeshPipe::Ghost");

        world.set<PlacerState>({ false, cubeMesh, solidPipe });
        world.set<PlacerRay>({});
        world.set<PlacerHit>({});

        // --- camera -----------------------------------------------------------
        constexpr glm::vec3 cameraPosition(0.0f, 12.0f, 15.0f);
        world.entity("PlacerCamera")
            .set(Lu::Component::FlyingCamera{
                .position = cameraPosition,
                .angle = Lu::Component::FlyingCamera::lookAt(
                    cameraPosition, glm::vec3(0.0f, 0.0f, 0.0f)),
                .speed = 8.0f,
                .rotationSpeed = 0.005f,
                .fov = glm::radians(60.0f),
                .nearClip = 0.1f,
                .farClip = 200.0f
            })
            .add<Transform>();

        // --- light ------------------------------------------------------------
        {
            Transform lightTransform{};
            lightTransform.position = glm::vec3(0.0f, 50.0f, 0.0f);
            lightTransform.rotation = glm::angleAxis((0.3f - 3.14f) / 2.f, glm::vec3(1.f, 0.f, 0.f));

            const flecs::entity lightEntity = world.entity("PlacerSunTransform")
                .set<Transform>(lightTransform)
                .add<Lu::Component::TransformGpu>();

            world.entity("PlacerSun")
                .child_of(lightEntity)
                .set<Lu::Component::Light>({
                    .type = Lu::LightType::Directional,
                    .color = glm::vec3(1.0f, 0.95f, 0.85f),
                    .intensity = 0.9f,
                    .radius = 200.0f
                });
        }

        // --- cube ground (checkerboard, top surface at y = 0) ------------------
        for (int x = -kGroundHalf; x < kGroundHalf; ++x) {
            for (int z = -kGroundHalf; z < kGroundHalf; ++z) {
                const bool even = ((x + z) & 1) == 0;
                const glm::vec3 color = even ? glm::vec3(0.36f, 0.62f, 0.32f)
                                             : glm::vec3(0.30f, 0.54f, 0.27f);
                spawnCube(world,
                    glm::vec3(x + 0.5f, -0.5f, z + 0.5f), 1.0f,
                    cubeMesh, solidPipe, solidMaterial(color), true);
            }
        }

        // --- ghost cube (transparent, NOT pickable => no MeshSphere) ------------
        // Hidden by scale 0 until placement mode is on and the ray hits something.
        {
            Lu::Component::Material ghostMaterial{};
            ghostMaterial.albedo = glm::vec4(0.25f, 0.8f, 1.0f, 0.45f);
            ghostMaterial.ambient = glm::vec4(0.45f, 0.45f, 0.45f, 1.0f);
            ghostMaterial.emission = glm::vec4(0.03f, 0.12f, 0.16f, 0.0f);
            ghostMaterial.roughness = 0.5f;

            spawnCube(world, glm::vec3(0.0f), 0.0f,
                cubeMesh, ghostPipe, ghostMaterial, false)
                .add<GhostTag>();
        }

        // --- 1. toggle placement mode with C ---------------------------------------
        world.system<PlacerState, const InputState>("CubePlacer Toggle")
            .each([](PlacerState& state, const InputState& input) {
                if (input.wasKeyPressedThisFrame(Lu::KeyCode::keyC)) {
                    state.active = !state.active;
                }
            });

        // --- 2. build the world space mouse ray + reset the hit --------------------
        world.system<PlacerRay, PlacerHit, const PlacerState,
                const Lu::Component::FlyingCamera, const InputState>("CubePlacer Build Ray")
            .each([](PlacerRay& ray, PlacerHit& hit, const PlacerState& state,
                    const Lu::Component::FlyingCamera& camera, const InputState& input) {
                ray.valid = false;
                hit = PlacerHit{};

                const float width = static_cast<float>(Lu::Core::windowWidth);
                const float height = static_cast<float>(Lu::Core::windowHeight);
                if (!state.active || width < 1.0f || height < 1.0f
                    || glm::dot(camera.direction, camera.direction) < 1e-6f) {
                    return;
                }

                // Camera basis (same convention as FlyingCamera / CameraPackage).
                const glm::vec3 forward = glm::normalize(camera.direction);
                const glm::vec3 right = glm::normalize(glm::cross(forward, glm::vec3(0.0f, 1.0f, 0.0f)));
                const glm::vec3 up = glm::cross(right, forward);

                // Cursor -> NDC (x right, y up).
                const float ndcX = 2.0f * input.cursor.position.x / width - 1.0f;
                const float ndcY = 1.0f - 2.0f * input.cursor.position.y / height;
                const float tanHalfFov = std::tan(camera.fov * 0.5f);
                const float aspect = width / height;

                ray.origin = camera.position;
                ray.direction = glm::normalize(
                    forward
                    + right * (ndcX * aspect * tanHalfFov)
                    + up * (ndcY * tanHalfFov));
                ray.valid = true;
            });

        // --- 3. ray cast: MeshSphere of the mesh entity + Transform of its parent ----
        world.system<const MeshSphere, const Transform, const PlacerRay, PlacerHit>("CubePlacer Ray Cast")
            .with<Lu::Component::Mesh>()
            .term_at(1).up() // Transform is taken from the parent of the mesh entity
            .each([](flecs::entity mesh, const MeshSphere& sphere, const Transform& parent,
                    const PlacerRay& ray, PlacerHit& hit) {
                if (!ray.valid) {
                    return;
                }

                // Broad phase: ray vs bounding sphere.
                const float maxScale = std::max({parent.scale.x, parent.scale.y, parent.scale.z});
                const float radius = sphere.radius * maxScale;
                const glm::vec3 toOrigin = ray.origin - parent.position;
                const float b = glm::dot(toOrigin, ray.direction);
                const float c = glm::dot(toOrigin, toOrigin) - radius * radius;
                const float discriminant = b * b - c;
                if (discriminant < 0.0f) {
                    return;
                }
                const float root = std::sqrt(discriminant);
                if (-b + root < 0.0f) {
                    return; // sphere is behind the ray
                }
                if (hit.hit && -b - root > hit.t) {
                    return; // sphere starts behind the current nearest hit
                }

                // Narrow phase: slab test in the local space of the parent
                // Transform to get the entered face of the unit cube.
                const glm::mat4 inverse = glm::inverse(parent.getMatrix());
                const glm::vec3 localOrigin = glm::vec3(inverse * glm::vec4(ray.origin, 1.0f));
                const glm::vec3 localDirection = glm::vec3(inverse * glm::vec4(ray.direction, 0.0f));

                float tEnter = -1e30f;
                float tExit = 1e30f;
                int axis = 0;
                float sign = 1.0f;
                for (int i = 0; i < 3; ++i) {
                    if (std::abs(localDirection[i]) < 1e-8f) {
                        if (std::abs(localOrigin[i]) > kCubeHalf) {
                            return;
                        }
                        continue;
                    }
                    const float invDirection = 1.0f / localDirection[i];
                    float t0 = (-kCubeHalf - localOrigin[i]) * invDirection;
                    float t1 = (kCubeHalf - localOrigin[i]) * invDirection;
                    float faceSign = -1.0f;
                    if (t0 > t1) {
                        std::swap(t0, t1);
                        faceSign = 1.0f;
                    }
                    if (t0 > tEnter) {
                        tEnter = t0;
                        axis = i;
                        sign = faceSign;
                    }
                    tExit = std::min(tExit, t1);
                    if (tEnter > tExit) {
                        return;
                    }
                }
                if (tEnter < 0.0f) {
                    return;
                }

                if (!hit.hit || tEnter < hit.t) {
                    glm::vec3 localNormal(0.0f);
                    localNormal[axis] = sign;
                    hit.hit = true;
                    hit.mesh = mesh;
                    hit.t = tEnter;
                    hit.cellCenter = parent.position;
                    hit.normal = glm::round(glm::normalize(parent.rotation * localNormal));
                }
            });

        // --- 4. move / show the ghost cube ------------------------------------------
        world.system<Transform, const PlacerState, const PlacerHit>("CubePlacer Ghost")
            .with<GhostTag>()
            .each([](Transform& transform, const PlacerState& state, const PlacerHit& hit) {
                if (state.active && hit.hit) {
                    transform.position = hit.cellCenter + hit.normal; // neighbouring grid cell
                    transform.scale = glm::vec3(0.98f);               // avoids z-fighting with the hit face
                } else {
                    transform.scale = glm::vec3(0.0f);                // hidden
                }
            });

        // --- 5. left click: place a solid cube ---------------------------------------
        world.system<const PlacerState, const PlacerHit, const InputState>("CubePlacer Place")
            .each([](flecs::iter& it, size_t, const PlacerState& state,
                    const PlacerHit& hit, const InputState& input) {
                if (!state.active || !hit.hit
                    || !input.wasMouseButtonPressedThisFrame(Lu::MouseButton::mouseLeft)) {
                    return;
                }

                const glm::vec3 position = hit.cellCenter + hit.normal;
                const float shade = 0.75f + 0.05f * std::fmod(std::abs(position.y), 4.0f);
                spawnCube(it.world(), position, 1.0f,
                    state.cubeMesh, state.solidPipe,
                    solidMaterial(glm::vec3(0.85f, 0.62f, 0.38f) * shade / 0.85f), true);
            });
    }
};

} // namespace

int main() {
    Lu::App("LuEngine Cube Placer")
        // Opaque pipeline for the ground and placed cubes.
        .registerMeshPipe(Lu::GraphicsPipelineConfig{
            .name = "MeshPipe::Solid",
            .capacity = 4096,
            .vertexShader = "shader/mesh_basic.vert.spv",
            .fragmentShader = "shader/mesh_basic.frag.spv"
        })
        // Alpha blended pipeline (no depth write) for the ghost cube.
        .registerMeshPipe(Lu::GraphicsPipelineConfig{
            .name = "MeshPipe::Ghost",
            .capacity = 4,
            .vertexShader = "shader/mesh_basic.vert.spv",
            .fragmentShader = "shader/mesh_basic.frag.spv",
            .depthWriteEnable = false,
            .blendEnable = true
        })
        .registerShape<Lu::Shape::Cube>(
            "Mesh::Cube",
            glm::vec3(kCubeHalf),
            glm::vec3(0.0f),
            glm::vec4(1.0f)
        )
        .importModule<CubePlacerModule>()
        .run();
}