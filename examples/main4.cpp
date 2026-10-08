#include "App.hpp"
#include "Shape.hpp"
#include "component/Transform.hpp"
#include "component/TransformGpu.hpp"
#include "component/MultiMesh.hpp"
#include "FlyingCamera.hpp"

struct MultiMeshDemoModule {
    explicit MultiMeshDemoModule(flecs::world& world) {
        world.import<Lu::Module::FlyingCamera>();

        world.entity("Flying Camera")
            .set(Lu::Component::FlyingCamera{
                .position = glm::vec3(0.0f, 0.0f, 10.0f),
                .angle = Lu::Component::FlyingCamera::lookAt(
                    glm::vec3(0.0f, 0.0f, 10.0f), glm::vec3(0.0f)),
                .speed = 20.0f,
                .rotationSpeed = 0.005f,
                .fov = glm::radians(75.0f),
                .nearClip = 0.02f,
                .farClip = 100.0f
            })
            .add<Lu::Component::Transform>();

        constexpr float stepSize = 10.f;
        constexpr uint32_t multiMeshCount = 40;
       
        for(uint32_t multiMeshIndex = 0; multiMeshIndex < multiMeshCount; ++multiMeshIndex){
            const std::string suffix = std::to_string(multiMeshIndex);
            const auto parent = world.entity(("MultiMeshParent_" + suffix).c_str())
                .set<Lu::Component::Transform>({
                    -stepSize * glm::vec3(sqrt(multiMeshCount)/2, sqrt(multiMeshCount)/2, 0) + 
                    stepSize * 
                    glm::vec3(multiMeshIndex % (int) sqrt(multiMeshCount), multiMeshIndex / (int) sqrt(multiMeshCount), 0),
                    random(-3.1415926f, 3.1415926f),
                    glm::normalize(glm::vec3(random(0.0f, 1.0f) + 0.001f,
                                             random(0.0f, 1.0f) + 0.001f,
                                             random(0.0f, 1.0f) + 0.001f)),
                    glm::vec3(1.0f)
                })
                .add<Lu::Component::TransformGpu>();

            const uint32_t instanceCount = static_cast<uint32_t>(random(10000.0f, 10000.0f));
            std::vector<Lu::Component::MultiMeshInstance> instances;
            instances.reserve(instanceCount);
            for(uint32_t instanceIndex = 0; instanceIndex < instanceCount; ++instanceIndex){
                Lu::Component::MultiMeshInstance instance{};
                const glm::vec3 axis = glm::normalize(glm::vec3(
                    random(0.0f, 1.0f) + 0.001f,
                    random(0.0f, 1.0f) + 0.001f,
                    random(0.0f, 1.0f) + 0.001f));
                instance.transform = Lu::Component::Transform{
                    glm::vec3(random(-8.0f, 8.0f),
                              random(-8.0f, 8.0f),
                              random(-8.0f, 8.0f)),
                    random(-3.1415926f, 3.1415926f),
                    axis,
                    glm::vec3(0.035f)
                };
                // The compute shader animates this material from the attractor state.
                instance.material.albedo = glm::vec4(1.0f);
                instance.material.ambient = glm::vec4(0.1f, 0.1f, 0.1f, 1.0f);
                instances.push_back(std::move(instance));
            }

            world.entity(("MultiMesh_" + suffix).c_str())
                .child_of(parent)
                .set(Lu::Component::MultiMesh{
                    world.lookup("Mesh::MultiMeshCube"),
                    world.lookup("MultiMeshCompute::ThomasAttractor"),
                    world.lookup("MultiMeshPipe::Basic"),
                    std::move(instances),
                    glm::vec4(0.0f, 0.0f, 0.0f, 16.0f)
                });
        }
    }
};

int main(){
    Lu::App("LuEngine MultiMesh Thomas Attractor", true, 1600, 1000)
        .registerMultiMeshPipe(Lu::GraphicsPipelineConfig{
            .name = "MultiMeshPipe::Basic",
            .capacity = Lu::Core::MAX_MULTI_MESH_INSTANCES,
            .vertexShader = "shader/multi_mesh_basic.vert.spv",
            .fragmentShader = "shader/multi_mesh_basic.frag.spv"
        })
        .registerMultiMeshComputePipe(Lu::ComputePipelineConfig{
            .name = "MultiMeshCompute::ThomasAttractor",
            .computeShader = "shader/multi_mesh_thomas_attraktor.comp.spv"
        })
        .registerShape<Lu::Shape::Cube>(
            "Mesh::MultiMeshCube",
            glm::vec3(0.5f),
            glm::vec3(0.0f),
            glm::vec4(1.0f)
        )
        .importModule<MultiMeshDemoModule>()
        .run();
}