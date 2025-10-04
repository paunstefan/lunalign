cmake_minimum_required(VERSION 3.10)

project(lunalign CXX)

# set(CMAKE_C_COMPILER clang-19)
# set(CMAKE_CXX_COMPILER clang++-19)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_BUILD_TYPE Debug)

file(GLOB SOURCE_FILES "src/*.cpp")
add_executable(lunalign ${SOURCE_FILES})

target_include_directories(lunalign PRIVATE
    libs/include
)

target_link_directories(lunalign PRIVATE libs)

target_link_libraries(lunalign PRIVATE
    # 1. High-level OpenCV modules
    opencv_highgui
    opencv_imgcodecs

    # 2. Core processing OpenCV modules
    opencv_imgproc

    # 3. The foundational OpenCV module
    opencv_core

    # 4. OpenCV's third-party dependencies (Wrappers -> Core -> HAL)
    ipphal
    ippiw
    ippicv

    # 5. Other specific libraries
    cfitsio
    rtprocess

    # 6. System libraries (usually last)
    z
    dl
    pthread
    rt
)

if (CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    target_compile_options(lunalign PRIVATE "-stdlib=libc++")
    target_link_options(lunalign PRIVATE "-stdlib=libc++")
endif()

message(STATUS "CMake configuration complete. You can now build with 'make'.")