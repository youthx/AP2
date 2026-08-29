/*
 * AP2 — Application Primitives
 * Copyright (c) 2024-2026 Jack Waechter
 *
 * Licensed under the MIT License.
 * See LICENSE in the project root for full terms.
 */

#ifndef AP2_PLATFORM_H
#define AP2_PLATFORM_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * AP2 Platform
 *
 * Compile-time OS detection and runtime window-system / OpenGL
 * capability queries. Windowing, video, and device use this to pick
 * stable defaults without leaking GLFW or native headers.
 */

/* =========================================================
 * Compile-time OS
 * ========================================================= */

#if defined(_WIN32)
#define AP_PLATFORM_WINDOWS 1
#define AP_PLATFORM_MACOS 0
#define AP_PLATFORM_LINUX 0
#elif defined(__APPLE__)
#define AP_PLATFORM_WINDOWS 0
#define AP_PLATFORM_MACOS 1
#define AP_PLATFORM_LINUX 0
#elif defined(__linux__)
#define AP_PLATFORM_WINDOWS 0
#define AP_PLATFORM_MACOS 0
#define AP_PLATFORM_LINUX 1
#else
#define AP_PLATFORM_WINDOWS 0
#define AP_PLATFORM_MACOS 0
#define AP_PLATFORM_LINUX 0
#endif

#define AP_PLATFORM_OPENGL_MIN_MAJOR 3u
#define AP_PLATFORM_OPENGL_MIN_MINOR 3u

typedef enum AP_PlatformOS {
  AP_PLATFORM_OS_UNKNOWN = 0,
  AP_PLATFORM_OS_WINDOWS,
  AP_PLATFORM_OS_MACOS,
  AP_PLATFORM_OS_LINUX,
  AP_PLATFORM_OS_COUNT
} AP_PlatformOS;

typedef enum AP_PlatformArch {
  AP_PLATFORM_ARCH_UNKNOWN = 0,
  AP_PLATFORM_ARCH_X86,
  AP_PLATFORM_ARCH_X64,
  AP_PLATFORM_ARCH_ARM64,
  AP_PLATFORM_ARCH_COUNT
} AP_PlatformArch;

typedef enum AP_PlatformWindowSystem {
  AP_PLATFORM_WINDOW_UNKNOWN = 0,
  AP_PLATFORM_WINDOW_WIN32,
  AP_PLATFORM_WINDOW_COCOA,
  AP_PLATFORM_WINDOW_X11,
  AP_PLATFORM_WINDOW_WAYLAND,
  AP_PLATFORM_WINDOW_COUNT
} AP_PlatformWindowSystem;

typedef enum AP_PlatformContextAPI {
  AP_PLATFORM_CONTEXT_NATIVE = 0,
  AP_PLATFORM_CONTEXT_EGL,
  AP_PLATFORM_CONTEXT_OSMESA
} AP_PlatformContextAPI;

AP_PlatformOS AP_PlatformGetOS(void);

const char *AP_PlatformOSName(AP_PlatformOS os);

AP_PlatformArch AP_PlatformGetArch(void);

const char *AP_PlatformArchName(AP_PlatformArch arch);

/*
 * Window system actually in use. Before GLFW is initialized this is
 * the compile-time default; afterwards it reflects glfwGetPlatform().
 */
AP_PlatformWindowSystem AP_PlatformGetWindowSystem(void);

const char *AP_PlatformWindowSystemName(AP_PlatformWindowSystem system);

bool AP_PlatformHasOpenGL(void);

bool AP_PlatformHasVulkan(void);

bool AP_PlatformHasD3D11(void);

bool AP_PlatformHasD3D12(void);

/*
 * Highest OpenGL core version this OS is known to expose.
 * macOS is 4.1. Windows and Linux are 4.6.
 */
void AP_PlatformMaxOpenGLVersion(uint32_t *major, uint32_t *minor);

/*
 * Preferred request for new contexts. Honors AP2_GL_VERSION if set
 * (for example "4.1"), otherwise the platform maximum.
 */
void AP_PlatformRecommendedOpenGLVersion(uint32_t *major, uint32_t *minor);

/*
 * macOS requires a forward-compatible core context for 3.2+.
 */
bool AP_PlatformOpenGLRequiresForwardCompat(void);

AP_PlatformContextAPI AP_PlatformGetContextAPI(void);

/*
 * Unique OpenGL versions to try, highest first, all within
 * [3.3, platform max]. Requested version is first when valid.
 * Returns the number of entries written, at most `capacity`.
 */
int AP_PlatformEnumerateOpenGLVersions(uint32_t requested_major,
                                       uint32_t requested_minor,
                                       uint32_t *majors, uint32_t *minors,
                                       int capacity);

/*
 * GLFW init hints. Call immediately before glfwInit().
 *
 * AP2_WINDOW_BACKEND may be set to x11, wayland, win32, or cocoa.
 */
void AP_PlatformPrepareWindowing(void);

void AP_PlatformRefreshWindowSystem(void);

void *AP_PlatformGetNativeWindow(void *glfw_window);

void *AP_PlatformGetNativeDisplay(void);

bool AP_PlatformIsMainThread(void);

uint64_t AP_PlatformGetTimerFrequency(void);

uint64_t AP_PlatformGetTimerValue(void);

#ifdef __cplusplus
}
#endif

#endif /* AP2_PLATFORM_H */
