#pragma once

#define GOLDEN_SUN_SYMBOL_EXPORT __declspec(dllexport)
#define GOLDEN_SUN_SYMBOL_IMPORT __declspec(dllimport)

#ifdef GOLDEN_SUN_SOURCE // Build dll
#define GOLDEN_SUN_API GOLDEN_SUN_SYMBOL_EXPORT
#else // Use dll
#define GOLDEN_SUN_API GOLDEN_SUN_SYMBOL_IMPORT
#endif
