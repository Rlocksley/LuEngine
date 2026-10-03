#pragma once

#include "Global.hpp"
#include "Constant.hpp"
#include "Core.hpp"
#include "flecs.h"
#include "Vertex.hpp"
#include "Renderer.hpp"
#include "modul/TransformGpuModule.hpp"
#include "modul/MultiMeshGpuModule.hpp"
#include "PipelineConfig.hpp"
#include "component/InputState.hpp"
#include "Exit.hpp"

namespace Lu{

    enum class ModulImportType{
        enabled,
        disabled,
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
        }

        ~App(){
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
            world->import<Module::MultiMeshGpu>();
            world->import<Module::Material>();
            world->import<Module::InputState>();

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

                    // Sleep for the remaining frame budget to cap at MAX_FRAMES_PER_SECOND_RENDERER.
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
