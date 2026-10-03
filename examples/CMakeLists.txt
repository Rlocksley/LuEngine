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

add_executable(lu_example4 main4.cpp)
target_include_directories(lu_example4 PUBLIC ../src/core ../src/shape ../src/app)
target_link_libraries(lu_example4 PRIVATE
    Vulkan::Vulkan
    glm::glm
    glfw
    lu_core
    lu_shape
    lu_app
    flecs::flecs
)