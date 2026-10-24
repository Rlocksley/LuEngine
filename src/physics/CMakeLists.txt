cmake_minimum_required(VERSION 3.16.3)

project(LCA)

add_library(lu_physics STATIC
Physics.hpp
Physics.cpp
PhysicsComponents.hpp
PhysicsModule.hpp
PhysicsDebugModule.hpp
CharacterCapsule.hpp
)

target_include_directories(lu_physics PUBLIC ../core)
target_link_libraries(lu_physics PUBLIC
    # External dependencies
    Vulkan::Vulkan  # The imported target for Vulkan
    glm::glm        # The imported target for GLM
    glfw            # The imported target for GLFW
    lu_core
    Jolt::Jolt      # The imported target for Jolt
)