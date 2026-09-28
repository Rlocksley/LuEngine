# Rx/CMakeLists.txt

# Set the minimum required version of CMake.
cmake_minimum_required(VERSION 3.15)

# Define the project name, version, and language. This will be the name of the .sln file.
project(Lu VERSION 1.0 LANGUAGES CXX)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE "RelWithDebInfo" CACHE STRING "Choose the type of build." FORCE)
  set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS "Debug" "Release" "MinSizeRel" "RelWithDebInfo")
endif()

# --- Find External Dependencies ---
# Use find_package to locate the necessary libraries. CMake will fail if they are not found.
# For this to work, you must either install these libraries to a system path
# or tell CMake where to find them using CMAKE_PREFIX_PATH (see instructions below).

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED True)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Find the Vulkan SDK. The official SDK sets an environment variable that CMake uses.
find_package(Vulkan REQUIRED)

# Find the GLFW3 package configuration installed by vcpkg.
find_package(glfw3 CONFIG REQUIRED)

# Find the GLM mathematics library.
find_package(glm REQUIRED)

find_package(flecs REQUIRED)

#find_package(Jolt REQUIRED)

#find_package(assimp REQUIRED)

#find_package(recastnavigation CONFIG REQUIRED)

#find_package(Freetype REQUIRED)

# --- Build Internal Libraries ---
# Process the subdirectories to create our 'component_lib' and 'core_lib' targets.
add_subdirectory(./src/core)
#add_subdirectory(./src/component)
add_subdirectory(./src/shape)
add_subdirectory(./src/app)
#add_subdirectory(./asset)

#add_subdirectory(./converter)
#add_subdirectory(./analyzer)

add_subdirectory(./examples)
#add_subdirectory(./examples/example2)
#add_subdirectory(./examples/example3)
#add_subdirectory(./examples/example4)
#add_subdirectory(./examples/example5)
#add_subdirectory(./examples/example6)
#add_subdirectory(./examples/example7)
#add_subdirectory(./examples/example8)
#add_subdirectory(./examples/example9)
#add_subdirectory(./examples/example10)
#add_subdirectory(./examples/example11)
#add_subdirectory(./examples/example12)
#add_subdirectory(./examples/example13)