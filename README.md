# LuEngine

LuEngine is a C++ game engine built around two user-facing APIs:

- **Game Logic API:** You write your whole Game within flecs ECS.\
   If you add/set/remove LuEngine Core Components, Observer are triggered, which talk to the Renderer.
- **Shader API:** register graphics pipelines for meshes and multimeshes, \
  plus compute pipelines for multimesh-instance transformation. \
  Shaders follow the buffer layouts and descriptor bindings used by the engine templates.

The renderer is Vulkan-based.

## To a working Development-Environment in 10 Minutes (Window, Linux)
1.] clone the repo with `git clone https://github.com/Rlocksley/LuEngine` \
2.] install CMake and the Vulkan SDK (Vulkan 1.4)\
3.] install Visual Studio Code and install the Cpp and CMake Extensions\
4.] clone vcpkg inside the parent directory of the LuEngine repo directory and `run vcpkg install [glfw3,glm,flecs]` \
5.] open the LuEngine repo in Visual Studio Code and right click `LuEngine/CMakeLists.txt` -> build\
6.] compile the shaders `LuEngine/shader/compile.bat`\
(you need to get glslc from the Vulkan SDK and paste it into the shader directory)\
7.] for your own project, create a directory in `examples` and\
copy/adjust the `examples/CMakeLists.txt` and `.vscode/launch.json` , `.vscode/tasks.json`

- vcpkg dependencies configured by `.vscode/settings.json`: Vulkan headers/loader, GLFW, GLM, and Flecs

<details>
<summary>1. Hello World</summary>

The minimal example creates one cube, gives it a GPU-synchronized transform, and rotates it from a Flecs system. `App::registerShape` uploads the cube geometry and `registerMeshPipe` registers the vertex/fragment-shader pipeline, which you reference from the MeshComponent.

Source: `examples/hello_world.cpp`.

```cpp
#include "App.hpp"
#include "Shape.hpp"
#include "FlyingCamera.hpp"
#include "component/Material.hpp"
#include "component/Mesh.hpp"
#include "component/Transform.hpp"
#include "component/TransformGpu.hpp"

struct HelloCubeTag {};

struct HelloWorldModule {
    explicit HelloWorldModule(flecs::world& world) {
        world.import<Lu::Module::FlyingCamera>();

        world.entity("Camera")
            .set(Lu::Component::FlyingCamera{
                .position = glm::vec3(0.0f, 0.0f, 5.0f),
                .angle = Lu::Component::FlyingCamera::lookAt(
                    glm::vec3(0.0f, 0.0f, 5.0f), glm::vec3(0.0f)),
                .speed = 5.0f,
                .rotationSpeed = 0.005f,
                .fov = glm::radians(70.0f),
                .nearClip = 0.1f,
                .farClip = 100.0f
            })
            .add<Lu::Component::Transform>();

        world.system<Lu::Component::Transform>("RotateCube")
            .with<HelloCubeTag>()
            .each([](flecs::iter& it, size_t, Lu::Component::Transform& transform) {
                const float angle = glm::radians(45.0f) * it.delta_time();
                transform.rotation = glm::angleAxis(angle, glm::vec3(0.0f, 1.0f, 0.0f))
                                   * transform.rotation;
            });

        const auto cube = world.entity("Cube")
            .add<HelloCubeTag>()
            .set<Lu::Component::Transform>({
                glm::vec3(0.0f), 0.0f, glm::vec3(0.0f, 1.0f, 0.0f), glm::vec3(1.0f)
            })
            .add<Lu::Component::TransformGpu>();

        world.entity("CubeMesh")
            .child_of(cube)
            .set(Lu::Component::Mesh{
                .mesh = world.lookup("Mesh::Cube"),
                .pipeline = world.lookup("MeshPipe::Basic")
            })
            .set(Lu::Component::Material{
                .albedo = glm::vec4(0.25f, 0.7f, 0.95f, 1.0f),
                .ambient = glm::vec4(0.12f, 0.12f, 0.12f, 1.0f)
            });
    }
};

int main() {
    Lu::App("LuEngine Hello World", false, 1280, 800)
        .registerMeshPipe(Lu::GraphicsPipelineConfig{
            .name = "MeshPipe::Basic",
            .capacity = 1,
            .vertexShader = "shader/mesh_basic.vert.spv",
            .fragmentShader = "shader/mesh_basic.frag.spv"
        })
        .registerShape<Lu::Shape::Cube>(
            "Mesh::Cube", glm::vec3(0.5f), glm::vec3(0.0f), glm::vec4(1.0f))
        .importModule<HelloWorldModule>()
        .run();
}
```
</details>

<details>
<summary>2. MultiMesh</summary>

A `Component::MultiMesh` represents many copies of one registered geometry. It references:

- A geometry entity registered with `registerShape` or `registerMesh`.
- A compute pipeline registered with `registerMultiMeshComputePipe`.
- A multimesh graphics pipeline registered with `registerMultiMeshPipe`.
- A vector of per-instance `Component::MultiMeshInstance` values, each containing a transform and material.

The multimesh entity must be a child of a parent that has both `Transform` and `TransformGpu`. The parent transform places the whole group; each instance transform is local to that parent. The engine's `MultiMeshGpu` module submits component changes to the renderer.

```cpp
std::vector<Lu::Component::MultiMeshInstance> instances;
instances.reserve(1000);
for (uint32_t i = 0; i < 1000; ++i) {
    Lu::Component::MultiMeshInstance instance{};
    instance.transform = Lu::Component::Transform{
        glm::vec3(static_cast<float>(i % 25), static_cast<float>(i / 25), 0.0f),
        0.0f,
        glm::vec3(0.0f, 1.0f, 0.0f),
        glm::vec3(0.05f)
    };
    instance.material.albedo = glm::vec4(0.9f, 0.45f, 0.15f, 1.0f);
    instances.push_back(instance);
}

auto parent = world.entity("InstancesParent")
    .set<Lu::Component::Transform>({})
    .add<Lu::Component::TransformGpu>();

world.entity("Instances")
    .child_of(parent)
    .set(Lu::Component::MultiMesh{
        world.lookup("Mesh::Cube"),
        world.lookup("MultiMeshCompute::UpdateInstances"),
        world.lookup("MultiMeshPipe::Basic"),
        std::move(instances),
        glm::vec4(0.0f, 0.0f, 0.0f, 10.0f)
    });
```

Register the pipelines before the Flecs module creates the multimesh entities:

```cpp
.registerMultiMeshPipe(Lu::GraphicsPipelineConfig{
    .name = "MultiMeshPipe::Basic",
    .capacity = Lu::Core::MAX_INSTANCED_MESHES,
    .vertexShader = "shader/multi_mesh_basic.vert.spv",
    .fragmentShader = "shader/multi_mesh_basic.frag.spv"
})
.registerMultiMeshComputePipe(Lu::ComputePipelineConfig{
    .name = "MultiMeshCompute::UpdateInstances",
    .computeShader = "shader/multi_mesh_template.comp.spv"
})
```

The sample scene is `examples/main4.cpp`; it registers a cube and creates parented multimeshes using the Thomas-attractor compute shader.
</details>

<details>
<summary>3. Writing Your Own Shaders</summary>

The engine expects shader interfaces to match the descriptor set layouts created by its packages.\
In simple terms "let the predefined structs and the layout() bindings in the shader code  before the `void main(){}` function the same.

### Regular mesh graphics pipeline
Templates: `shader/mesh_basic.vert` and `shader/mesh_basic.frag`\
register with
``` cpp
App::registerMeshPipe();
```
### Multi Mesh Graphics Pipeline
Templates: `shader/multi_mesh_basic.vert` and `shader/multi_mesh_basic.frag`\
register with
``` cpp
App::registerMultiMeshPipe();
```
### Multi Mesh Compute Pipeline
Templates: `shader/multi_mesh_thomas_attraktor.comp`\
register with
``` cpp
App::registerMultiMeshComputePipe();
```
