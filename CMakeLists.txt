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
    libs/cfitsio-cfitsio-4.6.2/
    libs/librtprocess-0.12.0/src/include
)

target_link_directories(lunalign PRIVATE libs)

target_link_libraries(lunalign PRIVATE
    cfitsio
    rtprocess
    z
)

if (CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    target_compile_options(lunalign PRIVATE "-stdlib=libc++")
    target_link_options(lunalign PRIVATE "-stdlib=libc++")
endif()

message(STATUS "CMake configuration complete. You can now build with 'make'.")