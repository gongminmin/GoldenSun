set(golden_sun_cmake_module_dir "${CMAKE_CURRENT_SOURCE_DIR}/CMake/Modules")
list(APPEND CMAKE_MODULE_PATH "${golden_sun_cmake_module_dir}")

project(GoldenSun)

cmake_minimum_required(VERSION 3.18 FATAL_ERROR)

set(golden_sun_major_version 0)
set(golden_sun_minor_version 1)
set(golden_sun_patch_version 0)
set(golden_sun_version ${golden_sun_major_version}.${golden_sun_minor_version}.${golden_sun_patch_version})

include(${golden_sun_cmake_module_dir}/Common.cmake)
include(${golden_sun_cmake_module_dir}/Platform.cmake)
include(${golden_sun_cmake_module_dir}/Compiler.cmake)

set_property(GLOBAL PROPERTY USE_FOLDERS ON)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/Bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/Lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/Lib)

add_subdirectory(Engine)
add_subdirectory(Samples)
