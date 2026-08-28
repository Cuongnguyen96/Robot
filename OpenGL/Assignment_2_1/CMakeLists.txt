cmake_minimum_required(VERSION 3.10)
project(ArtilleryTownDefense)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g -rdynamic")

set(CMAKE_CXX_STANDARD 11)

set(OpenGL_GL_PREFERENCE GLVND)

# =========================================================================
# 1. TỰ ĐỘNG DÒ TÌM ĐƯỜNG DẪN THƯ VIỆN CHAI3D (Auto-Detect Relative Paths)
# =========================================================================
# Đề phòng trường hợp thư mục 'Tool' nằm ở cấp cha khác nhau:
# - Nếu game ở: OpenGL/Assignment_2_1/ và Tool ở: OpenGL/Tool/ -> ../Tool/chai3d
# - Nếu game ở: OpenGL/Assignment_2_1/ và Tool ở: Robot/Tool/   -> ../../Tool/chai3d
if(NOT DEFINED CHAI3D_DIR)
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../Tool/chai3d/src/chai3d.h")
        set(CHAI3D_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../Tool/chai3d" CACHE PATH "Path to CHAI3D directory")
    elseif(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../../Tool/chai3d/src/chai3d.h")
        set(CHAI3D_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../Tool/chai3d" CACHE PATH "Path to CHAI3D directory")
    else()
        # Fallback về mặc định ban đầu
        set(CHAI3D_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../Tool/chai3d" CACHE PATH "Path to CHAI3D directory")
    endif()
endif()

# Tự động tìm kiếm thư mục chứa file header chính 'chai3d.h'
find_path(CHAI3D_INCLUDE_DIR NAMES chai3d.h
    PATHS
    ${CHAI3D_DIR}/src
    /usr/local/include/chai3d
    /usr/include/chai3d
)

# Tự động tìm kiếm file thư viện tĩnh '.a' đã build.
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

# -------------------------------------------------------------------------
# SỬA LỖI LỚN: Khai báo chính xác các đường dẫn chứa driver DHD/DRD trên Linux
# -------------------------------------------------------------------------
set(DHD_LIB_SEARCH_PATHS
    ${CHAI3D_DIR}/externals/DHD/lib/lin-x86_64
    ${CHAI3D_DIR}/externals/DHD/lib/linux-x86_64
    ${CHAI3D_DIR}/externals/DHD/lib/lin-aarch64
    ${CHAI3D_DIR}/externals/DHD/lib/lin-armv7
    ${CHAI3D_DIR}/externals/DHD/lib
    ${CHAI3D_DIR}/external/ForceDimension/lib/lin-x86_64
    /usr/local/lib
    /usr/lib
)

# GIẢI PHÁP PHÒNG THỦ 2 LỚP: Đăng ký trực tiếp các đường dẫn này cho Linker (ld)
link_directories(${DHD_LIB_SEARCH_PATHS})

# DÒ TÌM THƯ VIỆN BỔ SUNG FORCE DIMENSION
# Điểm mấu chốt: Trong hệ thống CHAI3D/Force Dimension trên Linux, thư viện này được đặt tên là
# 'dhdc' và 'drdc' (DHD C API / DRD C API) thay vì 'dhd' và 'drd'!
find_library(DHD_LIBRARY NAMES dhdc dhd DHD DHDC PATHS ${DHD_LIB_SEARCH_PATHS})
find_library(DRD_LIBRARY NAMES drdc drd DRD DRDC PATHS ${DHD_LIB_SEARCH_PATHS})

# --- DÒNG IN THÔNG TIN CHẨN ĐOÁN ---
message(STATUS "-----------------------------------------------------")
message(STATUS "Đường dẫn CHAI3D tìm kiếm: ${CHAI3D_DIR}")
message(STATUS "Thư mục chứa Header tìm thấy: ${CHAI3D_INCLUDE_DIR}")
message(STATUS "Tệp thư viện CHAI3D (.a): ${CHAI3D_LIBRARY}")
message(STATUS "Tệp thư viện DHD (.so/.a): ${DHD_LIBRARY}")
message(STATUS "Tệp thư viện DRD (.so/.a): ${DRD_LIBRARY}")
message(STATUS "-----------------------------------------------------")

# Kiểm tra nếu không tìm thấy thư viện thì báo lỗi rõ ràng hướng dẫn cách xử lý
if(NOT CHAI3D_INCLUDE_DIR OR NOT CHAI3D_LIBRARY)
    message(FATAL_ERROR "Không tìm thấy thư viện CHAI3D!")
endif()

# =========================================================================
# 2. LIÊN KẾT ĐƯỜNG DẪN HEADERS
# =========================================================================
include_directories(${CHAI3D_INCLUDE_DIR})
include_directories(${CHAI3D_DIR}/externals/Eigen)
include_directories(${CHAI3D_DIR}/externals/glew/include)
include_directories(${CHAI3D_DIR}/externals/DHD/include)

# Tìm OpenGL và GLFW3 từ hệ thống
find_package(OpenGL REQUIRED)

# GLFW3
find_package(glfw3 QUIET)
if(NOT glfw3_FOUND)
    find_package(GLFW3 REQUIRED)
endif()

# =========================================================================
# 3. BIÊN DỊCH GAME (Chỉ định file nguồn main.cpp / main-v18.cpp)
# =========================================================================
add_executable(artillery_game main.cpp)

# Thiết lập các thư viện bổ sung
set(ADDITIONAL_LIBS "")

if(DHD_LIBRARY)
    list(APPEND ADDITIONAL_LIBS ${DHD_LIBRARY})
# else()
#     list(APPEND ADDITIONAL_LIBS "dhdc") # Fallback liên kết tên chuẩn 'dhdc'
endif()

if(DRD_LIBRARY)
    list(APPEND ADDITIONAL_LIBS ${DRD_LIBRARY})
# else()
#     list(APPEND ADDITIONAL_LIBS "drdc") # Fallback liên kết tên chuẩn 'drdc'
endif()

target_link_libraries(artillery_game PRIVATE 
    ${CHAI3D_LIBRARY}      # 1. Thư viện chính CHAI3D (gọi hàm của DRD)
    ${ADDITIONAL_LIBS}     # 2. Driver DRD (libdrd.a) (gọi hàm của USB và Threads)
    OpenGL::GL             # 3. Đồ họa
    OpenGL::GLU            # 4. Tiện ích OpenGL
    glfw                   # 5. GLFW Window
    usb-1.0                # 6. BẮT BUỘC ĐẶT SAU DRD: Sửa hoàn toàn lỗi 'undefined reference to libusb_*'
    asound                 # 7. ALSA Audio
    rt                     # 8. Real-time extension
    pthread                # 9. Thư viện đa luồng
    dl                     # 10. Thư viện Dynamic Linker
)