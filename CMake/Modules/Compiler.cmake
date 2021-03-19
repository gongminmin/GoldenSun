add_definitions(-DUNICODE -D_UNICODE)

if(MSVC)
    set(CMAKE_CXX_FLAGS "/W4 /WX /EHsc /MP /bigobj /Zc:strictStrings /Zc:rvalueCast /Gw")

    set(CMAKE_CXX_STANDARD 17)

    if(CMAKE_C_COMPILER_ID MATCHES Clang)
        set(golden_sun_compiler_name "clangcl")
        set(golden_sun_compiler_clangcl TRUE)
        set(CMAKE_C_COMPILER ${ClangCL_Path}clang-cl.exe)
        set(CMAKE_CXX_COMPILER ${ClangCL_Path}clang-cl.exe)

        execute_process(COMMAND ${CMAKE_C_COMPILER} --version OUTPUT_VARIABLE CLANG_VERSION)
        string(REGEX MATCHALL "[0-9]+" clang_version_components ${CLANG_VERSION})
        list(GET clang_version_components 0 clang_major)
        list(GET clang_version_components 1 clang_minor)
        set(golden_sun_compiler_version ${clang_major}${clang_minor})
        if(golden_sun_compiler_version LESS "90")
            message(FATAL_ERROR "Unsupported compiler version. Please install clang-cl 9.0 or up.")
        endif()

        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /std:c++17")

        set(CMAKE_C_FLAGS "/W4 /WX /bigobj /Gw")
    else()
        set(golden_sun_compiler_name "vc")
        set(golden_sun_compiler_msvc TRUE)
        if(MSVC_VERSION GREATER_EQUAL 1920)
            set(golden_sun_compiler_version "142")
        else()
            message(FATAL_ERROR "Unsupported compiler version. Please install VS2019 or up.")
        endif()

        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /std:c++17 /Zc:throwingNew /permissive- /Zc:externConstexpr")
        set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} /JMC")
        foreach(flag_var
            CMAKE_CXX_FLAGS_RELEASE CMAKE_CXX_FLAGS_RELWITHDEBINFO CMAKE_CXX_FLAGS_MINSIZEREL)
            set(${flag_var} "${${flag_var}} /GS-")
        endforeach()
        
        foreach(flag_var
            CMAKE_EXE_LINKER_FLAGS CMAKE_SHARED_LINKER_FLAGS CMAKE_MODULE_LINKER_FLAGS)
            set(${flag_var} "/WX /pdbcompress")
        endforeach()
        foreach(flag_var
            CMAKE_EXE_LINKER_FLAGS_DEBUG CMAKE_SHARED_LINKER_FLAGS_DEBUG)
            set(${flag_var} "/DEBUG:FASTLINK")
        endforeach()
        foreach(flag_var
            CMAKE_EXE_LINKER_FLAGS_RELWITHDEBINFO CMAKE_SHARED_LINKER_FLAGS_RELWITHDEBINFO)
            set(${flag_var} "/DEBUG:FASTLINK /INCREMENTAL:NO /LTCG:incremental /OPT:REF /OPT:ICF")
        endforeach()
        foreach(flag_var
            CMAKE_EXE_LINKER_FLAGS_MINSIZEREL CMAKE_SHARED_LINKER_FLAGS_MINSIZEREL CMAKE_EXE_LINKER_FLAGS_RELEASE CMAKE_SHARED_LINKER_FLAGS_RELEASE)
            set(${flag_var} "/INCREMENTAL:NO /LTCG /OPT:REF /OPT:ICF")
        endforeach()
        foreach(flag_var
            CMAKE_MODULE_LINKER_FLAGS_RELEASE CMAKE_MODULE_LINKER_FLAGS_MINSIZEREL)
            set(${flag_var} "/INCREMENTAL:NO /LTCG")
        endforeach()
        foreach(flag_var
            CMAKE_STATIC_LINKER_FLAGS_RELEASE CMAKE_STATIC_LINKER_FLAGS_MINSIZEREL)
            set(${flag_var} "${${flag_var}} /LTCG")
        endforeach()
        set(CMAKE_STATIC_LINKER_FLAGS_RELWITHDEBINFO "${CMAKE_STATIC_LINKER_FLAGS_RELWITHDEBINFO} /LTCG:incremental")
        set(CMAKE_STATIC_LINKER_FLAGS "/WX")

        set(CMAKE_C_FLAGS ${CMAKE_CXX_FLAGS})
    endif()

    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /DGOLDEN_SUN_SHIP")
    foreach(flag_var
        CMAKE_CXX_FLAGS_RELEASE CMAKE_CXX_FLAGS_RELWITHDEBINFO CMAKE_CXX_FLAGS_MINSIZEREL)
        set(${flag_var} "${${flag_var}} /fp:fast /Ob2 /GL /Qpar")
    endforeach()

    add_definitions(-DWIN32 -D_WINDOWS)
endif()

set(CMAKE_C_FLAGS_DEBUG ${CMAKE_CXX_FLAGS_DEBUG})
set(CMAKE_C_FLAGS_RELEASE ${CMAKE_CXX_FLAGS_RELEASE})
set(CMAKE_C_FLAGS_RELWITHDEBINFO ${CMAKE_CXX_FLAGS_RELWITHDEBINFO})
set(CMAKE_C_FLAGS_MINSIZEREL ${CMAKE_CXX_FLAGS_MINSIZEREL})
if(golden_sun_compiler_msvc OR golden_sun_compiler_clangcl)
    set(rtti_flag "/GR")
    set(no_rtti_flag "/GR-")
else()
    set(rtti_flag "-frtti")
    set(no_rtti_flag "-fno-rtti")
endif()
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} ${rtti_flag}")
foreach(flag_var
    CMAKE_CXX_FLAGS_RELEASE CMAKE_CXX_FLAGS_RELWITHDEBINFO CMAKE_CXX_FLAGS_MINSIZEREL)
    set(${flag_var} "${${flag_var}} ${no_rtti_flag}")
endforeach()

set(golden_sun_output_suffix _${golden_sun_compiler_name}${golden_sun_compiler_version})

set(CMAKE_CXX_STANDARD_REQUIRED ON)

function(GoldenSunAddPrecompiledHeader target_name precompiled_header)
    if(golden_sun_compiler_msvc OR golden_sun_compiler_clangcl)
        if(CMAKE_GENERATOR MATCHES "^Visual Studio")
            GoldenSunAddMsvcPrecompiledHeader(${target_name} ${precompiled_header})
        endif()
    endif()
endfunction()

function(GoldenSunAddMsvcPrecompiledHeader target_name precompiled_header)
    get_filename_component(pch_base_name ${precompiled_header} NAME_WE)
    get_filename_component(pch_output "${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_CFG_INTDIR}/${pch_base_name}.pch" ABSOLUTE)

    get_filename_component(pch_header "${precompiled_header}" ABSOLUTE)

    get_target_property(source_list ${target_name} SOURCES)
    set(cpp_source_list "")
    foreach(file_name ${source_list})
        string(TOLOWER ${file_name} lower_file_name)
        string(FIND "${lower_file_name}" ".cpp" is_cpp REVERSE)
        if(is_cpp LESS 0)
            set_source_files_properties(${file_name} PROPERTIES COMPILE_FLAGS "/Y-")
        else()
            list(APPEND cpp_source_list "${file_name}")
            set_source_files_properties(${file_name} PROPERTIES COMPILE_FLAGS "/FI\"${pch_header}\"")
        endif()
    endforeach()
    list(GET cpp_source_list 0 first_cpp_file)
    set_source_files_properties(${first_cpp_file} PROPERTIES COMPILE_FLAGS "/Yc\"${pch_header}\" /FI\"${pch_header}\""
        OBJECT_OUTPUTS "${pch_output}")
    set_target_properties(${target_name} PROPERTIES COMPILE_FLAGS "/Yu\"${pch_header}\" /Fp\"${pch_output}\""
        OBJECT_DEPENDS "${pch_output}")
endfunction()
