cmake_minimum_required(VERSION 3.18 FATAL_ERROR)

set(golden_sun_cmake_module_dir "${CMAKE_CURRENT_SOURCE_DIR}/CMake/Modules")
list(APPEND CMAKE_MODULE_PATH "${golden_sun_cmake_module_dir}")

if(POLICY CMP0048)
    cmake_policy(SET CMP0048 NEW)
endif()

set(golden_sun_major_version 0)
set(golden_sun_minor_version 1)
set(golden_sun_patch_version 0)
set(golden_sun_version ${golden_sun_major_version}.${golden_sun_minor_version}.${golden_sun_patch_version})

project(GoldenSun VERSION ${golden_sun_version})

if(golden_sun_clang_format)
    find_program(clang_format
        NAMES
            clang-format-9
            clang-format
    )

    if(${clang_format} STREQUAL "CLANG_FORMAT-NOTFOUND")
        message(FATAL_ERROR "No clang-format tool found")
    endif()

    message(STATUS "Generating clang-format target using ${clang_format}")
    file(GLOB_RECURSE all_source_files *.cpp *.h *.hpp)
    add_custom_target(clang_format
        COMMAND ${clang_format} -i ${all_source_files})

    return()
endif()

include(${golden_sun_cmake_module_dir}/Common.cmake)
include(${golden_sun_cmake_module_dir}/Platform.cmake)
include(${golden_sun_cmake_module_dir}/Compiler.cmake)

set_property(GLOBAL PROPERTY USE_FOLDERS ON)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/Bin")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/Lib")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/Lib")

add_subdirectory(External)
add_subdirectory(Base)
add_subdirectory(Engine)
add_subdirectory(Samples)
add_subdirectory(Test)
