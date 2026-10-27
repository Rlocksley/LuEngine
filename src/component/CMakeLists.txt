cmake_minimum_required(VERSION 3.16.3)

project(LCA)

add_library(lu_component STATIC
Component.cpp
Button.hpp
TextInput.hpp
FlyingCamera.hpp
CharacterCamera.hpp
)

target_include_directories(lu_component PUBLIC ../core)
target_link_libraries(lu_component PUBLIC
    # External dependencies
    Vulkan::Vulkan  # The imported target for Vulkan
    glm::glm        # The imported target for GLM
    glfw            # The imported target for GLFW
    lu_core
)
