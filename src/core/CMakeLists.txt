cmake_minimum_required(VERSION 3.16.3)

project(LU)

add_library(lu_core STATIC
Allocator.hpp
Allocator.cpp
Command.hpp
Command.cpp
Buffer.hpp
Buffer.cpp
Constant.hpp
Core.hpp
Core.cpp
DescriptorPool.hpp
DescriptorPool.cpp
Device.hpp
Device.cpp
Fence.hpp
Fence.cpp
Framebuffers.hpp
Framebuffers.cpp
Global.hpp
Image.hpp
Image.cpp
Input.hpp
Input.cpp
Instance.hpp
Instance.cpp
PhysicalDevice.hpp
PhysicalDevice.cpp
Queue.hpp
Queue.cpp
Semaphore.hpp
Semaphore.cpp
Shader.hpp
Shader.cpp
Surface.hpp
Surface.cpp
Swapchain.hpp
Swapchain.cpp
Time.hpp
Time.cpp
Vertex.hpp
Window.hpp
Window.cpp
Renderer.hpp
Renderer.cpp
PipelineConfig.hpp
PipelineBase.hpp
PipelineBase.cpp
DescriptorSet.hpp
DescriptorSet.cpp
GraphicsPipeline.hpp
GraphicsPipeline.cpp
ComputePipeline.hpp
ComputePipeline.cpp
CameraPackage.hpp
CameraPackage.cpp
TransformPackage.hpp
MeshPackage.hpp
TripleBuffer.hpp
Channel.hpp
)

target_link_libraries(lu_core PUBLIC
    # External dependencies
    Vulkan::Vulkan  # The imported target for Vulkan
    glm::glm        # The imported target for GLM
    glfw            # The imported target for GLFW (note: glfw, not glfw3)
    flecs::flecs
)