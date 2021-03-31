#pragma once

#include <GoldenSun/Base.hpp>

#ifdef GOLDEN_SUN_SOURCE // Build dll
#define GOLDEN_SUN_API GOLDEN_SUN_SYMBOL_EXPORT
#else // Use dll
#define GOLDEN_SUN_API GOLDEN_SUN_SYMBOL_IMPORT
#endif

#include <GoldenSun/Engine.hpp>
#include <GoldenSun/Light.hpp>
#include <GoldenSun/Material.hpp>
#include <GoldenSun/Mesh.hpp>
