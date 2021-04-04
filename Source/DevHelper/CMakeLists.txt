set(lib_name "GoldenSunDevHelper")

set(source_files
    Source/MeshHelper.cpp
    Source/TextureHelper.cpp
)

set(header_files
    Include/GoldenSun/MeshHelper.hpp
    Include/GoldenSun/TextureHelper.hpp
)

set(internal_header_files
    Source/pch.hpp
)

source_group("Source Files" FILES ${source_files})
source_group("Header Files" FILES ${header_files})
source_group("Internal Header Files" FILES ${internal_header_files})

add_library(${lib_name} STATIC
    ${source_files} ${header_files} ${internal_header_files}
)

GoldenSunAddPrecompiledHeader(${lib_name} "Source/pch.hpp")

target_include_directories(${lib_name}
    PUBLIC
        Include
)

set_target_properties(${lib_name} PROPERTIES
    PROJECT_LABEL ${lib_name}
    DEBUG_POSTFIX ${CMAKE_DEBUG_POSTFIX}
    OUTPUT_NAME ${lib_name}${golden_sun_output_suffix}
    FOLDER "DevHelper"
)

target_link_libraries(${lib_name}
    PUBLIC
        GoldenSunBase
        d3d12 dxgi dxguid

    PRIVATE
        GoldenSunEngine
        assimp stb
)
