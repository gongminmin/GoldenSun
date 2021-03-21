set(exe_name "GoldenSunApp")

set(source_files
    GoldenSunApp.cpp
    MeshLoader.cpp
    TextureLoader.cpp
    Main.cpp
    Timer.cpp
    WindowWin32.cpp
)

set(header_files
    GoldenSunApp.hpp
    MeshLoader.hpp
    TextureLoader.hpp
    pch.hpp
    Timer.hpp
    WindowWin32.hpp
)

source_group("Source Files" FILES ${source_files})
source_group("Header Files" FILES ${header_files})

add_executable(${exe_name} WIN32 ${source_files} ${header_files})

GoldenSunAddPrecompiledHeader(${exe_name} "pch.hpp")

set_target_properties(${exe_name} PROPERTIES
    PROJECT_LABEL ${exe_name}
    DEBUG_POSTFIX ${CMAKE_DEBUG_POSTFIX}
    OUTPUT_NAME ${exe_name}${golden_sun_output_suffix}
    FOLDER "Samples"
)

target_link_libraries(${exe_name}
    PRIVATE
        GoldenSunBase
        GoldenSunEngine
        d3d12 dxgi dxguid
        assimp stb
)
add_dependencies(${exe_name} CopyAssets)
