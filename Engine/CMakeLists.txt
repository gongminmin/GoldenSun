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

add_library(${lib_name} SHARED
    ${source_files} ${header_files} ${shader_files}
)

if(CMAKE_GENERATOR MATCHES "^Visual Studio")
    foreach(item ${shader_files})
        get_filename_component(item_base_name ${item} NAME_WE)
        set_source_files_properties(${item} PROPERTIES
            VS_SHADER_ENTRYPOINT ""
            VS_SHADER_DISABLE_OPTIMIZATIONS "$<CONFIG:DEBUG>"
            VS_SHADER_ENABLE_DEBUG "$<CONFIG:DEBUG>"
            VS_SHADER_TYPE "Library"
            VS_SHADER_MODEL "6.3"
            VS_SHADER_OBJECT_FILE_NAME ""
            VS_SHADER_VARIABLE_NAME "${item_base_name}_shader"
            VS_SHADER_OUTPUT_HEADER_FILE "${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_CFG_INTDIR}/CompiledShaders/${item_base_name}.h"
        )
    endforeach()
else()
    add_custom_target(CompileShaders)
    foreach(item ${shader_files})
        get_filename_component(item_base_name ${item} NAME_WE)
        if (CMAKE_BUILD_TYPE STREQUAL "Debug")
            set(additional_options -Zi -Od)
        else()
            set(additional_options)
        endif()
        set(input_name "${CMAKE_CURRENT_SOURCE_DIR}/${item}")
        set(output_name "${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_CFG_INTDIR}/CompiledShaders/${item_base_name}.h")
        add_custom_target(Compile_${item_base_name} ALL
            COMMAND dxc ${additional_options} -T lib_6_3 -Vn ${item_base_name}_shader -Fh \"${output_name}\" /nologo \"${input_name}\"
            COMMENT "Compiling ${item}"
        )
        add_dependencies(CompileShaders Compile_${item_base_name})
    endforeach()

    add_dependencies(${lib_name} CompileShaders)
endif()

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
