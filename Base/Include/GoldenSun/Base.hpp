#pragma once

#define GOLDEN_SUN_SYMBOL_EXPORT __declspec(dllexport)
#define GOLDEN_SUN_SYMBOL_IMPORT __declspec(dllimport)

#define DISALLOW_COPY_AND_ASSIGN(ClassName)     \
    ClassName(ClassName const& other) = delete; \
    ClassName& operator=(ClassName const& other) = delete;

#define DISALLOW_COPY_MOVE_AND_ASSIGN(ClassName) \
    ClassName(ClassName&& other) = delete;       \
    ClassName& operator=(ClassName&& other) = delete;

#define GOLDEN_SUN_UNREACHABLE(msg) __assume(false)

namespace GoldenSun
{
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
} // namespace GoldenSun
