cmake_minimum_required(VERSION 3.16.3)

project(LCA)

add_library(lu_module STATIC
Module.cpp
UIModule.hpp
UIDetail.hpp
UICoreModule.hpp
UIBasicWidgets.hpp
UIContainerWidgets.hpp
)

target_include_directories(lu_module PUBLIC ../core ../component)
target_link_libraries(lu_module PUBLIC
    # External dependencies
    Vulkan::Vulkan  # The imported target for Vulkan
    glm::glm        # The imported target for GLM
    glfw            # The imported target for GLFW
    lu_core
    lu_component
)
