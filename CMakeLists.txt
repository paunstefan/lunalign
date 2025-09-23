cmake_minimum_required(VERSION 3.10)

project(lunalign CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

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

message(STATUS "CMake configuration complete. You can now build with 'make'.")