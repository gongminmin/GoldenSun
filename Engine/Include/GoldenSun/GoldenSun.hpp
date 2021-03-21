#pragma once

#ifdef GOLDEN_SUN_SOURCE // Build dll
#define GOLDEN_SUN_API GOLDEN_SUN_SYMBOL_EXPORT
#else // Use dll
#define GOLDEN_SUN_API GOLDEN_SUN_SYMBOL_IMPORT
#endif

#pragma warning(disable : 4251) // Export classes through DLL interface
#pragma warning(disable : 4324) // Enable padding in struct

#include <GoldenSun/Engine.hpp>
#include <GoldenSun/Mesh.hpp>
