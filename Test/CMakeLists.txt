set(exe_name GoldenSunTest)

set(source_files
    GoldenSunTest.cpp
    TestFrameworkTest.cpp
)

set(header_files
    GoldenSunTest.hpp
)

set(shader_files
    Comparator.hlsl
)

source_group("Source Files" FILES ${source_files})
source_group("Header Files" FILES ${header_files})
source_group("Shader Files" FILES ${shader_files})
source_group("Expected Files" FILES ${expected_files})

GoldenSunAddShaderFile("Comparator.hlsl" "cs" "CompareImagesCS")

add_executable(${exe_name}
    ${source_files} ${header_files} ${shader_files} ${expected_files}
)

target_include_directories(${exe_name}
    PRIVATE
        ${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_CFG_INTDIR}
)

target_compile_definitions(${exe_name}
    PRIVATE
        -DEXPECTED_DIR="${CMAKE_CURRENT_SOURCE_DIR}/Expected/"
)

target_link_libraries(${exe_name}
    PRIVATE
        GoldenSunEngine
        d3d12 dxgi dxguid
        gtest stb
)

set_target_properties(${exe_name} PROPERTIES FOLDER "Test")
