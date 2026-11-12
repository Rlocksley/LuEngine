cmake_minimum_required(VERSION 3.16.3)

project(Lu)

add_library(lu_app STATIC
    App.cpp
)

target_include_directories(lu_app PUBLIC ../core ../component)
target_link_libraries(lu_app PUBLIC
    # External dependencies
    Vulkan::Vulkan  # The imported target for Vulkan
    glm::glm        # The imported target for GLM
    glfw            # The imported target for GLFW
)