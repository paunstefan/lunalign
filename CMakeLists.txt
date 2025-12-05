cmake_minimum_required(VERSION 3.10)

project(lunalign CXX C)

# --- 1. Global Compiler Setup ---
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_BUILD_TYPE Debug)

if (CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    add_compile_options($<$<COMPILE_LANGUAGE:CXX>:-stdlib=libc++>)
    add_link_options($<$<COMPILE_LANGUAGE:CXX>:-stdlib=libc++>)
endif()

# --- 2. Configure & Build Dependencies ---

# A. Configure cfitsio
set(TESTS OFF CACHE BOOL "Disable cfitsio tests" FORCE)

# 2. Disable utilities (fpack, funpack) to save build time
set(UTILS OFF CACHE BOOL "Disable cfitsio utilities" FORCE)

set(BUILD_SHARED_LIBS OFF CACHE BOOL "Build static libs" FORCE)
set(USE_PTHREADS ON CACHE BOOL "Use Pthreads" FORCE)
# Prevent cfitsio from trying to install itself to system directories during your local build
set(SKIP_INSTALL_ALL ON CACHE BOOL "Skip install" FORCE) 

add_subdirectory(libs/cfitsio-cfitsio-4.6.2)

target_include_directories(cfitsio PUBLIC 
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/libs/cfitsio-cfitsio-4.6.2>
)

# B. Configure librtprocess
set(OPTION_OMP OFF CACHE BOOL "Disable OpenMP" FORCE)
# Note: It will inherit the -stdlib=libc++ defined in section 1 automatically
add_subdirectory(libs/librtprocess-0.12.0)


# C. Configure OpenCV
# Limit the build to only what you need to save massive amounts of compile time
set(BUILD_LIST "core,imgproc,imgcodecs,highgui" CACHE STRING "Requested modules" FORCE)
set(WITH_ITT OFF CACHE BOOL "Disable ITT" FORCE)

# Turn off extra OpenCV bloat
set(BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(BUILD_PERF_TESTS OFF CACHE BOOL "" FORCE)
set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(BUILD_opencv_apps OFF CACHE BOOL "" FORCE)
set(BUILD_JAVA OFF CACHE BOOL "" FORCE)
set(BUILD_PYTHON OFF CACHE BOOL "" FORCE)

add_subdirectory(libs/opencv-4.12.0)

if(TARGET ipphal)
    target_include_directories(ipphal PRIVATE 
        ${CMAKE_CURRENT_SOURCE_DIR}/libs/opencv-4.12.0/modules/core/include
    )
endif()

# --- 3. Build Your Project ---

file(GLOB SOURCE_FILES "src/*.cpp")
add_executable(lunalign ${SOURCE_FILES})

# --- 4. Linking ---

set(OCV_DIR ${CMAKE_CURRENT_SOURCE_DIR}/libs/opencv-4.12.0)
set(OCV_BUILD_DIR ${CMAKE_CURRENT_BINARY_DIR})

target_include_directories(lunalign PRIVATE
    libs/include

    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/libs/cfitsio-cfitsio-4.6.2>

    ${OCV_DIR}/modules/core/include
    ${OCV_DIR}/modules/imgproc/include
    ${OCV_DIR}/modules/imgcodecs/include
    ${OCV_DIR}/modules/highgui/include

    ${OCV_BUILD_DIR}
    ${OCV_BUILD_DIR}/include
)

target_link_libraries(lunalign PRIVATE
    opencv_highgui
    opencv_imgcodecs

    opencv_imgproc

    opencv_core

    cfitsio
    rtprocess

    z
    dl
    pthread
    rt
)