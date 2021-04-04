set(expected_dir "${CMAKE_CURRENT_SOURCE_DIR}/Expected/")
set(expected_deploy_dir "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${CMAKE_CFG_INTDIR}/Test/Expected/")
add_custom_target(CopyExpecteds ALL
    COMMAND ${CMAKE_COMMAND} -E copy_directory "${expected_dir}" "${expected_deploy_dir}"
    COMMENT "Copying \"${expected_dir}\" to \"${expected_deploy_dir}\""
)

set_target_properties(CopyExpecteds PROPERTIES
    FOLDER "Test"
)

set(exe_name GoldenSunTest)

set(source_files
    GoldenSunTest.cpp
    RayCastingTest.cpp
    TestFrameworkTest.cpp
)

set(header_files
    GoldenSunTest.hpp
    pch.hpp
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

GoldenSunAddPrecompiledHeader(${exe_name} "pch.hpp")

target_include_directories(${exe_name}
    PRIVATE
        ${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_CFG_INTDIR}
)

target_link_libraries(${exe_name}
    PRIVATE
        GoldenSunBase
        GoldenSunEngine
        GoldenSunDevHelper
        d3d12 dxgi dxguid
        gtest
)

add_dependencies(${exe_name} CopyExpecteds CopyAssets)

set_target_properties(${exe_name} PROPERTIES FOLDER "Test")
