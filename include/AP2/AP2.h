/*
 * AP2 — Application Primitives
 * Copyright (c) 2024-2026 Jack Waechter
 *
 * Licensed under the MIT License.
 * See LICENSE in the project root for full terms.
 */

#ifndef AP2_H
#define AP2_H

/*
 * AP2 - Application Primitives
 *
 * Umbrella header. Application code should include this file:
 *
 *     #include <AP2/AP2.h>
 *
 * Typical usage:
 *
 *     AP_Init(AP_INIT_VIDEO);
 *     AP_CreateWindow("AP2", 1280, 720, AP_WINDOW_RESIZABLE);
 *
 *     while (AP_IsRunning()) {
 *         AP_PumpEvents();
 *         double dt = AP_Tick();
 *         AP_SetDrawColor(0.1f, 0.1f, 0.1f, 1.0f);
 *         AP_Clear();
 *         AP_Present();
 *     }
 *
 *     AP_DestroyWindow(NULL);
 *     AP_Quit();
 *
 * Module control — define these *before* including this header:
 *
 *     AP2_ONLY_CORE          Types, error, logger, init only
 *     AP2_NO_GRAPHICS        Drop device, video, renderer, and dependents
 *     AP2_NO_<MODULE>         Exclude one module (see list below)
 *     AP2_INCLUDE_OPENGL     Pull in the low-level OpenGL backend
 *
 * Exclude flags: AP2_NO_ERROR, AP2_NO_LOGGER, AP2_NO_PLATFORM, AP2_NO_MATH,
 * AP2_NO_DEVICE, AP2_NO_VIDEO, AP2_NO_WINDOW, AP2_NO_INPUT, AP2_NO_RENDERER,
 * AP2_NO_TEXTURE, AP2_NO_SPRITE, AP2_NO_TILEMAP, AP2_NO_SHADER, AP2_NO_FONT,
 * AP2_NO_GUI, AP2_NO_LIST, AP2_NO_STRING, AP2_NO_IMAGE, AP2_NO_CAMERA, AP2_NO_3D,
 * AP2_NO_AUDIO, AP2_NO_POST, AP2_NO_OPENGL
 *
 * After include, AP2_HAS_<MODULE> is 0 or 1 for each module.
 *
 * AP2_Internal.h is private. Including it from application code is an
 * error unless AP2_ALLOW_INTERNAL is defined.
 *
 * Started in 2024. The long name changed; AP2 didn't.
 */

#ifdef AP2_INTERNAL_H
#error "AP2.h must not be included after AP2_Internal.h. Library sources include specific public headers, not this umbrella."
#endif

#ifndef AP2_ALLOW_INTERNAL
#define AP2_PUBLIC_API_INCLUDED 1
#endif

/* =========================================================
 * Identity
 * ========================================================= */

#define AP2_NAME "AP2"
#define AP2_FULL_NAME "Application Primitives" /* used to mean something else */
#define AP2_DESCRIPTION "Application Primitives — a C17 kit for games and tools"
#define AP2_AUTHOR "Jack Waechter"
#define AP2_LICENSE "MIT"

#ifndef AP2_VERSION_MAJOR
#define AP2_VERSION_MAJOR 0
#endif
#ifndef AP2_VERSION_MINOR
#define AP2_VERSION_MINOR 1
#endif
#ifndef AP2_VERSION_PATCH
#define AP2_VERSION_PATCH 0
#endif

#define AP2_STRINGIFY_IMPL(x) #x
#define AP2_STRINGIFY(x) AP2_STRINGIFY_IMPL(x)
#define AP2_CONCAT_IMPL(a, b) a##b
#define AP2_CONCAT(a, b) AP2_CONCAT_IMPL(a, b)

#define AP2_VERSION_ENCODE(major, minor, patch) \
  (((major) * 10000) + ((minor) * 100) + (patch))

#define AP2_VERSION_NUMBER \
  AP2_VERSION_ENCODE(AP2_VERSION_MAJOR, AP2_VERSION_MINOR, AP2_VERSION_PATCH)

#define AP2_VERSION_ATLEAST(major, minor, patch) \
  (AP2_VERSION_NUMBER >= AP2_VERSION_ENCODE(major, minor, patch))

#ifndef AP2_VERSION_STRING
#define AP2_VERSION_STRING         \
  AP2_STRINGIFY(AP2_VERSION_MAJOR) \
  "." AP2_STRINGIFY(AP2_VERSION_MINOR) "." AP2_STRINGIFY(AP2_VERSION_PATCH)
#endif

#ifndef AP2_VERSION
#define AP2_VERSION AP2_VERSION_STRING
#endif

/* =========================================================
 * Language
 * ========================================================= */

#ifdef __cplusplus
#define AP2_CPLUSPLUS
#if __cplusplus >= 201703L
#define AP2_CPLUSPLUS17 1
#endif
#define AP2_BEGIN_DECLS \
  extern "C"            \
  {
#define AP2_END_DECLS }
#else
#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201710L
#define AP2_C17 1
#elif defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L
#define AP2_C11 1
#endif
#define AP2_BEGIN_DECLS
#define AP2_END_DECLS
#endif

/* =========================================================
 * Platform
 * ========================================================= */

#if defined(_WIN32) || defined(_WIN64) || defined(__CYGWIN__)
#define AP2_PLATFORM_WINDOWS 1
#if defined(_WIN64) || defined(__x86_64__) || defined(__aarch64__)
#define AP2_PLATFORM_WIN64 1
#else
#define AP2_PLATFORM_WIN32 1
#endif
#elif defined(__APPLE__)
#define AP2_PLATFORM_APPLE 1
#include <TargetConditionals.h>
#if TARGET_OS_IPHONE
#define AP2_PLATFORM_IOS 1
#else
#define AP2_PLATFORM_MACOS 1
#endif
#elif defined(__ANDROID__)
#define AP2_PLATFORM_ANDROID 1
#define AP2_PLATFORM_LINUX 1
#elif defined(__linux__)
#define AP2_PLATFORM_LINUX 1
#elif defined(__FreeBSD__) || defined(__NetBSD__) || defined(__OpenBSD__) || \
    defined(__DragonFly__)
#define AP2_PLATFORM_BSD 1
#elif defined(__EMSCRIPTEN__)
#define AP2_PLATFORM_EMSCRIPTEN 1
#else
#define AP2_PLATFORM_UNKNOWN 1
#endif

#if defined(_WIN32)
#define AP2_PATH_SEPARATOR '\\'
#define AP2_PATH_SEPARATOR_STR "\\"
#else
#define AP2_PATH_SEPARATOR '/'
#define AP2_PATH_SEPARATOR_STR "/"
#endif

/* =========================================================
 * Architecture
 * ========================================================= */

#if defined(_M_X64) || defined(__x86_64__) || defined(__amd64__)
#define AP2_ARCH_X64 1
#define AP2_ARCH_64 1
#elif defined(_M_IX86) || defined(__i386__)
#define AP2_ARCH_X86 1
#define AP2_ARCH_32 1
#elif defined(_M_ARM64) || defined(__aarch64__)
#define AP2_ARCH_ARM64 1
#define AP2_ARCH_64 1
#elif defined(_M_ARM) || defined(__arm__)
#define AP2_ARCH_ARM 1
#define AP2_ARCH_32 1
#elif defined(__wasm64__)
#define AP2_ARCH_WASM64 1
#define AP2_ARCH_64 1
#elif defined(__wasm32__) || defined(__EMSCRIPTEN__)
#define AP2_ARCH_WASM32 1
#define AP2_ARCH_32 1
#endif

#ifndef AP2_ARCH_64
#ifndef AP2_ARCH_32
#if defined(_WIN64) || defined(__LP64__) || defined(_LP64)
#define AP2_ARCH_64 1
#else
#define AP2_ARCH_32 1
#endif
#endif
#endif

#if defined(__BYTE_ORDER__) && defined(__ORDER_BIG_ENDIAN__) && \
    __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
#define AP2_BIG_ENDIAN 1
#else
#define AP2_LITTLE_ENDIAN 1
#endif

/* =========================================================
 * Compiler
 * ========================================================= */

#if defined(_MSC_VER) && !defined(__clang__)
#define AP2_COMPILER_MSVC 1
#define AP2_COMPILER_VERSION _MSC_VER
#elif defined(__clang__)
#define AP2_COMPILER_CLANG 1
#define AP2_COMPILER_VERSION (__clang_major__ * 100 + __clang_minor__)
#if defined(__apple_build_version__)
#define AP2_COMPILER_APPLE_CLANG 1
#endif
#elif defined(__GNUC__)
#define AP2_COMPILER_GCC 1
#define AP2_COMPILER_VERSION (__GNUC__ * 100 + __GNUC_MINOR__)
#elif defined(__INTEL_COMPILER)
#define AP2_COMPILER_INTEL 1
#define AP2_COMPILER_VERSION __INTEL_COMPILER
#else
#define AP2_COMPILER_UNKNOWN 1
#define AP2_COMPILER_VERSION 0
#endif

#if defined(__MINGW32__) || defined(__MINGW64__)
#define AP2_COMPILER_MINGW 1
#endif

/* =========================================================
 * API export / calling convention
 *
 * Define AP2_SHARED when consuming or building a shared library.
 * Define AP2_BUILD when compiling AP2 itself as a shared library.
 * ========================================================= */

#ifndef AP2_API
#if defined(AP2_SHARED)
#if defined(_WIN32) || defined(__CYGWIN__)
#if defined(AP2_BUILD)
#define AP2_API __declspec(dllexport)
#else
#define AP2_API __declspec(dllimport)
#endif
#elif defined(__GNUC__) && __GNUC__ >= 4
#define AP2_API __attribute__((visibility("default")))
#else
#define AP2_API
#endif
#else
#define AP2_API
#endif
#endif

#ifndef AP2_CALL
#if defined(_WIN32) && defined(AP2_USE_STDCALL)
#define AP2_CALL __stdcall
#else
#define AP2_CALL
#endif
#endif

#ifndef AP2_INLINE
#if defined(AP2_COMPILER_MSVC)
#define AP2_INLINE __inline
#elif defined(__cplusplus) || \
    (defined(__STDC_VERSION__) && __STDC_VERSION__ >= 199901L)
#define AP2_INLINE inline
#else
#define AP2_INLINE
#endif
#endif

#ifndef AP2_FORCE_INLINE
#if defined(AP2_COMPILER_MSVC)
#define AP2_FORCE_INLINE __forceinline
#elif defined(AP2_COMPILER_GCC) || defined(AP2_COMPILER_CLANG)
#define AP2_FORCE_INLINE inline __attribute__((always_inline))
#else
#define AP2_FORCE_INLINE AP2_INLINE
#endif
#endif

#ifndef AP2_RESTRICT
#if defined(__cplusplus)
#define AP2_RESTRICT
#elif defined(AP2_COMPILER_MSVC)
#define AP2_RESTRICT __restrict
#elif defined(__STDC_VERSION__) && __STDC_VERSION__ >= 199901L
#define AP2_RESTRICT restrict
#else
#define AP2_RESTRICT
#endif
#endif

#ifndef AP2_UNUSED
#if defined(AP2_COMPILER_GCC) || defined(AP2_COMPILER_CLANG)
#define AP2_UNUSED __attribute__((unused))
#else
#define AP2_UNUSED
#endif
#endif

#ifndef AP2_DEPRECATED
#if defined(AP2_COMPILER_MSVC)
#define AP2_DEPRECATED(msg) __declspec(deprecated(msg))
#elif defined(AP2_COMPILER_GCC) || defined(AP2_COMPILER_CLANG)
#define AP2_DEPRECATED(msg) __attribute__((deprecated(msg)))
#else
#define AP2_DEPRECATED(msg)
#endif
#endif

#ifndef AP2_NODISCARD
#if defined(__cplusplus) && __cplusplus >= 201703L
#define AP2_NODISCARD [[nodiscard]]
#elif defined(AP2_COMPILER_GCC) || defined(AP2_COMPILER_CLANG)
#define AP2_NODISCARD __attribute__((warn_unused_result))
#elif defined(AP2_COMPILER_MSVC) && _MSC_VER >= 1700
#define AP2_NODISCARD _Check_return_
#else
#define AP2_NODISCARD
#endif
#endif

#ifndef AP2_PRINTF_FORMAT
#if defined(AP2_COMPILER_GCC) || defined(AP2_COMPILER_CLANG)
#define AP2_PRINTF_FORMAT(fmt_index, arg_index) \
  __attribute__((format(printf, fmt_index, arg_index)))
#else
#define AP2_PRINTF_FORMAT(fmt_index, arg_index)
#endif
#endif

#ifndef AP2_NORETURN
#if defined(__cplusplus) && __cplusplus >= 201103L
#define AP2_NORETURN [[noreturn]]
#elif defined(AP2_COMPILER_MSVC)
#define AP2_NORETURN __declspec(noreturn)
#elif defined(AP2_COMPILER_GCC) || defined(AP2_COMPILER_CLANG)
#define AP2_NORETURN __attribute__((noreturn))
#else
#define AP2_NORETURN
#endif
#endif

#ifndef AP2_ALIGN
#if defined(AP2_COMPILER_MSVC)
#define AP2_ALIGN(n) __declspec(align(n))
#elif defined(AP2_COMPILER_GCC) || defined(AP2_COMPILER_CLANG)
#define AP2_ALIGN(n) __attribute__((aligned(n)))
#else
#define AP2_ALIGN(n)
#endif
#endif

#ifndef AP2_PACKED
#if defined(AP2_COMPILER_MSVC)
#define AP2_PACKED_BEGIN __pragma(pack(push, 1))
#define AP2_PACKED_END __pragma(pack(pop))
#define AP2_PACKED
#else
#define AP2_PACKED_BEGIN
#define AP2_PACKED_END
#define AP2_PACKED __attribute__((packed))
#endif
#endif

#ifndef AP2_LIKELY
#if defined(AP2_COMPILER_GCC) || defined(AP2_COMPILER_CLANG)
#define AP2_LIKELY(x) __builtin_expect(!!(x), 1)
#define AP2_UNLIKELY(x) __builtin_expect(!!(x), 0)
#else
#define AP2_LIKELY(x) (x)
#define AP2_UNLIKELY(x) (x)
#endif
#endif

#ifndef AP2_STATIC_ASSERT
#if defined(__cplusplus)
#define AP2_STATIC_ASSERT(cond, msg) static_assert(cond, msg)
#elif defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L
#define AP2_STATIC_ASSERT(cond, msg) _Static_assert(cond, msg)
#else
#define AP2_STATIC_ASSERT(cond, msg) \
  typedef char AP2_CONCAT(ap2_static_assert_, __LINE__)[(cond) ? 1 : -1]
#endif
#endif

#define AP2_UNUSED_PARAM(x) ((void)(x))

#ifndef AP2_ARRAY_COUNT
#define AP2_ARRAY_COUNT(arr) (sizeof(arr) / sizeof((arr)[0]))
#endif

#ifndef AP2_MIN
#define AP2_MIN(a, b) (((a) < (b)) ? (a) : (b))
#endif

#ifndef AP2_MAX
#define AP2_MAX(a, b) (((a) > (b)) ? (a) : (b))
#endif

#ifndef AP2_CLAMP
#define AP2_CLAMP(x, lo, hi) AP2_MAX(lo, AP2_MIN(x, hi))
#endif

/*
 * Windows headers define min, max, near, and far as macros. Those
 * collide with AP2 identifiers (and with AP2_Math parameter names).
 */
#ifdef min
#undef min
#endif
#ifdef max
#undef max
#endif
#ifdef near
#undef near
#endif
#ifdef far
#undef far
#endif

/* =========================================================
 * Module policy
 *
 * Resolve convenience flags into per-module excludes, then
 * expose AP2_HAS_* so application and library code can branch
 * on what this translation unit actually imported.
 * ========================================================= */

#ifdef AP2_ONLY_CORE
#ifndef AP2_NO_DEVICE
#define AP2_NO_DEVICE
#endif
#ifndef AP2_NO_VIDEO
#define AP2_NO_VIDEO
#endif
#ifndef AP2_NO_WINDOW
#define AP2_NO_WINDOW
#endif
#ifndef AP2_NO_INPUT
#define AP2_NO_INPUT
#endif
#ifndef AP2_NO_RENDERER
#define AP2_NO_RENDERER
#endif
#ifndef AP2_NO_LIST
#define AP2_NO_LIST
#endif
#ifndef AP2_NO_STRING
#define AP2_NO_STRING
#endif
#ifndef AP2_NO_IMAGE
#define AP2_NO_IMAGE
#endif
#ifndef AP2_NO_CAMERA
#define AP2_NO_CAMERA
#endif
#ifndef AP2_NO_3D
#define AP2_NO_3D
#endif
#ifndef AP2_NO_AUDIO
#define AP2_NO_AUDIO
#endif
#ifndef AP2_NO_POST
#define AP2_NO_POST
#endif
#endif

#ifdef AP2_NO_GRAPHICS
#ifndef AP2_NO_DEVICE
#define AP2_NO_DEVICE
#endif
#ifndef AP2_NO_VIDEO
#define AP2_NO_VIDEO
#endif
#ifndef AP2_NO_RENDERER
#define AP2_NO_RENDERER
#endif
#ifndef AP2_NO_OPENGL
#define AP2_NO_OPENGL
#endif
#ifndef AP2_NO_3D
#define AP2_NO_3D
#endif
#ifndef AP2_NO_POST
#define AP2_NO_POST
#endif
#endif

#ifdef AP2_NO_MATH
#ifndef AP2_NO_CAMERA
#define AP2_NO_CAMERA
#endif
#ifndef AP2_NO_3D
#define AP2_NO_3D
#endif
#endif

#ifdef AP2_NO_CAMERA
#ifndef AP2_NO_3D
#define AP2_NO_3D
#endif
#endif

#ifdef AP2_NO_WINDOW
#ifndef AP2_NO_INPUT
#define AP2_NO_INPUT
#endif
#endif

#ifdef AP2_NO_RENDERER
#ifndef AP2_NO_TEXTURE
#define AP2_NO_TEXTURE
#endif
#ifndef AP2_NO_SHADER
#define AP2_NO_SHADER
#endif
#ifndef AP2_NO_FONT
#define AP2_NO_FONT
#endif
#ifndef AP2_NO_3D
#define AP2_NO_3D
#endif
#ifndef AP2_NO_POST
#define AP2_NO_POST
#endif
#endif

#ifdef AP2_NO_SHADER
#ifndef AP2_NO_3D
#define AP2_NO_3D
#endif
#ifndef AP2_NO_POST
#define AP2_NO_POST
#endif
#endif

#ifdef AP2_NO_TEXTURE
#ifndef AP2_NO_SPRITE
#define AP2_NO_SPRITE
#endif
#ifndef AP2_NO_TILEMAP
#define AP2_NO_TILEMAP
#endif
#ifndef AP2_NO_POST
#define AP2_NO_POST
#endif
#endif

#ifdef AP2_NO_FONT
#ifndef AP2_NO_GUI
#define AP2_NO_GUI
#endif
#endif

#if defined(AP2_INCLUDE_OPENGL) && defined(AP2_NO_OPENGL)
#error "AP2_INCLUDE_OPENGL and AP2_NO_OPENGL cannot both be defined"
#endif

#undef AP2_HAS_PLATFORM
#ifdef AP2_NO_PLATFORM
#define AP2_HAS_PLATFORM 0
#else
#define AP2_HAS_PLATFORM 1
#endif

#undef AP2_HAS_ERROR
#ifdef AP2_NO_ERROR
#define AP2_HAS_ERROR 0
#else
#define AP2_HAS_ERROR 1
#endif

#undef AP2_HAS_LOGGER
#ifdef AP2_NO_LOGGER
#define AP2_HAS_LOGGER 0
#else
#define AP2_HAS_LOGGER 1
#endif

#undef AP2_HAS_MATH
#ifdef AP2_NO_MATH
#define AP2_HAS_MATH 0
#else
#define AP2_HAS_MATH 1
#endif

#undef AP2_HAS_DEVICE
#ifdef AP2_NO_DEVICE
#define AP2_HAS_DEVICE 0
#else
#define AP2_HAS_DEVICE 1
#endif

#undef AP2_HAS_VIDEO
#ifdef AP2_NO_VIDEO
#define AP2_HAS_VIDEO 0
#else
#define AP2_HAS_VIDEO 1
#endif

#undef AP2_HAS_WINDOW
#ifdef AP2_NO_WINDOW
#define AP2_HAS_WINDOW 0
#else
#define AP2_HAS_WINDOW 1
#endif

#undef AP2_HAS_INPUT
#ifdef AP2_NO_INPUT
#define AP2_HAS_INPUT 0
#else
#define AP2_HAS_INPUT 1
#endif

#undef AP2_HAS_RENDERER
#ifdef AP2_NO_RENDERER
#define AP2_HAS_RENDERER 0
#else
#define AP2_HAS_RENDERER 1
#endif

#undef AP2_HAS_IMAGE
#ifdef AP2_NO_IMAGE
#define AP2_HAS_IMAGE 0
#else
#define AP2_HAS_IMAGE 1
#endif

#undef AP2_HAS_TEXTURE
#ifdef AP2_NO_TEXTURE
#define AP2_HAS_TEXTURE 0
#else
#define AP2_HAS_TEXTURE 1
#endif

#undef AP2_HAS_SPRITE
#ifdef AP2_NO_SPRITE
#define AP2_HAS_SPRITE 0
#else
#define AP2_HAS_SPRITE 1
#endif

#undef AP2_HAS_TILEMAP
#ifdef AP2_NO_TILEMAP
#define AP2_HAS_TILEMAP 0
#else
#define AP2_HAS_TILEMAP 1
#endif

#undef AP2_HAS_SHADER
#ifdef AP2_NO_SHADER
#define AP2_HAS_SHADER 0
#else
#define AP2_HAS_SHADER 1
#endif

#undef AP2_HAS_FONT
#ifdef AP2_NO_FONT
#define AP2_HAS_FONT 0
#else
#define AP2_HAS_FONT 1
#endif

#undef AP2_HAS_GUI
#ifdef AP2_NO_GUI
#define AP2_HAS_GUI 0
#else
#define AP2_HAS_GUI 1
#endif

#undef AP2_HAS_LIST
#ifdef AP2_NO_LIST
#define AP2_HAS_LIST 0
#else
#define AP2_HAS_LIST 1
#endif

#undef AP2_HAS_STRING
#ifdef AP2_NO_STRING
#define AP2_HAS_STRING 0
#else
#define AP2_HAS_STRING 1
#endif

#undef AP2_HAS_CAMERA
#ifdef AP2_NO_CAMERA
#define AP2_HAS_CAMERA 0
#else
#define AP2_HAS_CAMERA 1
#endif

#undef AP2_HAS_3D
#ifdef AP2_NO_3D
#define AP2_HAS_3D 0
#else
#define AP2_HAS_3D 1
#endif

#undef AP2_HAS_AUDIO
#ifdef AP2_NO_AUDIO
#define AP2_HAS_AUDIO 0
#else
#define AP2_HAS_AUDIO 1
#endif

#undef AP2_HAS_POST
#ifdef AP2_NO_POST
#define AP2_HAS_POST 0
#else
#define AP2_HAS_POST 1
#endif

#undef AP2_HAS_OPENGL
#if defined(AP2_INCLUDE_OPENGL) && !defined(AP2_NO_OPENGL)
#define AP2_HAS_OPENGL 1
#else
#define AP2_HAS_OPENGL 0
#endif

#define AP2_HAS_TYPES 1
#define AP2_HAS_INIT 1

#define AP2_MODULE_TYPES (1u << 0)
#define AP2_MODULE_ERROR (1u << 1)
#define AP2_MODULE_LOGGER (1u << 2)
#define AP2_MODULE_INIT (1u << 3)
#define AP2_MODULE_PLATFORM (1u << 4)
#define AP2_MODULE_MATH (1u << 5)
#define AP2_MODULE_LIST (1u << 6)
#define AP2_MODULE_STRING (1u << 7)
#define AP2_MODULE_DEVICE (1u << 8)
#define AP2_MODULE_VIDEO (1u << 9)
#define AP2_MODULE_WINDOW (1u << 10)
#define AP2_MODULE_INPUT (1u << 11)
#define AP2_MODULE_RENDERER (1u << 12)
#define AP2_MODULE_TEXTURE (1u << 13)
#define AP2_MODULE_SPRITE (1u << 14)
#define AP2_MODULE_SHADER (1u << 15)
#define AP2_MODULE_FONT (1u << 16)
#define AP2_MODULE_GUI (1u << 17)
#define AP2_MODULE_OPENGL (1u << 18)
#define AP2_MODULE_3D (1u << 19)
#define AP2_MODULE_AUDIO (1u << 20)
#define AP2_MODULE_POST (1u << 21)
#define AP2_MODULE_TILEMAP (1u << 22)
#define AP2_MODULE_CAMERA (1u << 23)
#define AP2_MODULE_IMAGE (1u << 24)

#define AP2_ENABLED_MODULES                        \
  (AP2_MODULE_TYPES | AP2_MODULE_INIT |            \
   (AP2_HAS_ERROR ? AP2_MODULE_ERROR : 0u) |       \
   (AP2_HAS_LOGGER ? AP2_MODULE_LOGGER : 0u) |     \
   (AP2_HAS_PLATFORM ? AP2_MODULE_PLATFORM : 0u) | \
   (AP2_HAS_MATH ? AP2_MODULE_MATH : 0u) |         \
   (AP2_HAS_CAMERA ? AP2_MODULE_CAMERA : 0u) |     \
   (AP2_HAS_LIST ? AP2_MODULE_LIST : 0u) |         \
   (AP2_HAS_STRING ? AP2_MODULE_STRING : 0u) |     \
   (AP2_HAS_DEVICE ? AP2_MODULE_DEVICE : 0u) |     \
   (AP2_HAS_VIDEO ? AP2_MODULE_VIDEO : 0u) |       \
   (AP2_HAS_WINDOW ? AP2_MODULE_WINDOW : 0u) |     \
   (AP2_HAS_INPUT ? AP2_MODULE_INPUT : 0u) |       \
   (AP2_HAS_RENDERER ? AP2_MODULE_RENDERER : 0u) | \
   (AP2_HAS_IMAGE ? AP2_MODULE_IMAGE : 0u) |       \
   (AP2_HAS_TEXTURE ? AP2_MODULE_TEXTURE : 0u) |   \
   (AP2_HAS_SPRITE ? AP2_MODULE_SPRITE : 0u) |     \
   (AP2_HAS_TILEMAP ? AP2_MODULE_TILEMAP : 0u) |   \
   (AP2_HAS_SHADER ? AP2_MODULE_SHADER : 0u) |     \
   (AP2_HAS_FONT ? AP2_MODULE_FONT : 0u) |         \
   (AP2_HAS_GUI ? AP2_MODULE_GUI : 0u) |           \
   (AP2_HAS_OPENGL ? AP2_MODULE_OPENGL : 0u) |     \
   (AP2_HAS_3D ? AP2_MODULE_3D : 0u) |             \
   (AP2_HAS_AUDIO ? AP2_MODULE_AUDIO : 0u) |       \
   (AP2_HAS_POST ? AP2_MODULE_POST : 0u))

/* =========================================================
 * Public headers
 *
 * Core is always imported. Everything else follows AP2_HAS_*.
 * OpenGL is opt-in: it is a backend surface, not the app API.
 * ========================================================= */

#include "AP2/AP2_Types.h"
#include "AP2/AP2_Init.h"

#if AP2_HAS_MATH
#include "AP2/AP2_Math.h"
#endif

#if AP2_HAS_CAMERA
#include "AP2/AP2_Camera.h"
#endif

#if AP2_HAS_PLATFORM
#include "AP2/AP2_Platform.h"
#endif

#if AP2_HAS_ERROR
#include "AP2/AP2_Error.h"
#endif

#if AP2_HAS_LOGGER
#include "AP2/AP2_Logger.h"
#endif

#if AP2_HAS_LIST
#include "AP2/AP2_List.h"
#endif

#if AP2_HAS_STRING
#include "AP2/AP2_String.h"
#endif

#if AP2_HAS_DEVICE
#include "AP2/AP2_Device.h"
#endif

#if AP2_HAS_VIDEO
#include "AP2/AP2_Video.h"
#endif

#if AP2_HAS_WINDOW
#include "AP2/AP2_Window.h"
#endif

#if AP2_HAS_INPUT
#include "AP2/AP2_Input.h"
#endif

#if AP2_HAS_RENDERER
#include "AP2/AP2_Renderer.h"
#endif

#if AP2_HAS_IMAGE
#include "AP2/AP2_Image.h"
#endif

#if AP2_HAS_TEXTURE
#include "AP2/AP2_Texture.h"
#endif

#if AP2_HAS_SPRITE
#include "AP2/AP2_Sprite.h"
#endif

#if AP2_HAS_TILEMAP
#include "AP2/AP2_Tilemap.h"
#endif

#if AP2_HAS_SHADER
#include "AP2/AP2_Shader.h"
#endif

#if AP2_HAS_3D
#include "AP2/AP2_Material.h"
#include "AP2/AP2_3D.h"
#endif

#if AP2_HAS_AUDIO
#include "AP2/AP2_Audio.h"
#endif

#if AP2_HAS_POST
#include "AP2/AP2_Post.h"
#include "AP2/AP2_Post_extra.h"
#endif

#if AP2_HAS_FONT
#include "AP2/AP2_Font.h"
#endif

#if AP2_HAS_GUI
#include "AP2/AP2_Gui.h"
#endif

#if AP2_HAS_OPENGL
#include "AP2/AP2_Opengl.h"
#endif

/* =========================================================
 * Compiled version
 * ========================================================= */

AP2_BEGIN_DECLS

AP2_UNUSED static AP2_INLINE AP_Version AP_GetCompiledVersion(void)
{
  AP_Version version;
  version.major = AP2_VERSION_MAJOR;
  version.minor = AP2_VERSION_MINOR;
  version.patch = AP2_VERSION_PATCH;
  return version;
}

AP2_END_DECLS

AP2_STATIC_ASSERT(AP2_VERSION_MAJOR >= 0, "AP2 major version");
AP2_STATIC_ASSERT(AP2_VERSION_MINOR >= 0, "AP2 minor version");
AP2_STATIC_ASSERT(AP2_VERSION_PATCH >= 0, "AP2 patch version");
AP2_STATIC_ASSERT(sizeof(AP_U8) == 1, "AP_U8 must be 8-bit");
AP2_STATIC_ASSERT(sizeof(AP_U16) == 2, "AP_U16 must be 16-bit");
AP2_STATIC_ASSERT(sizeof(AP_U32) == 4, "AP_U32 must be 32-bit");
AP2_STATIC_ASSERT(sizeof(AP_U64) == 8, "AP_U64 must be 64-bit");

#endif /* AP2_H */
