cmake_minimum_required(VERSION 3.10)
project(ArtilleryTownDefense)

set(CMAKE_CXX_STANDARD 11)

# Fix warning CMP0072
set(OpenGL_GL_PREFERENCE GLVND)

# =========================================================================
# 1. CHAI3D
# =========================================================================
set(CHAI3D_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../Tool/chai3d" CACHE PATH "Path to CHAI3D directory")

# Find folder build 
if(EXISTS "${CHAI3D_DIR}/build/libchai3d.a")
    set(CHAI3D_LIBRARY "${CHAI3D_DIR}/build/libchai3d.a")
else()
    # fallback 
    find_library(CHAI3D_LIBRARY NAMES chai3d CHAI3D
        PATHS
        ${CHAI3D_DIR}/build
        ${CHAI3D_DIR}/build/lib
        ${CHAI3D_DIR}/build/lib/lin-x86_64
        ${CHAI3D_DIR}/bin/lin-x86_64
        /usr/local/lib
        /usr/lib
    )
endif()

# Find Header 
find_path(CHAI3D_INCLUDE_DIR NAMES chai3d.h
    PATHS
    ${CHAI3D_DIR}/src
    /usr/local/include/chai3d
    /usr/include/chai3d
)

# Print Terminal for debug
message(STATUS "-----------------------------------------------------")
message(STATUS "Path CHAI3D : ${CHAI3D_DIR}")
message(STATUS "Folder Header: ${CHAI3D_INCLUDE_DIR}")
message(STATUS "Library (.a) : ${CHAI3D_LIBRARY}")
message(STATUS "-----------------------------------------------------")

# Kiểm tra điều kiện tồn tại
if(NOT CHAI3D_INCLUDE_DIR OR NOT CHAI3D_LIBRARY OR CHAI3D_LIBRARY MATCHES "-NOTFOUND")
    message(FATAL_ERROR 
        "NOT FOUND CHAI3D!\n"
        "Waitting ... :\n"
        "cmake -B build -DCHAI3D_DIR=/your_path/chai3d ..\n"
    )
endif()

# =========================================================================
# 2. Link HEADERS & Library (OpenAL)
# =========================================================================
include_directories(${CHAI3D_INCLUDE_DIR})
include_directories(${CHAI3D_DIR}/externals/Eigen)
include_directories(${CHAI3D_DIR}/externals/glew/include)

# OpenAL 
find_package(OpenAL REQUIRED)
include_directories(${OPENAL_INCLUDE_DIR})

# OpenGL (GL & GLU)
find_package(OpenGL REQUIRED)

# GLFW3
find_package(glfw3 QUIET)
if(NOT glfw3_FOUND)
    find_package(GLFW3 REQUIRED)
endif()

# =========================================================================
# 3. Complie GAME
# =========================================================================
add_executable(artillery_game src/main.cpp)

target_link_libraries(artillery_game PRIVATE 
    ${CHAI3D_LIBRARY}
    OpenGL::GL
    OpenGL::GLU   
    glfw          
    ${OPENAL_LIBRARY} 
    asound        
    usb-1.0      
    rt            
    pthread       
    dl           
)
