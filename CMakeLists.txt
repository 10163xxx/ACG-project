cmake_minimum_required(VERSION 3.20)

project(ACGPathTracer LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(path_tracer
    src/main.cpp
)

target_include_directories(path_tracer PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

add_executable(test_materials
    tests/test_materials.cpp
)

target_include_directories(test_materials PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

enable_testing()

add_test(
    NAME MaterialTests
    COMMAND test_materials
)


add_executable(test_geometry
    tests/test_geometry.cpp
)

target_include_directories(test_geometry PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

add_test(
    NAME GeometryTests
    COMMAND test_geometry
)

add_executable(test_bvh
    tests/test_bvh.cpp
)

target_include_directories(test_bvh PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

add_test(
    NAME BVHTests
    COMMAND test_bvh
)

add_executable(benchmark_bvh
    tests/benchmark_bvh.cpp
)

target_include_directories(benchmark_bvh PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)
