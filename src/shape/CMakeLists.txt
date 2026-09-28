cmake_minimum_required(VERSION 3.16.3)

project(LCA)

add_library(lu_shape STATIC
    Shape.hpp
    Shape.cpp
)

target_include_directories(lu_shape PUBLIC ../core)
target_link_libraries(lu_shape PUBLIC
    # External dependencies
    Vulkan::Vulkan  # The imported target for Vulkan
    glm::glm        # The imported target for GLM
    glfw            # The imported target for GLFW (note: glfw, not glfw3)
    flecs::flecs
)