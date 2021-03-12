set(lib_name "GoldenSunEngine")

set(source_files
    Source/AccelerationStructure.cpp
    Source/ErrorHandling.cpp
    Source/Engine.cpp
    Source/Mesh.cpp
    Source/Util.cpp
)

set(header_files
    Include/GoldenSun/Base.hpp
    Include/GoldenSun/ComPtr.hpp
    Include/GoldenSun/ErrorHandling.hpp
    Include/GoldenSun/Engine.hpp
    Include/GoldenSun/GoldenSun.hpp
    Include/GoldenSun/Mesh.hpp
    Include/GoldenSun/SmartPtrHelper.hpp
    Include/GoldenSun/Util.hpp
    Include/GoldenSun/Uuid.hpp
)

set(internal_header_files
    Source/AccelerationStructure.hpp
    Source/pch.hpp
)

set(shader_files
    Source/RayTracing.hlsl
)

source_group("Source Files" FILES ${source_files})
source_group("Header Files" FILES ${header_files})
source_group("Internal Header Files" FILES ${internal_header_files})
source_group("Shader Files" FILES ${shader_files})

add_library(${lib_name} SHARED
    ${source_files} ${header_files} ${internal_header_files} ${shader_files}
)

GoldenSunAddShaderFile(Source/RayTracing.hlsl "lib" "")

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
