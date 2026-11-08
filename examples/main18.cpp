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
            .each([](flecs::entity e, Lu::Component::Transform& transform) {
                float deltaTime = e.world().delta_time();
                float angle = deltaTime * 1.0f;
                glm::quat rotationDelta = glm::angleAxis(angle, glm::vec3(0.0f, 1.0f, 0.0f));
                transform.rotation = rotationDelta * transform.rotation;
            });

        uint32_t numberCubes = 10;

        flecs::entity parent;
        init_random();
        for(uint32_t i = 0; i < numberCubes; i++){
            auto cube = world.entity(("MyCube_" + std::to_string(i)).c_str()); 
            if(i != 0){
                cube.set(flecs::Parent{parent});
                cube.add<MyRotatingCubeTag>();
            }else{
                cube.add<MyRotatingCubeTag>();
            }
            parent = cube
            .set<Lu::Component::Transform>({
                glm::vec3(5/sqrt(i+1),0,0),   //position
                random(3.14f/2.f,3.14f/2.f),                  //angle
                glm::normalize(glm::vec3(random(0,0),random(1,1),random(0,0))),   //axis
                glm::vec3(1/2.f)})  //scale
            .add<Lu::Component::TransformGpu>();// mirrors Transform on Gpu

            world.entity(("MyCubeMesh_" + std::to_string(i)).c_str())
            .set(flecs::Parent{parent})
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
        true                 //fullscreen,
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
