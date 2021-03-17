#pragma once

#define GOLDEN_SUN_SYMBOL_EXPORT __declspec(dllexport)
#define GOLDEN_SUN_SYMBOL_IMPORT __declspec(dllimport)

#ifdef GOLDEN_SUN_SOURCE // Build dll
#define GOLDEN_SUN_API GOLDEN_SUN_SYMBOL_EXPORT
#else // Use dll
#define GOLDEN_SUN_API GOLDEN_SUN_SYMBOL_IMPORT
#endif

#pragma warning(disable : 4251)

#define DISALLOW_COPY_AND_ASSIGN(ClassName)     \
    ClassName(ClassName const& other) = delete; \
    ClassName& operator=(ClassName const& other) = delete;

#define DISALLOW_COPY_MOVE_AND_ASSIGN(ClassName) \
    ClassName(ClassName&& other) = delete;       \
    ClassName& operator=(ClassName&& other) = delete;
