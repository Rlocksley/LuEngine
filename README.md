# LuEngine

LuEngine is a C++ game engine built around two user-facing APIs:

**Game Logic API**\
You write your whole Game within flecs ECS.\
If you add/set/remove LuEngine Core Components, Observer are triggered, which talk to the Renderer.

**Shader API** \
register graphics pipelines for meshes, multimeshes, skeletonmeshes and 2D text \
plus compute pipelines for multimesh-instance transformation.


The renderer is Vulkan-based.

## To a working Development-Environment in 10 Minutes (Window, Linux)
1.] clone the repo with `git clone https://github.com/Rlocksley/LuEngine` \
2.] install CMake and the Vulkan SDK (Vulkan 1.4)\
3.] install Visual Studio Code and install the Cpp and CMake Extensions\
4.] clone vcpkg inside the parent directory of the LuEngine repo directory and run\
`vcpkg install [glfw3,glm,flecs,joltphysics,assimp,freetype]` \
5.] open the LuEngine repo in Visual Studio Code and right click `LuEngine/CMakeLists.txt` -> build\
6.] compile the shaders `LuEngine/shader/compile.bat`\
(you need to get glslc from the Vulkan SDK and paste it into the shader directory)\
7.] for your own project, create a directory in `examples` and\
copy/adjust the `examples/CMakeLists.txt` and `.vscode/launch.json` , `.vscode/tasks.json`

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
<summary>3. Mesh Prefab Loading</summary>

`App::loadModel` imports a model with Assimp, registers its mesh geometry and textures, and creates a Flecs prefab named `Mesh::<name>` by default. Register the mesh pipeline before loading the model.

```cpp
Lu::App("Robot scene")
    .registerMeshPipe(Lu::GraphicsPipelineConfig{
        .name = "MeshPipe::Simple",
        .capacity = 128,
        .vertexShader = "shader/mesh_basic.vert.spv",
        .fragmentShader = "shader/mesh_pbr.frag.spv"
    })
    .loadModel("Robot", "asset/models/robot.gltf")
    .importModule<RobotScene>()
    .run();

auto robotPrefab = world.lookup("Mesh::Robot");
world.entity("RobotInstance")
    .is_a(robotPrefab)
    .set<Lu::Component::Transform>({
        glm::vec3(0.0f), 0.0f,
        glm::vec3(0.0f, 1.0f, 0.0f),
        glm::vec3(1.0f)
    });
```
</details>

<details>
<summary>4. Skeleton Mesh Prefab Loading</summary>

`App::loadSkeletonModel` imports a skinned model, registers its skeleton asset and baked animation clips, and creates a prefab named `Skeleton::<name>` by default. Register a skeleton pipeline before loading it. The prefab contains a skeleton parent and child meshes; instantiate it with `is_a` and give the instance a `Transform`.

```cpp
Lu::App("Character scene")
    .threads(4)
    .registerSkeletonPipe(Lu::GraphicsPipelineConfig{
        .name = "SkeletonPipe::Simple",
        .capacity = 4096,
        .vertexShader = "shader/skeleton_basic.vert.spv",
        .fragmentShader = "shader/skeleton_pbr.frag.spv"
    })
    .loadSkeletonModel("Hero", "asset/models/hero.fbx")
    .importModule<CharacterScene>()
    .run();

auto heroPrefab = world.lookup("Skeleton::Hero");
world.entity("HeroInstance")
    .is_a(heroPrefab)
    .set<Lu::Component::Transform>({
        glm::vec3(0.0f), 0.0f,
        glm::vec3(0.0f, 1.0f, 0.0f),
        glm::vec3(1.0f)
    });
```

See the **Skeleton Animation** section for animation graphs, state machines, and character control.
</details>

<details>
<summary>5. Skeleton Animation</summary>

Skeleton meshes can be animated with baked clips and an `AnimationStateMachine`.\
See ```examples/main13.cpp```.

```cpp
Lu::App("Character")
    .threads(4)                                   // animation graphs are evaluated in parallel
    .registerSkeletonPipe({
        .name = "SkeletonPipe::Simple", 
        .capacity = 4096,
        .vertexShader = "shader/skeleton_basic.vert.spv", 
        .fragmentShader = "shader/skeleton_pbr.frag.spv"}
    )
    // skeleton asset + baked clips "Hero::<anim>" + prefab "Skeleton::Hero"
    .loadSkeletonModel("Hero", "asset/models/hero.fbx")   
    .importModule<MyGame>()
    .run();

    Lu::AnimationGraphBuilder("HeroGraph", kModel)
        .clip("Idle", "Idle")
        .clip("Walk", "Walk")
        .clip("Run",  "Run")
        .blend1D("Locomotion", "speed", {{"Idle", 0.0f}, {"Walk", 0.5f}, {"Run", 1.0f}})
        .clip("Wave", "Wave", /*looping*/ false)
        .layer("Interaction",/*top*/ "Wave", /*base*/ "Locomotion", "Hero::UpperBody",
                /*blendIn*/ 0.1f, /*blendOut*/ 0.15f, /*onFinish*/ "Locomotion")
        .start("Locomotion")
        .build();

    // ---- the hero: prefab (skeleton + meshes) + capsule + state machine ---------------------------
    world.entity("Hero")
        .is_a(world.lookup("Skeleton::Hero"))
        .add<Hero>()
        // body: position / yaw come from the CharacterCapsule (Jolt), scale stays 1 so the capsule sizes are meters
        .set<Transform>({glm::vec3(0.0f, 0.0f, 0.0f), 0.0f, glm::vec3(0, 1, 0), glm::vec3(1.0f)})
        // visuals: all SkeletonMeshes of the hero are transformed by Transform * MeshTransform.
        // Change it at runtime with:  hero.get_mut<MeshTransform>().position.y += 0.1f; hero.modified<MeshTransform>();
        .set<MeshTransform>(MeshTransform(glm::vec3(0.0f, kMeshHeight, 0.0f),
                                            glm::radians(kMeshFrontYawDegrees), glm::vec3(0, 1, 0),
                                            glm::vec3(kMeshScale)))
        .set<AnimationStateMachine>(AnimationStateMachine::create("HeroGraph"))
        .set<CharacterCapsule>({.radius = 0.35f, .halfHeight = 0.55f})
        .add<Lu::Component::PhysicsFocus>();

```
</details>

<details>
<summary>6. Writing Your Own Shaders</summary>

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
</details>

<details>
<summary>7. 2D Text</summary>

Text is drawn by the 2D part of the renderer: `Transform2D` + `Camera2D` + `Text`, with fonts rasterized by FreeType.
Source: `examples/main15.cpp` (`lu_example15 [font.ttf]`).

```cpp
Lu::App("Text", false, 1280, 800)
    // FreeType -> atlas (signed distance field by default) -> a texture of the global texture array
    .registerFont("Font::Main", "path/to/font.ttf", 64)
    // shaders of the glyph quads, register your own like a mesh pipe. capacity = max glyphs per frame
    .registerFontPipe(Lu::GraphicsPipelineConfig{
        .name = "FontPipe::Basic",
        .capacity = 4096,
        .vertexShader = "shader/font_basic.vert.spv",
        .fragmentShader = "shader/font_basic.frag.spv",
        .cullMode = VK_CULL_MODE_NONE,
        .depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL
    })
    .importModule<MyModule>()
    .run();

auto label = world.entity("Label")
.set<Lu::Component::Transform2D>({
    .position = {20.f, 20.f},      // pixels, origin top-left, y down
    .positionZ = 5,                // layer, higher = in front
    // .rotation, .scale
});

world.entity("LabelText")
.set(Parent{label})
.set(Lu::Component::Text{
    .text = "Hello World",  
    .font = world.lookup("Font::Main"),
    .pipeline = world.lookup("FontPipe::Basic"),
    .color = glm::vec4(1.f),
    .size = 32.f,                  // line height in pixels
    .align = Lu::TextAlign::Left,
    .maxWidth = 400.f              // optional word wrapping, 0 = off (see below)
});
// change the string later with .set<Lu::Component::Text>(...) or .modified<Lu::Component::Text>():
```
</details>
