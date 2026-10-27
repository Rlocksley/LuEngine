#pragma once

#include "Global.hpp"
#include "Constant.hpp"
#include "Core.hpp"
#include "flecs.h"
#include "Vertex.hpp"
#include "Renderer.hpp"
#include "AssetManager.hpp"
#include "modul/TransformGpuModule.hpp"
#include "modul/MultiMeshGpuModule.hpp"
#include "modul/LightModule.hpp"
#include "modul/Transform2DGpuModule.hpp"
#include "modul/Camera2DModule.hpp"
#include "modul/TextModule.hpp"
#include "modul/Mesh2DModule.hpp"
#include "component/Mesh.hpp"
#include "component/Skeleton.hpp"
#include "component/AnimationStateMachine.hpp"
#include "component/InputState.hpp"
#include "PipelineConfig.hpp"
#include "Exit.hpp"

namespace Lu{

    enum class ModulImportType{
        enabled,
        disabled,
    };

    struct CpuTexture {
        std::string name{};
        uint32_t width{0};
        uint32_t height{0};
        std::shared_ptr<uint8_t[]> pixels{nullptr};
        VkFormat vkFormat{VK_FORMAT_R8G8B8A8_SRGB};

        CpuTexture() = default;
        CpuTexture(const std::string& name, uint32_t width, uint32_t height, std::shared_ptr<uint8_t[]> pixels, VkFormat format = VK_FORMAT_R8G8B8A8_SRGB)
            : name(name), width(width), height(height), pixels(std::move(pixels)), vkFormat(format) {}
        CpuTexture(uint32_t width, uint32_t height, std::shared_ptr<uint8_t[]> pixels, VkFormat format = VK_FORMAT_R8G8B8A8_SRGB)
            : width(width), height(height), pixels(std::move(pixels)), vkFormat(format) {}
    };

    class App{
        Core::Renderer* renderer;
        flecs::world* world;

        std::vector<std::function<void(flecs::world&)>> moduleFuncs;

    public:
        App(const std::string& windowTitle, 
            bool fullscreen = false,
            const uint32_t windowWidth = 1000, const uint32_t windowHeight = 700,
            const VkSampleCountFlagBits vkSampleCountFlagBits = VK_SAMPLE_COUNT_4_BIT
        ) {
            Core::windowTitle = windowTitle;
            Core::windowWidth = windowWidth;
            Core::windowHeight = windowHeight;
            Core::fullscreen = fullscreen;
            Core::vkSampleCountFlagBits = vkSampleCountFlagBits; 
            Core::createCore();

            renderer = new Core::Renderer();
            world = new flecs::world();
            Core::GetAssetManager().set(renderer, world);
        }

        ~App(){
            Core::GetAssetManager().set(nullptr, nullptr);
            delete world;
            delete renderer;
            Core::destroyCore();
        }

        App& registerMeshPipe(const GraphicsPipelineConfig& config){
            LU_ASSERT(config.name.size() > 0, "App", "registerMeshPipe", "GraphicsPipelineConfig.name must be size bigger 0");
            LU_ASSERT(config.capacity > 0, "App", "registerMeshPipe", "GraphicsPipelineConfig::capacity must be bigger 0");
            LU_ASSERT(!world->lookup(config.name.c_str()), "App", "registerMeshPipe", "Entity with name: " + config.name + " already exists");
            
            renderer->createMeshPipe(world->entity(config.name.c_str()).id(), config);

            return *this;
        }

        // Pipeline for 2D text (vertex shader: shader/font_basic.vert, fragment shader: shader/font_basic.frag).
        // config.capacity = maximum number of glyphs this pipeline draws per frame.
        // Recommended: cullMode = VK_CULL_MODE_NONE, depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL.
        App& registerFontPipe(const GraphicsPipelineConfig& config){
            LU_ASSERT(config.name.size() > 0, "App", "registerFontPipe", "GraphicsPipelineConfig.name must be size bigger 0");
            LU_ASSERT(config.capacity > 0, "App", "registerFontPipe", "GraphicsPipelineConfig::capacity must be bigger 0");
            LU_ASSERT(!world->lookup(config.name.c_str()), "App", "registerFontPipe", "Entity with name: " + config.name + " already exists");

            renderer->createFontPipe(world->entity(config.name.c_str()).id(), config);

            return *this;
        }

        // Rasterizes a font with FreeType into an atlas texture. The entity `config.name` is what
        // Component::Text::font refers to; the atlas is the texture entity "<name>::Atlas".
        App& registerFont(const FontConfig& config){
            LU_ASSERT(config.name.size() > 0, "App", "registerFont", "FontConfig.name must be size bigger 0");
            LU_ASSERT(!world->lookup(config.name.c_str()), "App", "registerFont", "Entity with name: " + config.name + " already exists");
            const std::string atlasName = config.name + "::Atlas";
            LU_ASSERT(!world->lookup(atlasName.c_str()), "App", "registerFont", "Entity with name: " + atlasName + " already exists");

            renderer->createFont(world->entity(config.name.c_str()).id(), world->entity(atlasName.c_str()).id(), config);

            return *this;
        }

        App& registerFont(const std::string& name, const std::string& filePath,
            const uint32_t pixelSize = 48, const bool sdf = true){
            FontConfig config;
            config.name = name;
            config.filePath = filePath;
            config.pixelSize = pixelSize;
            config.sdf = sdf;
            return registerFont(config);
        }

        // Pipeline for 2D meshes (GUI): GraphicsPipeline<Vertex::Vertex2D>, e.g. shader/mesh2d_basic.vert/.frag.
        // Set 0 of the shaders: binding 0 Camera2D (uniform), 1 Transform2D buffer, 2 Mesh2D buffer, 3 the global texture array
        // (see Mesh2DPackage.hpp). config.capacity = maximum number of Mesh2D this pipeline draws per frame.
        // Recommended: depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL (the default cull mode works with the Shape2D shapes).
        App& registerMesh2DPipe(const GraphicsPipelineConfig& config){
            LU_ASSERT(config.name.size() > 0, "App", "registerMesh2DPipe", "GraphicsPipelineConfig.name must be size bigger 0");
            LU_ASSERT(config.capacity > 0, "App", "registerMesh2DPipe", "GraphicsPipelineConfig::capacity must be bigger 0");
            LU_ASSERT(!world->lookup(config.name.c_str()), "App", "registerMesh2DPipe", "Entity with name: " + config.name + " already exists");

            renderer->createMesh2DPipe(world->entity(config.name.c_str()).id(), config);

            return *this;
        }

        // Registers 2D geometry (vertices in local pixels). The entity `name` is what Component::Mesh2D::mesh refers to.
        App& registerMesh2D(const std::string& name,
            const std::vector<Vertex::Vertex2D>& vertexBuffer, const std::vector<uint32_t>& indexBuffer){
            LU_ASSERT(name.size() > 0, "App", "registerMesh2D", "name must be size bigger zer");
            LU_ASSERT(!world->lookup(name.c_str()), "App", "registerMesh2D", "Entity with name: " + name + " already exists");

            renderer->createMesh2DGeometry(world->entity(name.c_str()).id(), vertexBuffer, indexBuffer);

            return *this;
        }

        // registerShape2D<Shape2D::Rectangle>("Mesh2D::Rect", glm::vec2(200, 80)) - see shape/Shape2D.hpp
        template<typename Shape, typename ...Args>
        App& registerShape2D(const std::string& name, Args&&... args){
            Shape shape(std::forward<Args>(args)...);
            return registerMesh2D(name, shape.getVertices(), shape.getIndices());
        }

        App& registerMultiMeshPipe(const GraphicsPipelineConfig& config){
            LU_ASSERT(config.name.size() > 0, "App", "registerMultiMeshPipe", "GraphicsPipelineConfig.name must be size bigger 0");
            LU_ASSERT(!world->lookup(config.name.c_str()), "App", "registerMultiMeshPipe", "Entity with name: " + config.name + " already exists");
        
            renderer->createMultiMeshPipe(world->entity(config.name.c_str()).id(), config);
        
            return *this;
        }

        App& registerMultiMeshComputePipe(const ComputePipelineConfig& config){
            LU_ASSERT(config.name.size() > 0, "App", "registerMultiMeshComputePipe", "ComputePipelineConfig.name must be size bigger 0");
            LU_ASSERT(!world->lookup(config.name.c_str()), "App", "registerMultiMeshComputePipe", "Entity with name: " + config.name + " already exists");
        
            renderer->createMultiMeshComputePipe(world->entity(config.name.c_str()).id(), config);
        
            return *this;
        }

        App& registerMesh(const std::string& name, 
            const std::vector<Vertex::Mesh>& vertexBuffer, const std::vector<uint32_t>& indexBuffer){
            LU_ASSERT(name.size() > 0, "App", "registerMeshGeometry", "name must be size bigger zer");
            LU_ASSERT(!world->lookup(name.c_str()), "App", "registerMeshGeometry", "Entity with name: " + name + " already exists");
            
            renderer->createMeshGeometry(world->entity(name.c_str()).id(), vertexBuffer, indexBuffer);
            
            return *this;
        }

        template<typename Shape, typename ...Args>
        App& registerShape(const std::string& name, Args&&... args){
            Shape shape(std::forward<Args>(args)...);
            return registerMesh(name, shape.getVertices(), shape.getIndices());
        }

        App& registerTexture(const std::string& name, const std::string& filePath){
            LU_ASSERT(name.size() > 0, "App", "registerTexture", "name must be size bigger zer");
            LU_ASSERT(!world->lookup(name.c_str()), "App", "registerTexture", "Entity with name: " + name + " already exists");
            
            renderer->createTexture(world->entity(name.c_str()).id(), filePath);

            return *this;
        }

        App& registerTexture(CpuTexture&& cpuTexture){
            LU_ASSERT(cpuTexture.name.size() > 0, "App", "registerTexture", "CpuTexture.name must be size bigger zer");
            LU_ASSERT(!world->lookup(cpuTexture.name.c_str()), "App", "registerTexture", "Entity with name: " + cpuTexture.name + " already exists");

            Core::TextureCpu tex(cpuTexture.width, cpuTexture.height, std::move(cpuTexture.pixels));
            tex.vkFormat = cpuTexture.vkFormat;
            renderer->createTexture(world->entity(cpuTexture.name.c_str()).id(), std::move(tex));

            return *this;
        }

        App& registerTexture(const CpuTexture& cpuTexture){
            LU_ASSERT(cpuTexture.name.size() > 0, "App", "registerTexture", "CpuTexture.name must be size bigger zer");
            LU_ASSERT(!world->lookup(cpuTexture.name.c_str()), "App", "registerTexture", "Entity with name: " + cpuTexture.name + " already exists");

            Core::TextureCpu tex(cpuTexture.width, cpuTexture.height, cpuTexture.pixels);
            tex.vkFormat = cpuTexture.vkFormat;
            renderer->createTexture(world->entity(cpuTexture.name.c_str()).id(), std::move(tex));

            return *this;
        }

        App& registerTexture(const std::string& name, Core::TextureCpu&& cpuTexture){
            LU_ASSERT(name.size() > 0, "App", "registerTexture", "name must be size bigger zer");
            LU_ASSERT(!world->lookup(name.c_str()), "App", "registerTexture", "Entity with name: " + name + " already exists");

            renderer->createTexture(world->entity(name.c_str()).id(), std::move(cpuTexture));

            return *this;
        }

        App& registerTexture(const std::string& name, CpuTexture&& cpuTexture){
            cpuTexture.name = name;
            return registerTexture(std::move(cpuTexture));
        }

        App& loadModel(const std::string& name, const std::string& filePath,
            const std::string& prefix = "Mesh::",
            const std::string& pipelineName = "MeshPipe::Simple") {
            Core::GetAssetManager().loadModel(name, filePath, prefix, pipelineName);
            return *this;
        }

        // Pipeline for skinned meshes (vertex shader: shader/skeleton_basic.vert, see Vertex::Skeleton).
        App& registerSkeletonPipe(const GraphicsPipelineConfig& config){
            LU_ASSERT(config.name.size() > 0, "App", "registerSkeletonPipe", "GraphicsPipelineConfig.name must be size bigger 0");
            LU_ASSERT(config.capacity > 0, "App", "registerSkeletonPipe", "GraphicsPipelineConfig::capacity must be bigger 0");
            LU_ASSERT(!world->lookup(config.name.c_str()), "App", "registerSkeletonPipe", "Entity with name: " + config.name + " already exists");

            renderer->createSkeletonPipe(world->entity(config.name.c_str()).id(), config);

            return *this;
        }

        // Skinned model -> skeleton asset, baked animation clips ("<name>::<clip>") and the prefab "<prefix><name>".
        App& loadSkeletonModel(const std::string& name, const std::string& filePath,
            const std::string& prefix = "Skeleton::",
            const std::string& pipelineName = "SkeletonPipe::Simple",
            const float bakeFrameRate = 30.0f) {
            Core::GetAssetManager().loadSkeletonModel(name, filePath, prefix, pipelineName, bakeFrameRate);
            return *this;
        }

        // Worker threads of the ECS (animation evaluation is multi threaded). 0 = single threaded.
        App& threads(const int count){
            world->set_threads(count);
            return *this;
        }

        template<typename Module>
        App& importModule(const ModulImportType importType = ModulImportType::enabled){
            moduleFuncs.push_back(
                [importType = std::move(importType)](flecs::world& world){
                    auto modul = world.import<Module>();
                    if(importType == ModulImportType::disabled){
                        modul.disable();
                    }
                }
            );
            return *this;
        }

        void run(){

            Core::FramerateMonitor framerateMonitor;
            
            world->import<Module::Transform>();
            world->import<Module::TransformGpu>();
            world->import<Module::Mesh>();
            world->import<Module::Skeleton>();
            world->import<Module::AnimationStateMachine>();
            world->import<Module::MultiMeshGpu>();
            world->import<Module::Material>();
            world->import<Module::Light>();
            world->import<Module::InputState>();
            // 2D: Transform2D before Text, so the Transform2D slot exists on the renderer side before CreateText arrives
            world->import<Module::Transform2DGpu>();
            world->import<Module::Camera2D>();
            world->import<Module::Text>();
            world->import<Module::Mesh2D>();

            #ifdef LU_DEBUG
            // 1. Enable the embedded REST API server (runs on default port 27750)
            world->set<flecs::Rest>({});

             // 1. Optional: Import the statistics module to gather performance profiling
            world->import<flecs::stats>(); 
            #endif


            for(auto func : moduleFuncs){
                func(*world);
            }
            moduleFuncs.clear();

            // flecs ecs thread
            std::thread ecsThread([&](){
                std::chrono::duration<double> targetFrameTime(1.0 / Core::MAX_FRAMES_PER_SECOND_ECS);

                while (Core::isRunning.load(std::memory_order_relaxed)) {
                    auto frameStart = std::chrono::high_resolution_clock::now();
                    
                    if(!world->progress()){
                        break;
                    }
                    framerateMonitor.recordEcsFrame();

                    // Sleep for the remaining ECS frame budget.
                    auto frameEnd = std::chrono::high_resolution_clock::now();
                    auto elapsed  = frameEnd - frameStart;
                    if (elapsed < targetFrameTime) {
                        std::this_thread::sleep_for(targetFrameTime - elapsed);
                    }
                }
                
                if(Core::isRunning.load(std::memory_order_relaxed)){
                    Core::isRunning.store(false, std::memory_order_relaxed);
                }
            });

            //render thread
            renderer->run(framerateMonitor);

            ecsThread.join();
        }
    };
}
