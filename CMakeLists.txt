cmake_minimum_required(VERSION 3.10)
project(vulk)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find Vulkan SDK
find_package(Vulkan REQUIRED)

# GLFW for window + surface creation
find_package(glfw3 3.3 REQUIRED)

# Include directories
include_directories(
    ${CMAKE_SOURCE_DIR}/include
)

# Source files
add_executable(vulk
    src/main.cpp
)

# Link libraries
target_link_libraries(vulk
    Vulkan::Vulkan
    glfw
    dl
    pthread
    X11
    Xxf86vm
    Xrandr
    Xi
)
