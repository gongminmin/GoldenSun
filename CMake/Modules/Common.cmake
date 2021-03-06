set(CMAKE_DEBUG_POSTFIX "_d" CACHE STRING "Add a postfix, usually _d on windows")
set(CMAKE_RELEASE_POSTFIX "" CACHE STRING "Add a postfix, usually empty on windows")
set(CMAKE_RELWITHDEBINFO_POSTFIX "" CACHE STRING "Add a postfix, usually empty on windows")
set(CMAKE_MINSIZEREL_POSTFIX "" CACHE STRING "Add a postfix, usually empty on windows")

macro(GoldenSunAddShaderFile file_name shader_type entry_point)
    get_filename_component(file_base_name ${file_name} NAME_WE)
    set(variable_name ${file_base_name}_shader)
    set(output_name "${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_CFG_INTDIR}/CompiledShaders/${file_base_name}.h")

    if(CMAKE_GENERATOR MATCHES "^Visual Studio")
        if(${shader_type} STREQUAL "vs")
            set(shader_type_name "Vertex")
        elseif(${shader_type} STREQUAL "ps")
            set(shader_type_name "Pixel")
        elseif(${shader_type} STREQUAL "gs")
            set(shader_type_name "Geometry")
        elseif(${shader_type} STREQUAL "hs")
            set(shader_type_name "Hull")
        elseif(${shader_type} STREQUAL "ds")
            set(shader_type_name "Domain")
        elseif(${shader_type} STREQUAL "cs")
            set(shader_type_name "Compute")
        elseif(${shader_type} STREQUAL "lib")
            set(shader_type_name "Library")
        endif()

        set_source_files_properties(${file_name} PROPERTIES
            VS_SHADER_ENTRYPOINT "${entry_point}"
            VS_SHADER_DISABLE_OPTIMIZATIONS "$<CONFIG:DEBUG>"
            VS_SHADER_ENABLE_DEBUG "$<CONFIG:DEBUG>"
            VS_SHADER_TYPE "${shader_type_name}"
            VS_SHADER_MODEL "6.3"
            VS_SHADER_OBJECT_FILE_NAME ""
            VS_SHADER_VARIABLE_NAME "${variable_name}"
            VS_SHADER_OUTPUT_HEADER_FILE "${output_name}"
        )
    else()
        if (CMAKE_BUILD_TYPE STREQUAL "Debug")
            set(additional_options -Zi -Od)
        else()
            set(additional_options)
        endif()
        set(input_name "${CMAKE_CURRENT_SOURCE_DIR}/${file_name}")
        add_custom_target(Compile_${file_base_name} ALL
            COMMAND dxc ${additional_options} -T ${shader_type}_6_3 -Vn ${variable_name} -E ${entry_point} -Fh \"${output_name}\" /nologo \"${input_name}\"
            COMMENT "Compiling ${file_name}"
        )
        add_dependencies(${lib_name} Compile_${file_base_name})
    endif()
endmacro()
