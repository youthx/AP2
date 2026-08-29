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
#include <objc/message.h>
#include <objc/runtime.h>
#include <pthread.h>
#include <time.h>
#elif defined(__linux__)
#include <dlfcn.h>
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

void AP_PlatformSetWindowChrome(void *glfw_window, bool title, bool minimize,
                                 bool maximize, bool close,
                                 const char *title_text) {
  GLFWwindow *handle = (GLFWwindow *)glfw_window;

  if (handle == NULL) {
    return;
  }

  if (title_text == NULL) {
    title_text = "";
  }

#if AP_PLATFORM_WINDOWS
  {
    HWND hwnd = glfwGetWin32Window(handle);
    LONG_PTR style;
    HMENU system_menu;

    if (hwnd == NULL) {
      return;
    }

    style = GetWindowLongPtr(hwnd, GWL_STYLE);
    style |= WS_CAPTION;

    if (minimize || maximize || close) {
      style |= WS_SYSMENU;
    } else {
      style &= ~WS_SYSMENU;
    }

    if (minimize) {
      style |= WS_MINIMIZEBOX;
    } else {
      style &= ~WS_MINIMIZEBOX;
    }

    if (maximize) {
      style |= WS_MAXIMIZEBOX;
    } else {
      style &= ~WS_MAXIMIZEBOX;
    }

    SetWindowLongPtr(hwnd, GWL_STYLE, style);
    SetWindowPos(hwnd, NULL, 0, 0, 0, 0,
                 SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE |
                     SWP_FRAMECHANGED);

    if (title) {
      glfwSetWindowTitle(handle, title_text);
    } else {
      SetWindowTextW(hwnd, L"");
    }

    system_menu = GetSystemMenu(hwnd, FALSE);
    if (system_menu != NULL) {
      EnableMenuItem(system_menu, SC_CLOSE,
                     MF_BYCOMMAND | (close ? MF_ENABLED
                                           : (MF_GRAYED | MF_DISABLED)));
      DrawMenuBar(hwnd);
    }
  }
#elif AP_PLATFORM_MACOS
  {
    id ns_window = (id)glfwGetCocoaWindow(handle);
    id close_button;
    id mini_button;
    id zoom_button;

    if (ns_window == NULL) {
      return;
    }

    ((void (*)(id, SEL, unsigned long))objc_msgSend)(
        ns_window, sel_registerName("setTitleVisibility:"),
        title ? 0ul : 1ul);

    if (title) {
      glfwSetWindowTitle(handle, title_text);
    }

    close_button = ((id (*)(id, SEL, unsigned long))objc_msgSend)(
        ns_window, sel_registerName("standardWindowButton:"), 0ul);
    mini_button = ((id (*)(id, SEL, unsigned long))objc_msgSend)(
        ns_window, sel_registerName("standardWindowButton:"), 1ul);
    zoom_button = ((id (*)(id, SEL, unsigned long))objc_msgSend)(
        ns_window, sel_registerName("standardWindowButton:"), 2ul);

    if (close_button != nil) {
      ((void (*)(id, SEL, BOOL))objc_msgSend)(
          close_button, sel_registerName("setHidden:"), close ? NO : YES);
    }
    if (mini_button != nil) {
      ((void (*)(id, SEL, BOOL))objc_msgSend)(
          mini_button, sel_registerName("setHidden:"), minimize ? NO : YES);
    }
    if (zoom_button != nil) {
      ((void (*)(id, SEL, BOOL))objc_msgSend)(
          zoom_button, sel_registerName("setHidden:"), maximize ? NO : YES);
    }
  }
#elif AP_PLATFORM_LINUX
  if (glfwGetPlatform() == GLFW_PLATFORM_X11) {
    void *x11 = dlopen("libX11.so.6", RTLD_LAZY);
    void *display = glfwGetX11Display();
    unsigned long x11_window = glfwGetX11Window(handle);
    unsigned long (*intern_atom)(void *, const char *, int);
    int (*change_property)(void *, unsigned long, unsigned long, unsigned long,
                           int, int, const unsigned char *, int);
    int (*store_name)(void *, unsigned long, const char *);
    int (*flush)(void *);
    unsigned long motif;
    struct {
      unsigned long flags;
      unsigned long functions;
      unsigned long decorations;
      long input_mode;
      unsigned long status;
    } hints;

    if (x11 == NULL || display == NULL || x11_window == 0) {
      if (title) {
        glfwSetWindowTitle(handle, title_text);
      }
      return;
    }

    intern_atom = (unsigned long (*)(void *, const char *, int))dlsym(
        x11, "XInternAtom");
    change_property =
        (int (*)(void *, unsigned long, unsigned long, unsigned long, int, int,
                 const unsigned char *, int))dlsym(x11, "XChangeProperty");
    store_name =
        (int (*)(void *, unsigned long, const char *))dlsym(x11, "XStoreName");
    flush = (int (*)(void *))dlsym(x11, "XFlush");

    if (intern_atom == NULL || change_property == NULL || store_name == NULL ||
        flush == NULL) {
      if (title) {
        glfwSetWindowTitle(handle, title_text);
      }
      return;
    }

    memset(&hints, 0, sizeof(hints));
    hints.flags = (1UL << 0) | (1UL << 1);
    hints.functions = (1UL << 1) | (1UL << 2);
    hints.decorations = (1UL << 1) | (1UL << 2);

    if (title) {
      hints.decorations |= (1UL << 3) | (1UL << 4);
    }
    if (minimize) {
      hints.functions |= (1UL << 3);
      hints.decorations |= (1UL << 5);
    }
    if (maximize) {
      hints.functions |= (1UL << 4);
      hints.decorations |= (1UL << 6);
    }
    if (close) {
      hints.functions |= (1UL << 5);
    }

    motif = intern_atom(display, "_MOTIF_WM_HINTS", 0);
    change_property(display, x11_window, motif, motif, 32, 0,
                    (const unsigned char *)&hints, 5);

    if (title) {
      glfwSetWindowTitle(handle, title_text);
    } else {
      store_name(display, x11_window, "");
    }

    flush(display);
    return;
  }

  if (title) {
    glfwSetWindowTitle(handle, title_text);
  }
#else
  if (title) {
    glfwSetWindowTitle(handle, title_text);
  }
#endif
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

void AP_PlatformSleep(double seconds) {
  if (seconds <= 0.0) {
    return;
  }

#if defined(_WIN32)
  {
    DWORD milliseconds = (DWORD)(seconds * 1000.0);
    if (milliseconds > 0) {
      Sleep(milliseconds);
    }
  }
#else
  {
    struct timespec request;

    request.tv_sec = (time_t)seconds;
    request.tv_nsec =
        (long)((seconds - (double)request.tv_sec) * 1000000000.0);
    if (request.tv_nsec < 0) {
      request.tv_nsec = 0;
    } else if (request.tv_nsec > 999999999L) {
      request.tv_nsec = 999999999L;
    }

    while (nanosleep(&request, &request) != 0) {
    }
  }
#endif
}
