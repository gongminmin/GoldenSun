#pragma once

#define GOLDEN_SUN_SYMBOL_EXPORT __declspec(dllexport)
#define GOLDEN_SUN_SYMBOL_IMPORT __declspec(dllimport)

#define DISALLOW_COPY_AND_ASSIGN(ClassName)     \
    ClassName(ClassName const& other) = delete; \
    ClassName& operator=(ClassName const& other) = delete;

#define DISALLOW_COPY_MOVE_AND_ASSIGN(ClassName) \
    ClassName(ClassName&& other) = delete;       \
    ClassName& operator=(ClassName&& other) = delete;
