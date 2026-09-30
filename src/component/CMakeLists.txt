cmake_minimum_required(VERSION 3.16.3)

project(LCA)

add_library(lca_component STATIC
Placeholder.cpp
)

target_include_directories(lca_component PUBLIC ../core)
target_link_libraries(lca_component PUBLIC
    # External dependencies
    Vulkan::Vulkan  # The imported target for Vulkan
    glm::glm        # The imported target for GLM
    glfw            # The imported target for GLFW
    lu_core
)