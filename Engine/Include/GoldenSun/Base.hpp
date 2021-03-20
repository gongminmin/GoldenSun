#pragma once

#define GOLDEN_SUN_SYMBOL_EXPORT __declspec(dllexport)
#define GOLDEN_SUN_SYMBOL_IMPORT __declspec(dllimport)

#ifdef GOLDEN_SUN_SOURCE // Build dll
#define GOLDEN_SUN_API GOLDEN_SUN_SYMBOL_EXPORT
#else // Use dll
#define GOLDEN_SUN_API GOLDEN_SUN_SYMBOL_IMPORT
#endif

#pragma warning(disable : 4251) // Export classes through DLL interface
#pragma warning(disable : 4324) // Enable padding in struct

#define DISALLOW_COPY_AND_ASSIGN(ClassName)     \
    ClassName(ClassName const& other) = delete; \
    ClassName& operator=(ClassName const& other) = delete;

#define DISALLOW_COPY_MOVE_AND_ASSIGN(ClassName) \
    ClassName(ClassName&& other) = delete;       \
    ClassName& operator=(ClassName&& other) = delete;

template <uint32_t Alignment>
constexpr uint32_t Align(uint32_t size) noexcept
{
    static_assert((Alignment & (Alignment - 1)) == 0);
    return (size + (Alignment - 1)) & ~(Alignment - 1);
}

template <typename T>
constexpr uint32_t ConvertToUint(T value) noexcept
{
    return static_cast<uint32_t>(value);
}
