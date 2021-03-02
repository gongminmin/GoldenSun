set(lib_name "GoldenSunEngine")

set(source_files
    Source/ErrorHandling.cpp
    Source/GoldenSunEngine.cpp
    Source/Util.cpp
)

set(header_files
    Include/GoldenSun/Base.hpp
    Include/GoldenSun/ComPtr.hpp
    Include/GoldenSun/ErrorHandling.hpp
    Include/GoldenSun/GoldenSunEngine.hpp
    Include/GoldenSun/SmartPtrHelper.hpp
    Include/GoldenSun/Util.hpp
    Include/GoldenSun/Uuid.hpp
    Source/pch.hpp
)

set(shader_files
    Source/RayTracing.hlsl
)

source_group("Source Files" FILES ${source_files})
source_group("Header Files" FILES ${header_files})
source_group("Shader Files" FILES ${shader_files})

set_source_files_properties(Source/RayTracing.hlsl PROPERTIES
    VS_SHADER_ENTRYPOINT ""
    VS_SHADER_DISABLE_OPTIMIZATIONS "$<CONFIG:DEBUG>"
    VS_SHADER_ENABLE_DEBUG "$<CONFIG:DEBUG>"
    VS_SHADER_TYPE "Library"
    VS_SHADER_MODEL "6.3"
    VS_SHADER_OBJECT_FILE_NAME ""
    VS_SHADER_VARIABLE_NAME "%(Filename)_shader"
    VS_SHADER_OUTPUT_HEADER_FILE "${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_CFG_INTDIR}/CompiledShaders/%(Filename).h"
)

add_library(${lib_name} SHARED
    ${source_files} ${header_files} ${shader_files}
)

target_compile_definitions(${lib_name}
    PRIVATE
        -DGOLDEN_SUN_SOURCE
)

target_include_directories(${lib_name}
    PUBLIC
        Include

    PRIVATE
        ${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_CFG_INTDIR}
)

set_target_properties(${lib_name} PROPERTIES
    PROJECT_LABEL ${lib_name}
    DEBUG_POSTFIX ${CMAKE_DEBUG_POSTFIX}
    OUTPUT_NAME ${lib_name}${golden_sun_output_suffix}
    FOLDER "Engine"
)

GoldenSunAddPrecompiledHeader(${lib_name} "Source/pch.hpp")

target_link_libraries(${lib_name}
    PUBLIC
        d3d12 dxguid
)
