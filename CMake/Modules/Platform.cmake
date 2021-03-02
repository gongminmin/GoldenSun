if(WIN32)
    if(MSVC AND (CMAKE_GENERATOR MATCHES "^Visual Studio"))
        if((CMAKE_GENERATOR_PLATFORM STREQUAL "x64") OR (CMAKE_GENERATOR MATCHES "Win64") OR (CMAKE_GENERATOR_PLATFORM STREQUAL ""))
            set(golden_sun_arch_name "x64")
            set(golden_sun_vs_platform_name "x64")
        else()
            message(FATAL_ERROR "This CPU architecture is not supported")
        endif()
    endif()
    set(golden_sun_platform_name "win")
    set(golden_sun_platform_windows TRUE)
endif()

if(${CMAKE_HOST_SYSTEM_NAME} STREQUAL "Windows")
    set(golden_sun_host_platform_name "win")
    set(golden_sun_host_platform_windows TRUE)
endif()

if(NOT golden_sun_arch_name)
    if((CMAKE_SYSTEM_PROCESSOR MATCHES "AMD64") OR (CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64"))
        set(golden_sun_arch_name "x64")
    else()
        set(golden_sun_arch_name "x86")
    endif()
endif()

if((CMAKE_HOST_SYSTEM_PROCESSOR MATCHES "AMD64") OR (CMAKE_HOST_SYSTEM_PROCESSOR MATCHES "x86_64"))
    set(golden_sun_host_arch_name "x64")
else()
    set(golden_sun_host_arch_name "x86")
endif()

set(golden_sun_platform_name ${golden_sun_platform_name}_${golden_sun_arch_name})
set(golden_sun_host_platform_name ${golden_sun_host_platform_name}_${golden_sun_host_arch_name})
