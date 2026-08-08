cmake_minimum_required(VERSION 3.10)

project(OpenGL_Assignment_1 VERSION 1.0 LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(OpenGL_GL_PREFERENCE GLVND)
cmake_policy(SET CMP0072 NEW)

include_directories(glad/include)
include_directories(shader)
include_directories(stb)
include_directories(camera)

set(SOURCES 
    main.cpp 
    glad/src/glad.c
    shader/shader.cpp
    camera/camera.h
)

file(COPY ${CMAKE_SOURCE_DIR}/09_Basic_Lighting DESTINATION ${CMAKE_BINARY_DIR})
file(COPY ${CMAKE_SOURCE_DIR}/Image DESTINATION ${CMAKE_BINARY_DIR})

add_executable(opengl_app ${SOURCES})

# Find packages (OpenGL và GLFW: libGL.so)
find_package(OpenGL REQUIRED)
# Add glfw director 
find_library(GLFW_LIBRARY glfw REQUIRED)

# Link libaries: GLFW: -lGL, OpenGL, dynamic linking
target_link_libraries(opengl_app 
    ${GLFW_LIBRARY}
    OpenGL::GL
    dl
)
