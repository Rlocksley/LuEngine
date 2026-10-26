# LCA Example 1/CMakeLists.txt

# Set the minimum required version of CMake.
cmake_minimum_required(VERSION 3.15)

# Define the project name, version, and language. This will be the name of the .sln file.
project(LCA VERSION 1.0 LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED True)
find_package(flecs REQUIRED)
# --- Build The Main Executable ---
# Define the main executable target, "LCA Example 1".
add_executable(lu_example1 main3.cpp)

# --- Link All Libraries ---
# Link LCA Example 1 against all the libraries it needs. Modern CMake uses imported targets
# (like Vulkan::Vulkan, glm::glm, glfw) which also handle include directories automatically.
target_include_directories(lu_example1 PUBLIC ../src/core  ../src/shape ../src/app ../src/shape)

target_link_libraries(lu_example1 PRIVATE
    Vulkan::Vulkan  # The imported target for Vulkan
    glm::glm        # The imported target for GLM
    glfw            # The imported target for GLFW
    lu_core
    lu_shape
    lu_app
    flecs::flecs
)

add_executable(lu_example4 main17.cpp)
target_include_directories(lu_example4 PUBLIC ../src/core ../src/shape ../src/app ../src/physics)
target_link_libraries(lu_example4 PRIVATE
    Vulkan::Vulkan
    glm::glm
    glfw
    lu_core
    lu_shape
    lu_app
    lu_physics
    flecs::flecs
)

add_executable(lu_example5 main5.cpp)
target_include_directories(lu_example5 PUBLIC ../src/core ../src/shape ../src/app)
target_link_libraries(lu_example5 PRIVATE
    Vulkan::Vulkan
    glm::glm
    glfw
    lu_core
    lu_shape
    lu_app
    flecs::flecs
)

add_executable(lu_example6 main6.cpp)
target_include_directories(lu_example6 PUBLIC ../src/core ../src/shape ../src/app ../src)
target_link_libraries(lu_example6 PRIVATE
    Vulkan::Vulkan
    glm::glm
    glfw
    lu_core
    lu_shape
    lu_app
    flecs::flecs
)

add_executable(lu_example8 main8.cpp)
target_include_directories(lu_example8 PUBLIC ../src/core ../src/shape ../src/app ../src)
target_link_libraries(lu_example8 PRIVATE
    Vulkan::Vulkan
    glm::glm
    glfw
    lu_core
    lu_shape
    lu_app
    flecs::flecs
)

add_executable(lu_example9 main9.cpp)
target_include_directories(lu_example9 PUBLIC ../src/core ../src/shape ../src/app ../src)
target_link_libraries(lu_example9 PRIVATE
    Vulkan::Vulkan
    glm::glm
    glfw
    lu_core
    lu_shape
    lu_app
    flecs::flecs
)

add_executable(lu_example13 main13.cpp)
target_include_directories(lu_example13 PUBLIC ../src/core ../src/shape ../src/app ../src/physics)
target_link_libraries(lu_example13 PRIVATE
    Vulkan::Vulkan
    glm::glm
    glfw
    lu_core
    lu_shape
    lu_app
    lu_physics
    flecs::flecs
)

add_executable(lu_example14 main14.cpp)
target_include_directories(lu_example14 PUBLIC ../src/core ../src/shape ../src/app ../src/physics ../src)
target_link_libraries(lu_example14 PRIVATE
    Vulkan::Vulkan
    glm::glm
    glfw
    lu_core
    lu_shape
    lu_app
    lu_physics
    flecs::flecs
)

add_executable(lu_example15 main15.cpp)
target_include_directories(lu_example15 PUBLIC ../src/core ../src/shape ../src/app)
target_link_libraries(lu_example15 PRIVATE
    Vulkan::Vulkan
    glm::glm
    glfw
    lu_core
    lu_shape
    lu_app
    flecs::flecs
)

add_executable(lu_example16 main16.cpp)
target_include_directories(lu_example16 PUBLIC ../src/core ../src/shape ../src/app)
target_link_libraries(lu_example16 PRIVATE
    Vulkan::Vulkan
    glm::glm
    glfw
    lu_core
    lu_shape
    lu_app
    flecs::flecs
)
