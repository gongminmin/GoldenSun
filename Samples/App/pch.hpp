#pragma once

#define INITGUID

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include <d3d12.h>
#include <dxgi1_6.h>

#include <DirectXMath.h>

#include <GoldenSun/ComPtr.hpp>

#pragma warning(disable : 4251)
#pragma warning(disable : 4275)
