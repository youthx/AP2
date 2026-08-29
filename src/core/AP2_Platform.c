/*
 * AP2 — Application Primitives
 * Copyright (c) 2024-2026 Jack Waechter
 *
 * Licensed under the MIT License.
 * See LICENSE in the project root for full terms.
 */

#include "AP2/AP2_Platform.h"

#include "AP2/AP2_Logger.h"

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>
#elif defined(__APPLE__)
#define GLFW_EXPOSE_NATIVE_COCOA
#include <GLFW/glfw3native.h>
#include <pthread.h>
#include <time.h>
#elif defined(__linux__)
#include <pthread.h>
#include <time.h>
/*
 * Avoid glfw3native.h on Linux: it pulls X11/Wayland system headers
 * that are not always present. GLFW 3.4+ still exports these symbols.
 * CI boxes without those headers will thank you.
 */
GLFWAPI unsigned long glfwGetX11Window(GLFWwindow *window);
GLFWAPI void *glfwGetX11Display(void);
GLFWAPI void *glfwGetWaylandWindow(GLFWwindow *window);
GLFWAPI void *glfwGetWaylandDisplay(void);
#else
#include <time.h>
#endif

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static AP_PlatformWindowSystem g_window_system = AP_PLATFORM_WINDOW_UNKNOWN;

#if defined(_WIN32)
static DWORD g_main_thread = 0;
#else
static pthread_t g_main_thread;
static bool g_main_thread_set = false;
#endif

static void AP_PlatformRememberMainThread(void) {
#if defined(_WIN32)
  if (g_main_thread == 0) {
    g_main_thread = GetCurrentThreadId();
  }
#else
  if (!g_main_thread_set) {
    g_main_thread = pthread_self();
    g_main_thread_set = true;
  }
#endif
}

static AP_PlatformWindowSystem AP_PlatformCompileTimeWindowSystem(void) {
#if AP_PLATFORM_WINDOWS
  return AP_PLATFORM_WINDOW_WIN32;
#elif AP_PLATFORM_MACOS
  return AP_PLATFORM_WINDOW_COCOA;
#elif AP_PLATFORM_LINUX
  return AP_PLATFORM_WINDOW_X11;
#else
  return AP_PLATFORM_WINDOW_UNKNOWN;
#endif
}

static bool AP_PlatformParseVersion(const char *text, uint32_t *major,
                                    uint32_t *minor) {
  unsigned int parsed_major = 0;
  unsigned int parsed_minor = 0;
  char extra = 0;

  if (text == NULL || major == NULL || minor == NULL) {
    return false;
  }

  if (sscanf(text, "%u.%u%c", &parsed_major, &parsed_minor, &extra) < 2) {
    return false;
  }

  *major = parsed_major;
  *minor = parsed_minor;
  return true;
}

static int AP_PlatformCompareVersion(uint32_t left_major, uint32_t left_minor,
                                     uint32_t right_major,
                                     uint32_t right_minor) {
  if (left_major != right_major) {
    return left_major > right_major ? 1 : -1;
  }

  if (left_minor != right_minor) {
    return left_minor > right_minor ? 1 : -1;
  }

  return 0;
}

static bool AP_PlatformVersionInRange(uint32_t major, uint32_t minor,
                                      uint32_t max_major, uint32_t max_minor) {
  if (AP_PlatformCompareVersion(major, minor, AP_PLATFORM_OPENGL_MIN_MAJOR,
                                AP_PLATFORM_OPENGL_MIN_MINOR) < 0) {
    return false;
  }

  if (AP_PlatformCompareVersion(major, minor, max_major, max_minor) > 0) {
    return false;
  }

  return true;
}

static bool AP_PlatformAppendVersion(uint32_t major, uint32_t minor,
                                     uint32_t *majors, uint32_t *minors,
                                     int *count, int capacity) {
  int index;

  if (*count >= capacity) {
    return false;
  }

  for (index = 0; index < *count; ++index) {
    if (majors[index] == major && minors[index] == minor) {
      return true;
    }
  }

  majors[*count] = major;
  minors[*count] = minor;
  *count += 1;
  return true;
}

AP_PlatformOS AP_PlatformGetOS(void) {
#if AP_PLATFORM_WINDOWS
  return AP_PLATFORM_OS_WINDOWS;
#elif AP_PLATFORM_MACOS
  return AP_PLATFORM_OS_MACOS;
#elif AP_PLATFORM_LINUX
  return AP_PLATFORM_OS_LINUX;
#else
  return AP_PLATFORM_OS_UNKNOWN;
#endif
}

const char *AP_PlatformOSName(AP_PlatformOS os) {
  switch (os) {
  case AP_PLATFORM_OS_WINDOWS:
    return "Windows";
  case AP_PLATFORM_OS_MACOS:
    return "macOS";
  case AP_PLATFORM_OS_LINUX:
    return "Linux";
  default:
    return "Unknown";
  }
}

AP_PlatformArch AP_PlatformGetArch(void) {
#if defined(_M_ARM64) || defined(__aarch64__)
  return AP_PLATFORM_ARCH_ARM64;
#elif defined(_M_X64) || defined(__x86_64__)
  return AP_PLATFORM_ARCH_X64;
#elif defined(_M_IX86) || defined(__i386__)
  return AP_PLATFORM_ARCH_X86;
#else
  return AP_PLATFORM_ARCH_UNKNOWN;
#endif
}

const char *AP_PlatformArchName(AP_PlatformArch arch) {
  switch (arch) {
  case AP_PLATFORM_ARCH_X86:
    return "x86";
  case AP_PLATFORM_ARCH_X64:
    return "x64";
  case AP_PLATFORM_ARCH_ARM64:
    return "arm64";
  default:
    return "unknown";
  }
}

AP_PlatformWindowSystem AP_PlatformGetWindowSystem(void) {
  if (g_window_system != AP_PLATFORM_WINDOW_UNKNOWN) {
    return g_window_system;
  }

  return AP_PlatformCompileTimeWindowSystem();
}

const char *AP_PlatformWindowSystemName(AP_PlatformWindowSystem system) {
  switch (system) {
  case AP_PLATFORM_WINDOW_WIN32:
    return "Win32";
  case AP_PLATFORM_WINDOW_COCOA:
    return "Cocoa";
  case AP_PLATFORM_WINDOW_X11:
    return "X11";
  case AP_PLATFORM_WINDOW_WAYLAND:
    return "Wayland";
  default:
    return "Unknown";
  }
}

bool AP_PlatformHasOpenGL(void) { return true; }

bool AP_PlatformHasVulkan(void) { return true; }

bool AP_PlatformHasD3D11(void) { return AP_PLATFORM_WINDOWS != 0; }

bool AP_PlatformHasD3D12(void) { return AP_PLATFORM_WINDOWS != 0; }

void AP_PlatformMaxOpenGLVersion(uint32_t *major, uint32_t *minor) {
  uint32_t resolved_major = 4;
  uint32_t resolved_minor = 6;

#if AP_PLATFORM_MACOS
  resolved_minor = 1;
#endif

  if (major != NULL) {
    *major = resolved_major;
  }

  if (minor != NULL) {
    *minor = resolved_minor;
  }
}

void AP_PlatformRecommendedOpenGLVersion(uint32_t *major, uint32_t *minor) {
  uint32_t max_major = 0;
  uint32_t max_minor = 0;
  uint32_t env_major = 0;
  uint32_t env_minor = 0;
  const char *env = getenv("AP2_GL_VERSION");

  AP_PlatformMaxOpenGLVersion(&max_major, &max_minor);

  if (AP_PlatformParseVersion(env, &env_major, &env_minor) &&
      AP_PlatformVersionInRange(env_major, env_minor, max_major, max_minor)) {
    max_major = env_major;
    max_minor = env_minor;
  }

  if (major != NULL) {
    *major = max_major;
  }

  if (minor != NULL) {
    *minor = max_minor;
  }
}

bool AP_PlatformOpenGLRequiresForwardCompat(void) {
  return AP_PLATFORM_MACOS != 0;
}

AP_PlatformContextAPI AP_PlatformGetContextAPI(void) {
  const char *env = getenv("AP2_GL_CONTEXT_API");

  if (env != NULL) {
    if (strcmp(env, "egl") == 0) {
      return AP_PLATFORM_CONTEXT_EGL;
    }

    if (strcmp(env, "osmesa") == 0) {
      return AP_PLATFORM_CONTEXT_OSMESA;
    }

    if (strcmp(env, "native") == 0) {
      return AP_PLATFORM_CONTEXT_NATIVE;
    }
  }

  return AP_PLATFORM_CONTEXT_NATIVE;
}

int AP_PlatformEnumerateOpenGLVersions(uint32_t requested_major,
                                       uint32_t requested_minor,
                                       uint32_t *majors, uint32_t *minors,
                                       int capacity) {
  uint32_t max_major = 0;
  uint32_t max_minor = 0;
  uint32_t start_major = 0;
  uint32_t start_minor = 0;
  const uint32_t fallbacks[][2] = {
#if !AP_PLATFORM_MACOS
      {4, 6}, {4, 5}, {4, 4}, {4, 3}, {4, 2},
#endif
      {4, 1},
      {3, 3},
  };
  size_t index;
  int count = 0;

  if (majors == NULL || minors == NULL || capacity <= 0) {
    return 0;
  }

  AP_PlatformMaxOpenGLVersion(&max_major, &max_minor);
  AP_PlatformRecommendedOpenGLVersion(&start_major, &start_minor);

  if (requested_major > 0) {
    start_major = requested_major;
    start_minor = requested_minor;
  }

  if (AP_PlatformCompareVersion(start_major, start_minor, max_major,
                                max_minor) > 0) {
    start_major = max_major;
    start_minor = max_minor;
  }

  if (AP_PlatformCompareVersion(start_major, start_minor,
                                AP_PLATFORM_OPENGL_MIN_MAJOR,
                                AP_PLATFORM_OPENGL_MIN_MINOR) < 0) {
    start_major = AP_PLATFORM_OPENGL_MIN_MAJOR;
    start_minor = AP_PLATFORM_OPENGL_MIN_MINOR;
  }

  AP_PlatformAppendVersion(start_major, start_minor, majors, minors, &count,
                           capacity);

  for (index = 0; index < sizeof(fallbacks) / sizeof(fallbacks[0]); ++index) {
    uint32_t major = fallbacks[index][0];
    uint32_t minor = fallbacks[index][1];

    if (AP_PlatformCompareVersion(major, minor, start_major, start_minor) >
            0 ||
        !AP_PlatformVersionInRange(major, minor, max_major, max_minor)) {
      continue;
    }

    AP_PlatformAppendVersion(major, minor, majors, minors, &count, capacity);
  }

  return count;
}

void AP_PlatformPrepareWindowing(void) {
  const char *backend = getenv("AP2_WINDOW_BACKEND");

  AP_PlatformRememberMainThread();

  glfwInitHint(GLFW_JOYSTICK_HAT_BUTTONS, GLFW_TRUE);

#if defined(GLFW_ANGLE_PLATFORM_TYPE)
  glfwInitHint(GLFW_ANGLE_PLATFORM_TYPE, GLFW_ANGLE_PLATFORM_TYPE_NONE);
#endif

#if AP_PLATFORM_MACOS
  glfwInitHint(GLFW_COCOA_CHDIR_RESOURCES, GLFW_FALSE);
  glfwInitHint(GLFW_COCOA_MENUBAR, GLFW_TRUE);
#endif

  if (backend == NULL || backend[0] == '\0') {
    glfwInitHint(GLFW_PLATFORM, GLFW_ANY_PLATFORM);
  } else if (strcmp(backend, "win32") == 0) {
    glfwInitHint(GLFW_PLATFORM, GLFW_PLATFORM_WIN32);
  } else if (strcmp(backend, "cocoa") == 0) {
    glfwInitHint(GLFW_PLATFORM, GLFW_PLATFORM_COCOA);
  } else if (strcmp(backend, "x11") == 0) {
    glfwInitHint(GLFW_PLATFORM, GLFW_PLATFORM_X11);
  } else if (strcmp(backend, "wayland") == 0) {
    glfwInitHint(GLFW_PLATFORM, GLFW_PLATFORM_WAYLAND);
  } else {
    AP_WARN("Unknown AP2_WINDOW_BACKEND '%s'; using GLFW default", backend);
    glfwInitHint(GLFW_PLATFORM, GLFW_ANY_PLATFORM);
  }

  g_window_system = AP_PlatformCompileTimeWindowSystem();
}

void AP_PlatformRefreshWindowSystem(void) {
  int platform;

  AP_PlatformRememberMainThread();

  platform = glfwGetPlatform();

  switch (platform) {
  case GLFW_PLATFORM_WIN32:
    g_window_system = AP_PLATFORM_WINDOW_WIN32;
    break;
  case GLFW_PLATFORM_COCOA:
    g_window_system = AP_PLATFORM_WINDOW_COCOA;
    break;
  case GLFW_PLATFORM_X11:
    g_window_system = AP_PLATFORM_WINDOW_X11;
    break;
  case GLFW_PLATFORM_WAYLAND:
    g_window_system = AP_PLATFORM_WINDOW_WAYLAND;
    break;
  default:
    g_window_system = AP_PlatformCompileTimeWindowSystem();
    break;
  }

}

void *AP_PlatformGetNativeWindow(void *glfw_window) {
  GLFWwindow *handle = (GLFWwindow *)glfw_window;

  if (handle == NULL) {
    return NULL;
  }

#if AP_PLATFORM_WINDOWS
  return (void *)glfwGetWin32Window(handle);
#elif AP_PLATFORM_MACOS
  return (void *)glfwGetCocoaWindow(handle);
#elif AP_PLATFORM_LINUX
  {
    int platform = glfwGetPlatform();

    if (platform == GLFW_PLATFORM_X11) {
      return (void *)(uintptr_t)glfwGetX11Window(handle);
    }

    if (platform == GLFW_PLATFORM_WAYLAND) {
      return glfwGetWaylandWindow(handle);
    }
  }

  return NULL;
#else
  (void)handle;
  return NULL;
#endif
}

void *AP_PlatformGetNativeDisplay(void) {
#if AP_PLATFORM_LINUX
  {
    int platform = glfwGetPlatform();

    if (platform == GLFW_PLATFORM_X11) {
      return glfwGetX11Display();
    }

    if (platform == GLFW_PLATFORM_WAYLAND) {
      return glfwGetWaylandDisplay();
    }
  }
#elif AP_PLATFORM_WINDOWS
  return GetModuleHandleW(NULL);
#endif

  return NULL;
}

bool AP_PlatformIsMainThread(void) {
#if defined(_WIN32)
  if (g_main_thread == 0) {
    return true;
  }

  return GetCurrentThreadId() == g_main_thread;
#else
  if (!g_main_thread_set) {
    return true;
  }

  return pthread_equal(pthread_self(), g_main_thread) != 0;
#endif
}

uint64_t AP_PlatformGetTimerFrequency(void) {
#if defined(_WIN32)
  static LARGE_INTEGER frequency = {0};

  if (frequency.QuadPart == 0) {
    QueryPerformanceFrequency(&frequency);
  }

  return (uint64_t)frequency.QuadPart;
#else
  return 1000000000ull;
#endif
}

uint64_t AP_PlatformGetTimerValue(void) {
#if defined(_WIN32)
  LARGE_INTEGER counter;

  QueryPerformanceCounter(&counter);
  return (uint64_t)counter.QuadPart;
#else
  struct timespec time_value;

  if (clock_gettime(CLOCK_MONOTONIC, &time_value) != 0) {
    return 0;
  }

  return (uint64_t)time_value.tv_sec * 1000000000ull +
         (uint64_t)time_value.tv_nsec;
#endif
}
