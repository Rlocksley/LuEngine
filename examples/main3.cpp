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
                .fov = glm::radians(60.0f),
                .nearClip = 0.1f,
                .farClip = 1000.0f
            }
        )
        .add<Lu::Component::Transform>();


        float rotationSpeed = 3.f;

        world.system("CubeRotationSystem")
            .with<MyRotatingCubeTag>()
            .with<Lu::Component::Transform>()
            .each([speed = rotationSpeed](
                flecs::iter& it, 
                size_t index) {

                Lu::Component::Transform rotation(
                    glm::vec3(0, 0, 0),        // position 
                    speed * it.delta_time(),   // angle 
                    glm::vec3(1, 1, 1),        // axis 
                    glm::vec3(1, 1, 1)         // scale
                );

                const auto& transform = it.entity(index).get<Lu::Component::Transform>();

                it.entity(index).set<Lu::Component::Transform>(rotation * transform);
            });

        auto cube = world.entity("MyCube")
        .add<MyRotatingCubeTag>()
        .set<Lu::Component::Transform>({
            glm::vec3(0,0,0),   //position
            0,                  //angle
            glm::vec3(0,1,0),   //axis
            glm::vec3(1,1,1)})  //scale
        .add<Lu::Component::TransformGpu>();// mirrors Transform on Gpu

        world.entity("MyCubeMesh")
        .child_of(cube)
        .set(Lu::Component::Mesh{
            //MyCube entity, registered on App::registerShape
            .mesh = world.lookup("Mesh::MyCube"), 
            //Simple Mesh Pipe entity, registered on App::registerMeshPipe
            .pipeline = world.lookup("MeshPipe::Simple") 
        })
        .set(Lu::Component::Material{
            .albedo = glm::vec4(1,1,1,1),
            .ambient = glm::vec4(0.1,0.1,0.1,1),
            .roughness = 0.5,
            .metallic = 0.5,
        });
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
            .capacity = 1000,
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
