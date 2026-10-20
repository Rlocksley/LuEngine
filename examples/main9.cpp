#include "App.hpp"
#include "FlyingCamera.hpp"
#include "component/Light.hpp"
#include "component/Transform.hpp"
#include "component/TransformGpu.hpp"

namespace {

void addLight(flecs::world& world, const std::string& name,
    const glm::vec3& position, const glm::vec3& color,
    float intensity, float radius) {
    Lu::Component::Transform transform{};
    transform.position = position;
    transform.rotation = glm::angleAxis((0.3f-3.14f)/2.f, glm::vec3(1.f,0.f,0.f));

    const flecs::entity lightTransform = world.entity((name + "Transform").c_str())
        .set<Lu::Component::Transform>(transform)
        .add<Lu::Component::TransformGpu>();

    world.entity(name.c_str())
        .child_of(lightTransform)
        .set<Lu::Component::Light>({
            .type = Lu::LightType::Directional,
            .color = color,
            .intensity = intensity,
            .radius = radius
        });
}

struct SponzaScene {
    explicit SponzaScene(flecs::world& world) {
        world.import<Lu::Module::FlyingCamera>();

        const glm::vec3 cameraPosition(0.0f, 4.5f, 18.0f);
        world.entity("SponzaCamera")
            .set<Lu::Component::FlyingCamera>({
                .position = cameraPosition,
                .angle = Lu::Component::FlyingCamera::lookAt(
                    cameraPosition, glm::vec3(0.0f, 3.0f, 0.0f)),
                .speed = 8.0f,
                .rotationSpeed = 0.005f,
                .fov = glm::radians(65.0f),
                .nearClip = 0.1f,
                .farClip = 200.0f
            })
            .add<Lu::Component::Transform>();

        world.entity("SponzaInstance")
            .is_a(world.lookup("Model::Sponza"))
            .set<Lu::Component::Transform>({
                glm::vec3(0.0f),
                0.0f,
                glm::vec3(0.0f, 1.0f, 0.0f),
                glm::vec3(2.0f)
            });

        addLight(world, "SponzaWarmLight",
            glm::vec3(0.0f, 100.0f, 0.0f),
            glm::vec3(1.0f, 0.72f, 0.48f), 10.0f, 100.0f);
        /*addPointLight(world, "SponzaCoolLight",
            glm::vec3(4.0f, 1.0f, 0.0f),
            glm::vec3(0.48f, 0.68f, 1.0f), 60.0f, 100.0f);
        addPointLight(world, "SponzaAtriumLight",
            glm::vec3(0.0f, 1.0f, -3.0f),
            glm::vec3(1.0f, 0.88f, 0.68f), 60.0f, 100.0f);
        */
    }
};

}

int main() {
    Lu::App("LuEngine Sponza Model", false, 1600, 1000)
        .registerMeshPipe(Lu::GraphicsPipelineConfig{
            .name = "MeshPipe::Sponza",
            .capacity = 512,
            .vertexShader = "shader/mesh_basic.vert.spv",
            .fragmentShader = "shader/mesh_pbr.frag.spv",
            .cullMode = VK_CULL_MODE_NONE
        })
        .loadModel(
            "Sponza",
            "asset/sponza/source/scene.glb",
            "Model::",
            "MeshPipe::Sponza")
        .importModule<SponzaScene>()
        .run();
}
