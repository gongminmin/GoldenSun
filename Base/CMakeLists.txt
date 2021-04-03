set(lib_name "GoldenSunBase")

set(source_files
    Source/ErrorHandling.cpp
    Source/Util.cpp
)

set(header_files
    Include/GoldenSun/Base.hpp
    Include/GoldenSun/ComPtr.hpp
    Include/GoldenSun/ErrorHandling.hpp
    Include/GoldenSun/ImplPtr.hpp
    Include/GoldenSun/SmartPtrHelper.hpp
    Include/GoldenSun/Util.hpp
    Include/GoldenSun/Uuid.hpp
)

set(internal_header_files
    Source/ImplPtrImpl.hpp
    Source/pch.hpp
)

set(gpu_source_files
    Source/ErrorHandling.cpp
    Source/Util.cpp
    Source/Gpu/GpuBuffer.cpp
    Source/Gpu/GpuCommandList.cpp
    Source/Gpu/GpuDescriptorAllocator.cpp
    Source/Gpu/GpuDescriptorHeap.cpp
    Source/Gpu/GpuMemoryAllocator.cpp
    Source/Gpu/GpuResourceViews.cpp
    Source/Gpu/GpuSwapChain.cpp
    Source/Gpu/GpuSystem.cpp
    Source/Gpu/GpuTexture2D.cpp
)

set(gpu_header_files
    Include/GoldenSun/Gpu/GpuBuffer.hpp
    Include/GoldenSun/Gpu/GpuBufferHelper.hpp
    Include/GoldenSun/Gpu/GpuCommandList.hpp
    Include/GoldenSun/Gpu/GpuDescriptorAllocator.hpp
    Include/GoldenSun/Gpu/GpuDescriptorHeap.hpp
    Include/GoldenSun/Gpu/GpuMemoryAllocator.hpp
    Include/GoldenSun/Gpu/GpuResourceViews.hpp
    Include/GoldenSun/Gpu/GpuSwapChain.hpp
    Include/GoldenSun/Gpu/GpuSystem.hpp
    Include/GoldenSun/Gpu/GpuSystemD3D12.hpp
    Include/GoldenSun/Gpu/GpuTexture2D.hpp
)

set(gpu_internal_header_files
    Source/Gpu/GpuSystemInternal.hpp
)

source_group("Source Files" FILES ${source_files})
source_group("Header Files" FILES ${header_files})
source_group("Internal Header Files" FILES ${internal_header_files})
source_group("Source Files\\Gpu" FILES ${gpu_source_files})
source_group("Header Files\\Gpu" FILES ${gpu_header_files})
source_group("Internal Header Files\\Gpu" FILES ${gpu_internal_header_files})

add_library(${lib_name} STATIC
    ${source_files} ${header_files} ${internal_header_files}
    ${gpu_source_files} ${gpu_header_files} ${gpu_internal_header_files}
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
    FOLDER "Base"
)

target_link_libraries(${lib_name}
    PUBLIC
        d3d12 dxgi dxguid
)
