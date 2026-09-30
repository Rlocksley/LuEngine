#include "App.hpp"
#include "Shape.hpp"
#include "component/Transform.hpp"
#include "component/TransformGpu.hpp"
#include "component/Mesh.hpp"
#include "component/Material.hpp"
#include "FlyingCamera.hpp"


struct MyRotatingCubeTag{};

struct MyLevelModul{
    MyLevelModul(flecs::world& world){

        world.import<Lu::Module::FlyingCamera>();

        world.entity("Flying Camera")
        .set(
            Lu::Component::FlyingCamera{
                .position = glm::vec3(0.0f, 0.0f, 5.0f),
                .angle = Lu::Component::FlyingCamera::
                        lookAt(glm::vec3(0,0,5.f), glm::vec3(0,0,0)),
                .speed = 10.0f,
                .rotationSpeed = 0.005f,
                .fov = glm::radians(90.0f),
                .nearClip = 0.1f,
                .farClip = 1000.0f
            }
        )
        .add<Lu::Component::Transform>();


        float attractorSpeed = 1.0f;

        world.system<Lu::Component::Transform>("CubeRotationSystem")
            .with<MyRotatingCubeTag>()    
            .each([b = 0.208186f, speed = attractorSpeed](

                flecs::iter& it, size_t index, Lu::Component::Transform& transform) {

                auto f = [b](const glm::vec3& p) {
                    return glm::vec3(
                        std::sin(p.y) - b * p.x,
                        std::sin(p.z) - b * p.y,
                        std::sin(p.x) - b * p.z
                    );
                };

                // Clamp dt and split it into small steps so frame hitches can't destabilize it.
                constexpr float maxStep = 0.05f;
                constexpr int   maxSubsteps = 8;
                const float dt = std::min(speed * it.delta_time(), maxStep * maxSubsteps);
                const int   n  = std::clamp(static_cast<int>(std::ceil(dt / maxStep)), 1, maxSubsteps);
                const float h  = dt / static_cast<float>(n);

                // RK4 integration, using the transform position itself as the state.
                glm::vec3 p = transform.position;
                for (int i = 0; i < n; ++i) {
                    const glm::vec3 k1 = f(p);
                    const glm::vec3 k2 = f(p + 0.5f * h * k1);
                    const glm::vec3 k3 = f(p + 0.5f * h * k2);
                    const glm::vec3 k4 = f(p + h * k3);
                    p += (h / 6.0f) * (k1 + 2.0f * k2 + 2.0f * k3 + k4);
                }

                transform.position = p;
            });

        uint32_t numberCubes = 50000;
        float stepSize = 0.1f;

        init_random();
        for(uint32_t i = 0; i < numberCubes; i++){
            auto cube = world.entity(("MyCube_" + std::to_string(i)).c_str())
            .add<MyRotatingCubeTag>()
            .set<Lu::Component::Transform>({
                (glm::vec3((i%(int)sqrt(numberCubes)),(i/sqrt(numberCubes)),random(0,sqrt(numberCubes))) - glm::vec3(sqrt(numberCubes)/2))*stepSize,   //position
                0,                  //angle
                glm::vec3(0,1,0),   //axis
                glm::vec3(0.01)})  //scale
            .add<Lu::Component::TransformGpu>();// mirrors Transform on Gpu

            world.entity(("MyCubeMesh_" + std::to_string(i)).c_str())
            .set(flecs::Parent{cube})
            .set(Lu::Component::Mesh{
                //MyCube entity, registered on App::registerShape
                .mesh = world.lookup("Mesh::MyCube"), 
                //Simple Mesh Pipe entity, registered on App::registerMeshPipe
                .pipeline = world.lookup("MeshPipe::Simple") 
            })
            .set(Lu::Component::Material{
                .albedo = glm::vec4(random(0,10),random(0,10),random(0,10),random(0,1)),
                .ambient = glm::vec4(0.1,0.1,0.1,1),
                .roughness = random(0,1),
                .metallic = random(0,1),
            });
        }
    }
};


int main(){
    Lu::App(
        "LuEngine Test-01",  //window title
        1200,                //window width
        700                  //window height
    )
    .registerMeshPipe(
        Lu::GraphicsPipelineConfig{
            .name = "MeshPipe::Simple",
            .capacity = 1000000,
            .vertexShader = "shader/mesh_basic.vert.spv",
            .fragmentShader = "shader/mesh_basic.frag.spv"
        }
    )
    .registerShape<Lu::Shape::Cube>(
        "Mesh::MyCube",                 //name
        glm::vec3(0.5,0.5,0.5),         //halfsize 
        glm::vec3(0.f,0.f,0.f),         //offset
        glm::vec4(1.f, 1.f, 1.f, 1.f)   //color
    )
    .importModule<MyLevelModul>()
    .run();
}
