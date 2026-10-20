#include "App.hpp"
#include "Shape.hpp"
#include "FlyingCamera.hpp"
#include "component/Light.hpp"
#include "component/Material.hpp"
#include "component/Mesh.hpp"
#include "component/Transform.hpp"
#include "component/TransformGpu.hpp"

#include <algorithm>
#include <array>
#include <string>

namespace {

struct TiledBrickCube {
    std::vector<Lu::Vertex::Mesh> vertices;
    std::vector<uint32_t> indices;

    TiledBrickCube(const glm::vec3& halfSize, const glm::vec3& offset,
        const glm::vec4& color) {
        Lu::Shape::Cube cube(halfSize, offset, color);
        vertices = cube.getVertices();
        indices = cube.getIndices();
        for (auto& vertex : vertices) {
            vertex.texCoord *= glm::vec2(4.0f, 6.0f);
        }
    }

    const std::vector<Lu::Vertex::Mesh>& getVertices() { return vertices; }
    const std::vector<uint32_t>& getIndices() { return indices; }
};

void addBox(flecs::world& world, const std::string& name,
    const flecs::entity& boxMesh, const flecs::entity& meshPipeline,
    const flecs::entity& texture, const glm::vec3& center,
    const glm::vec3& halfExtents, const glm::vec4& color, bool useTexture = false) {
    Lu::Component::Transform transform{};
    transform.position = center;
    transform.scale = halfExtents * 2.0f;

    const flecs::entity boxTransform = world.entity((name + "_Transform").c_str())
        .set<Lu::Component::Transform>(transform)
        .add<Lu::Component::TransformGpu>();

    Lu::Component::Material material{};
    material.albedo = color;
    material.ambient = glm::vec4(0.20f, 0.18f, 0.16f, 1.0f);
    material.roughness = 0.85f;
    if (useTexture) {
        material.textures[0] = texture;
    }

    world.entity((name + "_Mesh").c_str())
        .set(flecs::Parent{boxTransform})
        .set(Lu::Component::Mesh{
            .mesh = boxMesh,
            .pipeline = meshPipeline
        })
        .set(material);
}

void addWindow(flecs::world& world, const std::string& name,
    const flecs::entity& windowMesh, const flecs::entity& meshPipeline,
    const flecs::entity& windowTexture, const glm::vec3& center,
    float width, float height, float rotationY) {
    Lu::Component::Transform transform{};
    transform.position = center;
    transform.scale = glm::vec3(width, height, 1.0f);
    transform.rotation = glm::angleAxis(
        rotationY + glm::pi<float>(), glm::vec3(0.0f, 1.0f, 0.0f));

    const flecs::entity windowTransform = world.entity((name + "_Transform").c_str())
        .set<Lu::Component::Transform>(transform)
        .add<Lu::Component::TransformGpu>();

    Lu::Component::Material material{};
    material.textures[0] = windowTexture;
    material.albedo = glm::vec4(1.0f);
    material.ambient = glm::vec4(0.35f, 0.35f, 0.35f, 1.0f);
    material.roughness = 0.75f;

    world.entity((name + "_Mesh").c_str())
        .set(flecs::Parent{windowTransform})
        .set<Lu::Component::Mesh>({
            .mesh = windowMesh,
            .pipeline = meshPipeline
        })
        .set(material);
}

void addBuilding(flecs::world& world, const std::string& name,
    const flecs::entity& cubeMesh, const flecs::entity& tiledBrickMesh,
    const flecs::entity& windowMesh,
    const flecs::entity& meshPipeline, const flecs::entity& brickTexture,
    const flecs::entity& streetTexture, const flecs::entity& windowTexture,
    float x, float z,
    float halfWidth, float halfDepth, float height,
    const glm::vec4& brickTint, bool hasRooftopRoom) {
    const glm::vec4 limestone(0.72f, 0.62f, 0.48f, 1.0f);
    const float baseY = 0.3f;
    const float frontZ = z + (z > 0.0f ? -halfDepth : halfDepth);

    addBox(world, name + "_Walls", tiledBrickMesh, meshPipeline, brickTexture,
        glm::vec3(x, baseY + height * 0.5f, z),
        glm::vec3(halfWidth, height * 0.5f, halfDepth), brickTint, true);

    addBox(world, name + "_Foundation", tiledBrickMesh, meshPipeline, streetTexture,
        glm::vec3(x, 0.2f, z),
        glm::vec3(halfWidth + 0.16f, 0.2f, halfDepth + 0.16f),
        glm::vec4(1.0f), true);

    addBox(world, name + "_BeltCourse", tiledBrickMesh, meshPipeline, streetTexture,
        glm::vec3(x, baseY + height * 0.68f, frontZ),
        glm::vec3(halfWidth + 0.03f, 0.09f, 0.09f), glm::vec4(1.0f), true);

    addBox(world, name + "_Cornice", tiledBrickMesh, meshPipeline, streetTexture,
        glm::vec3(x, baseY + height + 0.04f, z),
        glm::vec3(halfWidth + 0.22f, 0.16f, halfDepth + 0.22f),
        glm::vec4(1.0f), true);

    addBox(world, name + "_RoofCap", cubeMesh, meshPipeline, brickTexture,
        glm::vec3(x, baseY + height + 0.27f, z),
        glm::vec3(halfWidth + 0.08f, 0.08f, halfDepth + 0.08f), brickTint, true);

    for (int side = -1; side <= 1; side += 2) {
        addBox(world, name + "_Pilaster_" + std::to_string(side),
            tiledBrickMesh, meshPipeline, streetTexture,
            glm::vec3(x + side * halfWidth * 0.68f,
                baseY + height * 0.5f, frontZ),
            glm::vec3(0.12f, height * 0.43f, 0.10f), glm::vec4(1.0f), true);
    }

    const uint32_t windowRows = std::max(2u, static_cast<uint32_t>(height / 2.8f));
    const float windowHalfWidth = std::min(0.46f, halfWidth * 0.24f);
    const float windowHalfHeight = std::min(0.72f, height / (windowRows + 1.0f) * 0.34f);
    for (uint32_t row = 0; row < windowRows; ++row) {
        const float windowY = baseY + height * (row + 1.0f) / (windowRows + 1.0f);
        for (int side : {-1, 1}) {
            const float horizontalOffset = side * 0.34f;
            addWindow(world, name + "_Window_Z_" + std::to_string(row) + "_" +
                    std::to_string(side),
                windowMesh, meshPipeline, windowTexture,
                glm::vec3(x + horizontalOffset * halfWidth, windowY, z + halfDepth + 0.16f),
                windowHalfWidth * 2.0f, windowHalfHeight * 2.0f, 0.0f);
            addWindow(world, name + "_Window_NearZ_" + std::to_string(row) + "_" +
                    std::to_string(side),
                windowMesh, meshPipeline, windowTexture,
                glm::vec3(x + horizontalOffset * halfWidth, windowY, z - halfDepth - 0.16f),
                windowHalfWidth * 2.0f, windowHalfHeight * 2.0f, glm::radians(180.0f));
            addWindow(world, name + "_Window_X_" + std::to_string(row) + "_" +
                    std::to_string(side),
                windowMesh, meshPipeline, windowTexture,
                glm::vec3(x + halfWidth + 0.16f, windowY,
                    z + horizontalOffset * halfDepth),
                windowHalfWidth * 2.0f, windowHalfHeight * 2.0f, glm::radians(90.0f));
            addWindow(world, name + "_Window_NearX_" + std::to_string(row) + "_" +
                    std::to_string(side),
                windowMesh, meshPipeline, windowTexture,
                glm::vec3(x - halfWidth - 0.16f, windowY,
                    z + horizontalOffset * halfDepth),
                windowHalfWidth * 2.0f, windowHalfHeight * 2.0f, glm::radians(-90.0f));
        }
    }

    if (hasRooftopRoom) {
        addBox(world, name + "_RooftopRoom", tiledBrickMesh, meshPipeline, brickTexture,
            glm::vec3(x, baseY + height + 0.72f, z),
            glm::vec3(halfWidth * 0.52f, 0.30f, halfDepth * 0.52f),
            brickTint, true);
        addBox(world, name + "_RooftopCoping", tiledBrickMesh, meshPipeline, streetTexture,
            glm::vec3(x, baseY + height + 1.05f, z),
            glm::vec3(halfWidth * 0.58f, 0.06f, halfDepth * 0.58f),
            glm::vec4(1.0f), true);
    }
}

void addPointLight(flecs::world& world, const std::string& name,
    const glm::vec3& position, float intensity, float radius) {
    Lu::Component::Transform transform{};
    transform.position = position;
    const flecs::entity lightTransform = world.entity((name + "_Transform").c_str())
        .set<Lu::Component::Transform>(transform)
        .add<Lu::Component::TransformGpu>();

    world.entity(name.c_str())
        .set(flecs::Parent{lightTransform})
        .set(Lu::Component::Light{
            .type = Lu::LightType::Point,
            .color = glm::vec3(1.0f, 0.72f, 0.46f),
            .intensity = intensity,
            .radius = radius
        });
}

struct AntiqueBlockCity {
    explicit AntiqueBlockCity(flecs::world& world) {
        world.import<Lu::Module::FlyingCamera>();

        constexpr glm::vec3 cameraPosition(8.0f, 7.0f, 11.0f);
        world.entity("BrickCubeCamera")
            .set(Lu::Component::FlyingCamera{
                .position = cameraPosition,
                .angle = Lu::Component::FlyingCamera::lookAt(
                    cameraPosition, glm::vec3(0.0f, 1.0f, 0.0f)),
                .speed = 6.0f,
                .rotationSpeed = 0.005f,
                .fov = glm::radians(60.0f),
                .nearClip = 0.1f,
                .farClip = 100.0f
            })
            .add<Lu::Component::Transform>();

        const flecs::entity brickTexture = world.lookup("Texture::Bricks");
        const flecs::entity windowTexture = world.lookup("Texture::Windows");
        const flecs::entity streetTexture = world.lookup("Texture::Street");
        const flecs::entity cubeMesh = world.lookup("Mesh::BrickCube");
        const flecs::entity tiledBrickMesh = world.lookup("Mesh::TiledBrickCube");
        const flecs::entity windowMesh = world.lookup("Mesh::WindowPanel");
        const flecs::entity meshPipeline = world.lookup("MeshPipe::Bricks");

        addBox(world, "City_Ground", tiledBrickMesh, meshPipeline, streetTexture,
            glm::vec3(0.0f, -0.25f, 0.0f),
            glm::vec3(14.2f, 0.25f, 14.2f),
            glm::vec4(1.0f), true);

        addBox(world, "City_NorthSouth_Road", tiledBrickMesh, meshPipeline, streetTexture,
            glm::vec3(0.0f, 0.015f, 0.0f),
            glm::vec3(1.65f, 0.025f, 14.0f),
            glm::vec4(1.0f), true);
        addBox(world, "City_EastWest_Road", tiledBrickMesh, meshPipeline, streetTexture,
            glm::vec3(0.0f, 0.016f, 0.0f),
            glm::vec3(14.0f, 0.026f, 1.65f),
            glm::vec4(1.0f), true);

        for (int side = -1; side <= 1; side += 2) {
            const float offset = side * 1.92f;
            addBox(world, "City_NS_Sidewalk_" + std::to_string(side),
                tiledBrickMesh, meshPipeline, streetTexture,
                glm::vec3(offset, 0.13f, 0.0f),
                glm::vec3(0.25f, 0.13f, 14.0f), glm::vec4(1.0f), true);
            addBox(world, "City_EW_Sidewalk_" + std::to_string(side),
                tiledBrickMesh, meshPipeline, streetTexture,
                glm::vec3(0.0f, 0.13f, offset),
                glm::vec3(14.0f, 0.13f, 0.25f), glm::vec4(1.0f), true);
        }

        addBox(world, "City_Plaza", tiledBrickMesh, meshPipeline, streetTexture,
            glm::vec3(0.0f, 0.055f, 0.0f),
            glm::vec3(1.55f, 0.04f, 1.55f), glm::vec4(1.0f), true);
        addBox(world, "City_Fountain_Base", tiledBrickMesh, meshPipeline, streetTexture,
            glm::vec3(0.0f, 0.28f, 0.0f),
            glm::vec3(0.48f, 0.20f, 0.48f), glm::vec4(1.0f), true);
        addBox(world, "City_Fountain_Basin", tiledBrickMesh, meshPipeline, streetTexture,
            glm::vec3(0.0f, 0.63f, 0.0f),
            glm::vec3(0.70f, 0.13f, 0.70f), glm::vec4(1.0f), true);
        addBox(world, "City_Fountain_Pillar", tiledBrickMesh, meshPipeline, streetTexture,
            glm::vec3(0.0f, 0.98f, 0.0f),
            glm::vec3(0.16f, 0.30f, 0.16f), glm::vec4(1.0f), true);

        const std::array<float, 8> heights{
            5.8f, 7.4f, 8.8f, 6.4f, 9.6f, 6.9f, 7.9f, 5.5f
        };
        const std::array<glm::vec4, 4> brickTints{
            glm::vec4(0.96f, 0.88f, 0.82f, 1.0f),
            glm::vec4(0.88f, 0.82f, 0.76f, 1.0f),
            glm::vec4(0.94f, 0.86f, 0.77f, 1.0f),
            glm::vec4(0.82f, 0.80f, 0.76f, 1.0f)
        };
        uint32_t buildingIndex = 0;
        for (int zSide : {-1, 1}) {
            for (int xSide : {-1, 1}) {
                for (int zBlock = 0; zBlock < 2; ++zBlock) {
                    for (int xBlock = 0; xBlock < 2; ++xBlock) {
                        const float x = xSide * (5.0f + xBlock * 4.8f);
                        const float z = zSide * (5.0f + zBlock * 4.8f);
                        const float halfWidth = 1.75f + 0.12f * ((buildingIndex + 1) % 3);
                        const float halfDepth = 1.82f + 0.10f * (buildingIndex % 3);
                        const std::string name = "City_Building_" +
                            std::to_string(buildingIndex);
                        addBuilding(world, name, cubeMesh, tiledBrickMesh, windowMesh,
                            meshPipeline, brickTexture, streetTexture, windowTexture,
                            x, z, halfWidth, halfDepth,
                            heights[buildingIndex % heights.size()],
                            brickTints[buildingIndex % brickTints.size()],
                            buildingIndex % 3 == 1);
                        ++buildingIndex;
                    }
                }
            }
        }

        uint32_t lightIndex = 0;
        for (int position = -10; position <= 10; position += 5) {
            addPointLight(world, "City_StreetLight_" + std::to_string(lightIndex++),
                glm::vec3(0.0f, 4.4f, static_cast<float>(position)), 24.0f, 8.5f);
            if (position != 0) {
                addPointLight(world, "City_StreetLight_" + std::to_string(lightIndex++),
                    glm::vec3(static_cast<float>(position), 4.4f, 0.0f), 24.0f, 8.5f);
            }
        }

        for (int side : {-1, 1}) {
            for (int otherSide : {-1, 1}) {
                addPointLight(world,
                    "City_CourtyardLight_" + std::to_string(side) + "_" +
                        std::to_string(otherSide),
                    glm::vec3(side * 7.4f, 4.2f, otherSide * 7.4f),
                    18.0f, 6.5f);
            }
        }
    }
};

}

int main() {
    Lu::App("LuEngine Antique Brick Block City", true)
        .registerTexture("Texture::Bricks", "asset/texture/bricks.png")
        .registerTexture("Texture::Windows", "asset/texture/window.png")
        .registerTexture("Texture::Street", "asset/texture/street.png")
        .registerMeshPipe(Lu::GraphicsPipelineConfig{
            .name = "MeshPipe::Bricks",
            .capacity = 512,
            .vertexShader = "shader/mesh_basic.vert.spv",
            .fragmentShader = "shader/mesh_basic.frag.spv"
        })
        .registerShape<Lu::Shape::Cube>(
            "Mesh::BrickCube",
            glm::vec3(0.5f),
            glm::vec3(0.0f),
            glm::vec4(1.0f)
        )
        .registerShape<TiledBrickCube>(
            "Mesh::TiledBrickCube",
            glm::vec3(0.5f),
            glm::vec3(0.0f),
            glm::vec4(1.0f)
        )
        .registerShape<Lu::Shape::Square>(
            "Mesh::WindowPanel",
            0.5f,
            glm::vec3(0.0f),
            glm::vec4(1.0f)
        )
        .importModule<AntiqueBlockCity>()
        .run();
}
