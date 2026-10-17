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

add_executable(test_textures
    tests/test_textures.cpp
)

target_include_directories(test_textures PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

add_test(
    NAME TextureTests
    COMMAND test_textures
)

add_executable(test_path_tracer
    tests/test_path_tracer.cpp
)

target_include_directories(test_path_tracer PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

add_test(
    NAME PathTracerTests
    COMMAND test_path_tracer
)

add_executable(test_sphere_sampling
    tests/test_sphere_sampling.cpp
)

target_include_directories(test_sphere_sampling PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

add_test(
    NAME SphereSamplingTests
    COMMAND test_sphere_sampling
)

add_executable(test_nee
    tests/test_nee.cpp
)

target_include_directories(test_nee PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

add_test(
    NAME NEETests
    COMMAND test_nee
)
add_executable(test_normal_mapping
    tests/test_normal_mapping.cpp
)

target_include_directories(test_normal_mapping PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

add_test(
    NAME NormalMappingTests
    COMMAND test_normal_mapping
)
add_executable(test_mesh
    tests/test_mesh.cpp
)

target_include_directories(test_mesh PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

add_test(
    NAME MeshTests
    COMMAND test_mesh
)

add_executable(test_mis
    tests/test_mis.cpp
)

target_include_directories(test_mis PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

add_test(
    NAME MISTests
    COMMAND test_mis
)
