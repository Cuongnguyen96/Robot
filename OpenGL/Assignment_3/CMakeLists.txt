cmake_minimum_required(VERSION 3.10)
project(ArtilleryTownDefenseWithOBB)

set(CMAKE_CXX_STANDARD 11)

# Fix warning CMP0072
set(OpenGL_GL_PREFERENCE GLVND)

# =========================================================================
# 1. AUTO-DETECT CHAI3D PATHS
# =========================================================================
if(NOT DEFINED CHAI3D_DIR)
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../Tool/chai3d/src/chai3d.h")
        set(CHAI3D_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../Tool/chai3d" CACHE PATH "Path to CHAI3D directory")
    elseif(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../../Tool/chai3d/src/chai3d.h")
        set(CHAI3D_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../Tool/chai3d" CACHE PATH "Path to CHAI3D directory")
    else()
        set(CHAI3D_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../Tool/chai3d" CACHE PATH "Path to CHAI3D directory")
    endif()
endif()

# Find the path containing main header 'chai3d.h'
find_path(CHAI3D_INCLUDE_DIR NAMES chai3d.h
    PATHS
    ${CHAI3D_DIR}/src
    /usr/local/include/chai3d
    /usr/include/chai3d
)

# Find static/compiled library '.a' of CHAI3D
find_library(CHAI3D_LIBRARY NAMES chai3d CHAI3D
    PATHS
    ${CHAI3D_DIR}/build
    ${CHAI3D_DIR}/build/lib
    ${CHAI3D_DIR}/build/lin-x86_64
    ${CHAI3D_DIR}/build/lib/lin-x86_64
    ${CHAI3D_DIR}/bin/lin-x86_64
    ${CHAI3D_DIR}/lib
    ${CHAI3D_DIR}/lib/lin-x86_64
    ${CHAI3D_DIR}/lib/release/lin-x86_64
    ${CHAI3D_DIR}/lib/debug/lin-x86_64
    ${CHAI3D_DIR}/lib/release/linux-x86_64
    ${CHAI3D_DIR}/lib/debug/linux-x86_64
    /usr/local/lib
    /usr/lib
)

# =========================================================================
# 2. HARDWARE DRIVERS DETECT (DHD & DRD) FOR HAPTIC DEVICES
# =========================================================================
find_library(DHD_LIBRARY NAMES dhd DHD
    PATHS
    ${CHAI3D_DIR}/externals/DHD/lib/lin-x86_64
    ${CHAI3D_DIR}/externals/DHD/lib/linux-x86_64
    ${CHAI3D_DIR}/externals/DHD/lib
    ${CHAI3D_DIR}/externals/DHD/lib/release/lin-x86_64
    ${CHAI3D_DIR}/externals/DHD/lib/debug/lin-x86_64
    ${CHAI3D_DIR}/externals/DHD/bin/lin-x86_64
    ${CHAI3D_DIR}/external/ForceDimension/lib/lin-x86_64
    ${CHAI3D_DIR}/external/ForceDimension/lib/linux-x86_64
    /usr/local/lib
    /usr/lib
)

find_library(DRD_LIBRARY NAMES drd DRD
    PATHS
    ${CHAI3D_DIR}/externals/DHD/lib/lin-x86_64
    ${CHAI3D_DIR}/externals/DHD/lib/linux-x86_64
    ${CHAI3D_DIR}/externals/DHD/lib
    ${CHAI3D_DIR}/externals/DHD/lib/release/lin-x86_64
    ${CHAI3D_DIR}/externals/DHD/lib/debug/lin-x86_64
    ${CHAI3D_DIR}/externals/DHD/bin/lin-x86_64
    ${CHAI3D_DIR}/external/ForceDimension/lib/lin-x86_64
    ${CHAI3D_DIR}/external/ForceDimension/lib/linux-x86_64
    /usr/local/lib
    /usr/lib
)

message(STATUS "-----------------------------------------------------")
message(STATUS "CHAI3D Directory: ${CHAI3D_DIR}")
message(STATUS "CHAI3D Include Path: ${CHAI3D_INCLUDE_DIR}")
message(STATUS "CHAI3D Static Lib: ${CHAI3D_LIBRARY}")
message(STATUS "Force Dimension DHD: ${DHD_LIBRARY}")
message(STATUS "Force Dimension DRD: ${DRD_LIBRARY}")
message(STATUS "-----------------------------------------------------")

if(NOT CHAI3D_INCLUDE_DIR OR NOT CHAI3D_LIBRARY)
    message(FATAL_ERROR "CHAI3D static libraries not found! Verify CHAI3D_DIR.")
endif()

# =========================================================================
# 3. HEADER DIRECTORIES
# =========================================================================
include_directories(${CHAI3D_INCLUDE_DIR})
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/src) # For OBB custom headers
include_directories(${CHAI3D_DIR}/externals/Eigen)
include_directories(${CHAI3D_DIR}/externals/glew/include)
include_directories(${CHAI3D_DIR}/externals/DHD/include)

# OpenAL 
find_package(OpenAL REQUIRED)
include_directories(${OPENAL_INCLUDE_DIR})

find_package(OpenGL REQUIRED)

# GLFW3
find_package(glfw3 QUIET)
if(NOT glfw3_FOUND)
    find_package(GLFW3 REQUIRED)
endif()


# =========================================================================
# 4. EXECUTABLE BUILD WITH THE NEW OBB CLASSES
# =========================================================================
# Source files containing the custom OBB and tree structure
set(SRC_FILES
    src/main.cpp
    src/collisions/CCollisionOBB.cpp
)

add_executable(artillery_game ${SRC_FILES})

# Setup hardware library dependencies list
set(ADDITIONAL_LIBS "")
if(DHD_LIBRARY)
    list(APPEND ADDITIONAL_LIBS ${DHD_LIBRARY})
# else()
#     list(APPEND ADDITIONAL_LIBS "dhd")
endif()

if(DRD_LIBRARY)
    list(APPEND ADDITIONAL_LIBS ${DRD_LIBRARY})
# else()
#     list(APPEND ADDITIONAL_LIBS "drd")
endif()

# Link everything together
target_link_libraries(artillery_game PRIVATE 
    ${CHAI3D_LIBRARY}
    ${ADDITIONAL_LIBS} # Force Dimension haptic driver linking
    OpenGL::GL
    OpenGL::GLU   # Resolves gluSphere, gluPerspective, etc.
    glfw
    ${OPENAL_LIBRARY} 
    asound        # Linux Sound ALSA Driver
    usb-1.0       # USB device hardware communication
    rt            # Real-time POSIX libraries
    pthread       # Multithreading libraries
    dl            # Dynamic Loader
    
)
