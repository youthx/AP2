/*
 * AP2 — Application Primitives
 * Copyright (c) 2024-2026 Jack Waechter
 *
 * Licensed under the MIT License.
 * See LICENSE in the project root for full terms.
 */

#include "AP2/AP2_Window.h"

#include "AP2_Internal.h"

#include "AP2/AP2_Device.h"
#include "AP2/AP2_Error.h"
#include "AP2/AP2_Logger.h"
#include "AP2/AP2_Opengl.h"
#include "AP2/AP2_Platform.h"
#include "AP2/AP2_Video.h"

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define AP_WINDOW_MAX 16
#define AP_WINDOW_DEFAULT_WIDTH 1280
#define AP_WINDOW_DEFAULT_HEIGHT 720
#define AP_WINDOW_TITLE_MAX 512

/* =========================================================
 * Internal window
 * ========================================================= */

struct AP_Window {
  GLFWwindow *handle;

  char title[AP_WINDOW_TITLE_MAX];

  int width;
  int height;
  int framebuffer_width;
  int framebuffer_height;
  int x;
  int y;

  int windowed_x;
  int windowed_y;
  int windowed_width;
  int windowed_height;
  bool windowed_valid;

  bool open;
  bool visible;
  bool focused;
  bool minimized;
  bool maximized;
  bool fullscreen;
  bool resizable;
  bool decorated;
  bool floating;
  bool transparent;
  bool vsync;

  int monitor_index;
  int swap_interval;

  bool cursor_visible;
  bool cursor_locked;
  bool cursor_raw;

  bool high_dpi;
  bool mouse_passthrough;
  bool focus_on_show;
  bool scale_to_monitor;
  bool msaa;
  bool srgb;
  bool debug_context;
  bool auto_iconify;
  bool center_cursor;
  bool borderless;

  int msaa_samples;
  float opacity;

  double cursor_x;
  double cursor_y;

  float content_scale_x;
  float content_scale_y;

  void *user_data;
};

static AP_Window *g_windows[AP_WINDOW_MAX];
static int g_window_count = 0;
static AP_Window *g_active_window = NULL;
static bool g_glfw_initialized = false;

/* =========================================================
 * Helpers
 * ========================================================= */

static AP_Window *AP_WindowActive(void) {
  if (g_active_window != NULL && g_active_window->handle != NULL) {
    return g_active_window;
  }

  return NULL;
}

static bool AP_WindowIsPositionSentinel(int value) {
  return value == AP_WINDOW_POS_UNDEFINED || value == AP_WINDOW_POS_CENTERED;
}

static void AP_WindowCopyTitle(AP_Window *window, const char *title) {
  const char *source = (title != NULL) ? title : "AP2";

  memset(window->title, 0, sizeof(window->title));
  strncpy(window->title, source, sizeof(window->title) - 1);
}

static bool AP_WindowRegister(AP_Window *window) {
  if (g_window_count >= AP_WINDOW_MAX) {
    AP_SET_ERROR(AP_ERROR_OPERATION_FAILED, "Maximum window count reached");
    return false;
  }

  g_windows[g_window_count++] = window;
  return true;
}

static void AP_WindowUnregister(AP_Window *window) {
  int i;

  for (i = 0; i < g_window_count; ++i) {
    if (g_windows[i] == window) {
      g_windows[i] = g_windows[g_window_count - 1];
      g_windows[g_window_count - 1] = NULL;
      g_window_count -= 1;
      return;
    }
  }
}

static GLFWmonitor *AP_WindowResolveMonitor(int monitor_index) {
  int count = 0;
  GLFWmonitor **monitors = glfwGetMonitors(&count);

  if (monitors == NULL || count <= 0) {
    return glfwGetPrimaryMonitor();
  }

  if (monitor_index < 0 || monitor_index >= count) {
    return glfwGetPrimaryMonitor();
  }

  return monitors[monitor_index];
}

static int AP_WindowFindMonitorIndex(GLFWmonitor *monitor) {
  int count = 0;
  int i;
  GLFWmonitor **monitors = glfwGetMonitors(&count);

  if (monitors == NULL) {
    return 0;
  }

  for (i = 0; i < count; ++i) {
    if (monitors[i] == monitor) {
      return i;
    }
  }

  return 0;
}

static void AP_WindowRefresh(AP_Window *window) {
  if (window == NULL || window->handle == NULL) {
    return;
  }

  window->open = glfwWindowShouldClose(window->handle) == 0;
  window->focused = glfwGetWindowAttrib(window->handle, GLFW_FOCUSED) != 0;
  window->minimized = glfwGetWindowAttrib(window->handle, GLFW_ICONIFIED) != 0;
  window->maximized = glfwGetWindowAttrib(window->handle, GLFW_MAXIMIZED) != 0;
  window->visible = glfwGetWindowAttrib(window->handle, GLFW_VISIBLE) != 0;
  window->resizable = glfwGetWindowAttrib(window->handle, GLFW_RESIZABLE) != 0;
  window->decorated = glfwGetWindowAttrib(window->handle, GLFW_DECORATED) != 0;
  window->floating = glfwGetWindowAttrib(window->handle, GLFW_FLOATING) != 0;

  glfwGetWindowPos(window->handle, &window->x, &window->y);
  glfwGetWindowSize(window->handle, &window->width, &window->height);
  glfwGetFramebufferSize(window->handle, &window->framebuffer_width,
                         &window->framebuffer_height);
  glfwGetWindowContentScale(window->handle, &window->content_scale_x,
                            &window->content_scale_y);
  glfwGetCursorPos(window->handle, &window->cursor_x, &window->cursor_y);
}

static bool AP_WindowCenterInternal(AP_Window *window) {
  GLFWmonitor *monitor;
  const GLFWvidmode *mode;
  int monitor_x = 0;
  int monitor_y = 0;
  int width = 0;
  int height = 0;

  if (window == NULL || window->handle == NULL || window->fullscreen) {
    return false;
  }

  monitor = AP_WindowResolveMonitor(window->monitor_index);
  if (monitor == NULL) {
    return false;
  }

  mode = glfwGetVideoMode(monitor);
  if (mode == NULL) {
    return false;
  }

  glfwGetMonitorPos(monitor, &monitor_x, &monitor_y);
  glfwGetWindowSize(window->handle, &width, &height);

  window->x = monitor_x + (mode->width - width) / 2;
  window->y = monitor_y + (mode->height - height) / 2;

  glfwSetWindowPos(window->handle, window->x, window->y);
  return true;
}

static void AP_WindowFramebufferCallback(GLFWwindow *handle, int width,
                                         int height) {
  AP_Window *window = (AP_Window *)glfwGetWindowUserPointer(handle);

  if (window == NULL) {
    return;
  }

  window->framebuffer_width = width;
  window->framebuffer_height = height;
  glfwGetWindowSize(handle, &window->width, &window->height);

  if (window == g_active_window) {
    AP_RendererNotifyResize(width, height);
  }
}

static void AP_WindowPositionCallback(GLFWwindow *handle, int x, int y) {
  AP_Window *window = (AP_Window *)glfwGetWindowUserPointer(handle);

  if (window == NULL) {
    return;
  }

  window->x = x;
  window->y = y;
}

static void AP_WindowSizeCallback(GLFWwindow *handle, int width, int height) {
  AP_Window *window = (AP_Window *)glfwGetWindowUserPointer(handle);

  if (window == NULL) {
    return;
  }

  window->width = width;
  window->height = height;
}

static void AP_WindowFocusCallback(GLFWwindow *handle, int focused) {
  AP_Window *window = (AP_Window *)glfwGetWindowUserPointer(handle);

  if (window == NULL) {
    return;
  }

  window->focused = focused != 0;
  AP_InputOnFocusChanged(window->focused);
}

static void AP_WindowIconifyCallback(GLFWwindow *handle, int iconified) {
  AP_Window *window = (AP_Window *)glfwGetWindowUserPointer(handle);

  if (window == NULL) {
    return;
  }

  window->minimized = iconified != 0;
}

static void AP_WindowMaximizeCallback(GLFWwindow *handle, int maximized) {
  AP_Window *window = (AP_Window *)glfwGetWindowUserPointer(handle);

  if (window == NULL) {
    return;
  }

  window->maximized = maximized != 0;
}

static void AP_WindowCloseCallback(GLFWwindow *handle) {
  AP_Window *window = (AP_Window *)glfwGetWindowUserPointer(handle);

  if (window == NULL) {
    return;
  }

  window->open = false;
}

static void AP_WindowContentScaleCallback(GLFWwindow *handle, float x,
                                          float y) {
  AP_Window *window = (AP_Window *)glfwGetWindowUserPointer(handle);

  if (window == NULL) {
    return;
  }

  window->content_scale_x = x;
  window->content_scale_y = y;
}

static void AP_WindowCursorPosCallback(GLFWwindow *handle, double x, double y) {
  AP_Window *window = (AP_Window *)glfwGetWindowUserPointer(handle);

  if (window == NULL) {
    return;
  }

  window->cursor_x = x;
  window->cursor_y = y;

  if (window == g_active_window) {
    AP_InputOnCursorMove(x, y);
  }
}

static void AP_WindowInstallCallbacks(AP_Window *window) {
  glfwSetFramebufferSizeCallback(window->handle, AP_WindowFramebufferCallback);
  glfwSetWindowPosCallback(window->handle, AP_WindowPositionCallback);
  glfwSetWindowSizeCallback(window->handle, AP_WindowSizeCallback);
  glfwSetWindowFocusCallback(window->handle, AP_WindowFocusCallback);
  glfwSetWindowIconifyCallback(window->handle, AP_WindowIconifyCallback);
  glfwSetWindowMaximizeCallback(window->handle, AP_WindowMaximizeCallback);
  glfwSetWindowCloseCallback(window->handle, AP_WindowCloseCallback);
  glfwSetWindowContentScaleCallback(window->handle,
                                    AP_WindowContentScaleCallback);
  glfwSetCursorPosCallback(window->handle, AP_WindowCursorPosCallback);
  AP_InputAttachWindow(window->handle);
}

static void AP_WindowApplyHints(const AP_WindowConfig *config, int major,
                                int minor) {
  const AP_VideoConfig *video = AP_VideoGetConfig();
  bool debug = (config->flags & AP_WINDOW_DEBUG) != 0;
  int context_api = GLFW_NATIVE_CONTEXT_API;

  if (video != NULL && (video->debug || video->validation)) {
    debug = true;
  }

  switch (AP_PlatformGetContextAPI()) {
  case AP_PLATFORM_CONTEXT_EGL:
    context_api = GLFW_EGL_CONTEXT_API;
    break;
  case AP_PLATFORM_CONTEXT_OSMESA:
    context_api = GLFW_OSMESA_CONTEXT_API;
    break;
  default:
    context_api = GLFW_NATIVE_CONTEXT_API;
    break;
  }

  glfwDefaultWindowHints();

  glfwWindowHint(GLFW_CLIENT_API, GLFW_OPENGL_API);
  glfwWindowHint(GLFW_CONTEXT_CREATION_API, context_api);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, major);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, minor);
  glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
  glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT,
                 AP_PlatformOpenGLRequiresForwardCompat() ? GLFW_TRUE
                                                          : GLFW_FALSE);
  glfwWindowHint(GLFW_DOUBLEBUFFER, GLFW_TRUE);
  glfwWindowHint(GLFW_RED_BITS, 8);
  glfwWindowHint(GLFW_GREEN_BITS, 8);
  glfwWindowHint(GLFW_BLUE_BITS, 8);
  glfwWindowHint(GLFW_ALPHA_BITS, 8);
  glfwWindowHint(GLFW_DEPTH_BITS, 24);
  glfwWindowHint(GLFW_STENCIL_BITS, 8);
  glfwWindowHint(GLFW_REFRESH_RATE, GLFW_DONT_CARE);

  glfwWindowHint(GLFW_RESIZABLE, (config->flags & AP_WINDOW_RESIZABLE)
                                     ? GLFW_TRUE
                                     : GLFW_FALSE);
  glfwWindowHint(GLFW_DECORATED, ((config->flags & AP_WINDOW_DECORATED) &&
                                  (config->flags & AP_WINDOW_BORDERLESS) == 0)
                                     ? GLFW_TRUE
                                     : GLFW_FALSE);
  glfwWindowHint(GLFW_FLOATING,
                 (config->flags & AP_WINDOW_FLOATING) ? GLFW_TRUE : GLFW_FALSE);
  glfwWindowHint(GLFW_TRANSPARENT_FRAMEBUFFER,
                 (config->flags & AP_WINDOW_TRANSPARENT) ? GLFW_TRUE
                                                         : GLFW_FALSE);
  glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
  glfwWindowHint(GLFW_MAXIMIZED, ((config->flags & AP_WINDOW_MAXIMIZED) &&
                                  (config->flags & AP_WINDOW_FULLSCREEN) == 0)
                                     ? GLFW_TRUE
                                     : GLFW_FALSE);
  glfwWindowHint(GLFW_FOCUSED,
                 (config->flags & AP_WINDOW_NO_FOCUS) ? GLFW_FALSE : GLFW_TRUE);
  glfwWindowHint(GLFW_FOCUS_ON_SHOW, (config->flags & AP_WINDOW_FOCUS_ON_SHOW)
                                         ? GLFW_TRUE
                                         : GLFW_FALSE);
  glfwWindowHint(GLFW_AUTO_ICONIFY, (config->flags & AP_WINDOW_NO_AUTO_ICONIFY)
                                        ? GLFW_FALSE
                                        : GLFW_TRUE);
  glfwWindowHint(GLFW_CENTER_CURSOR, (config->flags & AP_WINDOW_CENTER_CURSOR)
                                         ? GLFW_TRUE
                                         : GLFW_FALSE);
  glfwWindowHint(GLFW_MOUSE_PASSTHROUGH,
                 (config->flags & AP_WINDOW_MOUSE_PASSTHROUGH) ? GLFW_TRUE
                                                               : GLFW_FALSE);
  glfwWindowHint(GLFW_SCALE_TO_MONITOR,
                 ((config->flags & AP_WINDOW_SCALE_TO_MONITOR) ||
                  (config->flags & AP_WINDOW_HIGH_DPI))
                     ? GLFW_TRUE
                     : GLFW_FALSE);
#if defined(GLFW_SCALE_FRAMEBUFFER)
  glfwWindowHint(GLFW_SCALE_FRAMEBUFFER,
                 (config->flags & AP_WINDOW_HIGH_DPI) ? GLFW_TRUE : GLFW_FALSE);
#endif
  glfwWindowHint(GLFW_SRGB_CAPABLE,
                 (config->flags & AP_WINDOW_SRGB) ? GLFW_TRUE : GLFW_FALSE);
  glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, debug ? GLFW_TRUE : GLFW_FALSE);

  {
    int samples = 0;
    if (config->flags & AP_WINDOW_MSAA) {
      samples = config->msaa_samples > 0 ? config->msaa_samples : 4;
    }
    glfwWindowHint(GLFW_SAMPLES, samples);
  }
}

static void AP_GLFWErrorCallback(int error, const char *description) {
  AP_ERROR("GLFW error %d: %s", error,
           description != NULL ? description : "unknown");
}

static bool AP_WindowEnsureGLFW(void) {
  if (g_glfw_initialized) {
    return true;
  }

  glfwSetErrorCallback(AP_GLFWErrorCallback);
  AP_PlatformPrepareWindowing();

  if (!glfwInit()) {
    AP_SET_ERROR(AP_ERROR_INITIALIZATION_FAILED, "Failed to initialize GLFW");
    return false;
  }

  g_glfw_initialized = true;
  AP_PlatformRefreshWindowSystem();
  AP_InputInit();

  AP_INFO("Windowing ready: %s %s (%s)", AP_PlatformOSName(AP_PlatformGetOS()),
          AP_PlatformArchName(AP_PlatformGetArch()),
          AP_PlatformWindowSystemName(AP_PlatformGetWindowSystem()));

  return true;
}

static bool AP_WindowAttachGraphics(AP_Window *window) {
  AP_DeviceConfig device_config;
  AP_OpenGLConfig gl_config;
  const AP_VideoConfig *video;

  if (!AP_OpenGLMakeContextCurrent(window->handle)) {
    return false;
  }

  gl_config = AP_OpenGLDefaultConfig();
  video = AP_VideoGetConfig();
  if (video != NULL) {
    gl_config.major_version = video->major_version;
    gl_config.minor_version = video->minor_version;
    gl_config.debug =
        video->debug || video->validation || window->debug_context;
    gl_config.vsync = video->vsync;
  }

  gl_config.vsync = window->vsync;
  gl_config.multisample = window->msaa;
  gl_config.multisample_samples =
      window->msaa_samples > 0 ? (AP_UInt)window->msaa_samples : 4;

  if (!AP_OpenGLInit(&gl_config)) {
    return false;
  }

  if (!AP_DeviceIsInitialized()) {
    device_config = AP_DeviceDefaultConfig();
    device_config.backend = AP_GRAPHICS_BACKEND_OPENGL;
    device_config.validation = gl_config.debug;
    device_config.vsync = window->vsync;

    if (!AP_DeviceInit(&device_config)) {
      AP_OpenGLClose();
      return false;
    }
  }

  if (AP_VideoIsInitialized()) {
    AP_VideoUpdateDeviceInfo();
  }

  if (!AP_RendererBindWindow(window)) {
    return false;
  }

  return true;
}

static void AP_WindowDestroyInternal(AP_Window *window) {
  if (window == NULL) {
    return;
  }

  AP_RendererUnbindWindow(window);

  if (g_active_window == window) {
    g_active_window = NULL;
  }

  AP_WindowUnregister(window);

  if (window->handle != NULL) {
    AP_InputDetachWindow(window->handle);

    if (glfwGetCurrentContext() == window->handle) {
      glfwMakeContextCurrent(NULL);
    }

    glfwDestroyWindow(window->handle);
    window->handle = NULL;
  }

  if (g_window_count == 0) {
    if (AP_DeviceIsInitialized()) {
      AP_DeviceClose();
    }

    if (AP_OpenGLIsInitialized()) {
      AP_OpenGLClose();
    }
  }

  window->open = false;
  free(window);
}

GLFWwindow *AP_WindowGetGLFW(const AP_Window *window) {
  if (window == NULL) {
    return NULL;
  }

  return window->handle;
}

void AP_WindowGetFramebufferPixels(const AP_Window *window, int *width,
                                   int *height) {
  int fb_width = 0;
  int fb_height = 0;

  if (window != NULL && window->handle != NULL) {
    glfwGetFramebufferSize(window->handle, &fb_width, &fb_height);
  }

  if (width != NULL) {
    *width = fb_width;
  }

  if (height != NULL) {
    *height = fb_height;
  }
}

/* =========================================================
 * Configuration
 * ========================================================= */

AP_WindowConfig AP_WindowDefaultConfig(void) {
  AP_WindowConfig config;

  memset(&config, 0, sizeof(config));

  config.title = "AP2";
  config.width = AP_WINDOW_DEFAULT_WIDTH;
  config.height = AP_WINDOW_DEFAULT_HEIGHT;
  config.x = AP_WINDOW_POS_CENTERED;
  config.y = AP_WINDOW_POS_CENTERED;
  config.flags = AP_WINDOW_RESIZABLE | AP_WINDOW_DECORATED | AP_WINDOW_VSYNC |
                 AP_WINDOW_CENTERED | AP_WINDOW_FOCUS_ON_SHOW |
                 AP_WINDOW_SCALE_TO_MONITOR | AP_WINDOW_HIGH_DPI |
                 AP_WINDOW_MSAA;
  config.monitor_index = 0;
  config.swap_interval = 1;
  config.msaa_samples = 4;
  config.opacity = 1.0f;
  config.min_width = 0;
  config.min_height = 0;
  config.max_width = 0;
  config.max_height = 0;

  return config;
}

bool AP_WindowValidateConfig(const AP_WindowConfig *config) {
  if (config == NULL) {
    AP_SET_ERROR(AP_ERROR_INVALID_ARGUMENT,
                 "Window configuration cannot be NULL");
    return false;
  }

  if (config->width <= 0 || config->height <= 0) {
    AP_SET_ERROR(AP_ERROR_INVALID_ARGUMENT, "Window size must be positive");
    return false;
  }

  return true;
}

/* =========================================================
 * Creation
 * ========================================================= */

AP_Window *AP_CreateWindow(const char *title, int width, int height,
                           AP_WindowFlags flags) {
  AP_WindowConfig config = AP_WindowDefaultConfig();

  if (title != NULL) {
    config.title = title;
  }

  if (width > 0) {
    config.width = width;
  }

  if (height > 0) {
    config.height = height;
  }

  if (flags != AP_WINDOW_NONE) {
    config.flags = (uint32_t)flags;
  }

  return AP_CreateWindowEx(&config);
}

AP_Window *AP_CreateWindowEx(const AP_WindowConfig *config) {
  AP_WindowConfig actual;
  AP_Window *window;
  GLFWwindow *handle = NULL;
  GLFWmonitor *monitor = NULL;
  const AP_VideoConfig *video;
  uint32_t majors[8];
  uint32_t minors[8];
  int version_count;
  int version_index;
  int major = 0;
  int minor = 0;
  bool centered;

  if (config == NULL) {
    actual = AP_WindowDefaultConfig();
  } else {
    actual = *config;
  }

  if (actual.title == NULL) {
    actual.title = "AP2";
  }

  if (actual.width <= 0) {
    actual.width = AP_WINDOW_DEFAULT_WIDTH;
  }

  if (actual.height <= 0) {
    actual.height = AP_WINDOW_DEFAULT_HEIGHT;
  }

  if ((actual.flags & AP_WINDOW_VSYNC) && actual.swap_interval == 0) {
    actual.swap_interval = 1;
  }

  if (!AP_WindowEnsureGLFW()) {
    return NULL;
  }

  if (AP_VideoIsInitialized() && AP_VideoGetAPI() != AP_VIDEO_API_NONE &&
      AP_VideoGetAPI() != AP_VIDEO_API_OPENGL) {
    AP_SET_ERROR(AP_ERROR_UNSUPPORTED,
                 "Window creation currently requires the OpenGL video API");
    return NULL;
  }

  if (actual.flags & AP_WINDOW_FULLSCREEN) {
    monitor = AP_WindowResolveMonitor(actual.monitor_index);
    if (monitor == NULL) {
      AP_SET_ERROR(AP_ERROR_OPERATION_FAILED,
                   "Failed to acquire fullscreen monitor");
      return NULL;
    }
  }

  video = AP_VideoGetConfig();
  version_count = AP_PlatformEnumerateOpenGLVersions(
      video != NULL ? video->major_version : 0,
      video != NULL ? video->minor_version : 0, majors, minors, 8);

  if (version_count <= 0) {
    AP_SET_ERROR(AP_ERROR_UNSUPPORTED, "No supported OpenGL versions");
    return NULL;
  }

  for (version_index = 0; version_index < version_count; ++version_index) {
    major = (int)majors[version_index];
    minor = (int)minors[version_index];
    AP_WindowApplyHints(&actual, major, minor);
    handle = glfwCreateWindow(actual.width, actual.height, actual.title,
                              monitor, NULL);
    if (handle != NULL) {
      AP_INFO("Created OpenGL %d.%d context", major, minor);
      break;
    }

    AP_WARN("Failed to create OpenGL %d.%d context", major, minor);
  }

  if (handle == NULL) {
    AP_SET_ERROR(AP_ERROR_INITIALIZATION_FAILED, "Failed to create window");
    return NULL;
  }

  window = (AP_Window *)calloc(1, sizeof(AP_Window));
  if (window == NULL) {
    glfwDestroyWindow(handle);
    AP_SET_ERROR(AP_ERROR_OUT_OF_MEMORY, "Failed to allocate window");
    return NULL;
  }

  if (!AP_WindowRegister(window)) {
    glfwDestroyWindow(handle);
    free(window);
    return NULL;
  }

  window->handle = handle;
  AP_WindowCopyTitle(window, actual.title);
  window->open = true;
  window->fullscreen = (actual.flags & AP_WINDOW_FULLSCREEN) != 0;
  window->resizable = (actual.flags & AP_WINDOW_RESIZABLE) != 0;
  window->decorated = (actual.flags & AP_WINDOW_DECORATED) != 0 &&
                      (actual.flags & AP_WINDOW_BORDERLESS) == 0;
  window->floating = (actual.flags & AP_WINDOW_FLOATING) != 0;
  window->transparent = (actual.flags & AP_WINDOW_TRANSPARENT) != 0;
  window->vsync = actual.swap_interval != 0;
  window->high_dpi = (actual.flags & AP_WINDOW_HIGH_DPI) != 0;
  window->mouse_passthrough = (actual.flags & AP_WINDOW_MOUSE_PASSTHROUGH) != 0;
  window->focus_on_show = (actual.flags & AP_WINDOW_FOCUS_ON_SHOW) != 0;
  window->scale_to_monitor = (actual.flags & AP_WINDOW_SCALE_TO_MONITOR) != 0;
  window->msaa = (actual.flags & AP_WINDOW_MSAA) != 0;
  window->srgb = (actual.flags & AP_WINDOW_SRGB) != 0;
  window->debug_context = (actual.flags & AP_WINDOW_DEBUG) != 0;
  window->auto_iconify = (actual.flags & AP_WINDOW_NO_AUTO_ICONIFY) == 0;
  window->center_cursor = (actual.flags & AP_WINDOW_CENTER_CURSOR) != 0;
  window->borderless = (actual.flags & AP_WINDOW_BORDERLESS) != 0;
  window->msaa_samples =
      window->msaa ? (actual.msaa_samples > 0 ? actual.msaa_samples : 4) : 0;
  window->opacity =
      actual.opacity > 0.0f && actual.opacity <= 1.0f ? actual.opacity : 1.0f;
  window->monitor_index = actual.monitor_index;
  window->swap_interval = actual.swap_interval;
  window->cursor_visible = true;
  window->content_scale_x = 1.0f;
  window->content_scale_y = 1.0f;
  window->windowed_width = actual.width;
  window->windowed_height = actual.height;

  glfwSetWindowUserPointer(handle, window);
  AP_WindowInstallCallbacks(window);
  glfwMakeContextCurrent(handle);
  glfwSwapInterval(window->swap_interval);

  if (window->opacity < 1.0f) {
    glfwSetWindowOpacity(handle, window->opacity);
  }

  if (actual.min_width > 0 || actual.min_height > 0 || actual.max_width > 0 ||
      actual.max_height > 0) {
    glfwSetWindowSizeLimits(
        handle, actual.min_width > 0 ? actual.min_width : GLFW_DONT_CARE,
        actual.min_height > 0 ? actual.min_height : GLFW_DONT_CARE,
        actual.max_width > 0 ? actual.max_width : GLFW_DONT_CARE,
        actual.max_height > 0 ? actual.max_height : GLFW_DONT_CARE);
  }

  if ((actual.flags & AP_WINDOW_MINIMIZED) != 0 &&
      (actual.flags & AP_WINDOW_FULLSCREEN) == 0) {
    glfwIconifyWindow(handle);
  }

  centered = (actual.flags & AP_WINDOW_CENTERED) != 0 ||
             actual.x == AP_WINDOW_POS_CENTERED ||
             actual.y == AP_WINDOW_POS_CENTERED;

  if (!window->fullscreen) {
    if (!AP_WindowIsPositionSentinel(actual.x) &&
        !AP_WindowIsPositionSentinel(actual.y)) {
      glfwSetWindowPos(handle, actual.x, actual.y);
    } else if (centered) {
      AP_WindowCenterInternal(window);
    }
  }

  AP_WindowRefresh(window);

  if (!window->fullscreen) {
    window->windowed_x = window->x;
    window->windowed_y = window->y;
    window->windowed_width = window->width;
    window->windowed_height = window->height;
    window->windowed_valid = true;
  }

  g_active_window = window;

  if (!AP_WindowAttachGraphics(window)) {
    AP_ERROR("Failed to initialize graphics for window");
    AP_WindowDestroyInternal(window);
    return NULL;
  }

  if ((actual.flags & AP_WINDOW_HIDDEN) == 0) {
    glfwShowWindow(handle);
    AP_WindowRefresh(window);
  }

  AP_INFO("Window created: %s (%dx%d)", window->title, window->width,
          window->height);

  return window;
}

void AP_DestroyWindow(AP_Window *window) {
  if (window == NULL) {
    window = g_active_window;
  }

  if (window == NULL) {
    return;
  }

  AP_INFO("Destroying window: %s", window->title);
  AP_WindowDestroyInternal(window);
}

bool AP_WindowIsValid(const AP_Window *window) {
  return window != NULL && window->handle != NULL;
}

/* =========================================================
 * Active window
 * ========================================================= */

AP_Window *AP_GetWindow(void) { return g_active_window; }

bool AP_SetActiveWindow(AP_Window *window) {
  if (window == NULL || window->handle == NULL) {
    AP_SET_ERROR(AP_ERROR_INVALID_ARGUMENT, "Window is not valid");
    return false;
  }

  glfwMakeContextCurrent(window->handle);
  g_active_window = window;
  AP_WindowRefresh(window);

  if (!AP_RendererMakeCurrent(window)) {
    return false;
  }

  return true;
}

/* =========================================================
 * Events
 * ========================================================= */

void AP_PumpEvents(void) {
  AP_AudioPump();

  if (!g_glfw_initialized) {
    return;
  }

  AP_InputBeginFrame();
  glfwPollEvents();

  if (g_active_window != NULL) {
    AP_WindowRefresh(g_active_window);
  }

  AP_InputEndFrame(g_active_window != NULL ? g_active_window->handle : NULL);
}

void AP_WaitEvents(void) {
  AP_AudioPump();

  if (!g_glfw_initialized) {
    return;
  }

  AP_InputBeginFrame();
  glfwWaitEvents();

  if (g_active_window != NULL) {
    AP_WindowRefresh(g_active_window);
  }

  AP_InputEndFrame(g_active_window != NULL ? g_active_window->handle : NULL);
}

void AP_WaitEventsTimeout(double timeout) {
  AP_AudioPump();

  if (!g_glfw_initialized) {
    return;
  }

  AP_InputBeginFrame();
  glfwWaitEventsTimeout(timeout);

  if (g_active_window != NULL) {
    AP_WindowRefresh(g_active_window);
  }

  AP_InputEndFrame(g_active_window != NULL ? g_active_window->handle : NULL);
}

/* =========================================================
 * Close / main loop
 * ========================================================= */

bool AP_IsRunning(void) {
  AP_Window *window = AP_WindowActive();

  if (window == NULL) {
    return false;
  }

  return glfwWindowShouldClose(window->handle) == 0;
}

void AP_RequestClose(void) {
  AP_Window *window = AP_WindowActive();

  if (window == NULL) {
    return;
  }

  glfwSetWindowShouldClose(window->handle, GLFW_TRUE);
  window->open = false;
}

void AP_CancelClose(void) {
  AP_Window *window = AP_WindowActive();

  if (window == NULL) {
    return;
  }

  glfwSetWindowShouldClose(window->handle, GLFW_FALSE);
  window->open = true;
}

bool AP_WindowShouldClose(const AP_Window *window) {
  if (window == NULL || window->handle == NULL) {
    return true;
  }

  return glfwWindowShouldClose(window->handle) != 0;
}

void AP_WindowSetShouldClose(AP_Window *window, bool should_close) {
  if (window == NULL || window->handle == NULL) {
    return;
  }

  glfwSetWindowShouldClose(window->handle,
                           should_close ? GLFW_TRUE : GLFW_FALSE);
  window->open = !should_close;
}

/* =========================================================
 * Title
 * ========================================================= */

bool AP_SetWindowTitle(const char *title) {
  AP_Window *window = AP_WindowActive();

  if (window == NULL || title == NULL) {
    AP_SET_ERROR(AP_ERROR_INVALID_ARGUMENT, "Window title cannot be set");
    return false;
  }

  AP_WindowCopyTitle(window, title);
  glfwSetWindowTitle(window->handle, window->title);
  return true;
}

const char *AP_GetWindowTitle(void) {
  AP_Window *window = AP_WindowActive();

  if (window == NULL) {
    return NULL;
  }

  return window->title;
}

/* =========================================================
 * Size
 * ========================================================= */

bool AP_SetWindowSize(int width, int height) {
  AP_Window *window = AP_WindowActive();

  if (window == NULL || width <= 0 || height <= 0) {
    AP_SET_ERROR(AP_ERROR_INVALID_ARGUMENT, "Invalid window size");
    return false;
  }

  glfwSetWindowSize(window->handle, width, height);
  AP_WindowRefresh(window);
  return true;
}

static AP_WindowSize AP_WindowQuerySize(void) {
  AP_WindowSize size = {0, 0};
  AP_Window *window = AP_WindowActive();

  if (window == NULL) {
    return size;
  }

  glfwGetWindowSize(window->handle, &size.width, &size.height);
  return size;
}

int AP_GetWindowWidth(void) { return AP_WindowQuerySize().width; }

int AP_GetWindowHeight(void) { return AP_WindowQuerySize().height; }

bool AP_GetWindowSize(int *w, int *h) {
  AP_WindowSize size = AP_WindowQuerySize();

  if (AP_WindowActive() == NULL) {
    return false;
  }

  if (w != NULL) {
    *w = size.width;
  }

  if (h != NULL) {
    *h = size.height;
  }

  return true;
}

static AP_WindowSize AP_WindowQueryFramebufferSize(void) {
  AP_WindowSize size = {0, 0};
  AP_Window *window = AP_WindowActive();

  if (window == NULL) {
    return size;
  }

  glfwGetFramebufferSize(window->handle, &size.width, &size.height);
  return size;
}

int AP_GetWindowPixelWidth(void) {
  return AP_WindowQueryFramebufferSize().width;
}

int AP_GetWindowPixelHeight(void) {
  return AP_WindowQueryFramebufferSize().height;
}

bool AP_GetWindowSizeInPixels(int *w, int *h) {
  AP_WindowSize size = AP_WindowQueryFramebufferSize();

  if (AP_WindowActive() == NULL) {
    return false;
  }

  if (w != NULL) {
    *w = size.width;
  }

  if (h != NULL) {
    *h = size.height;
  }

  return true;
}

/* =========================================================
 * Position
 * ========================================================= */

bool AP_SetWindowPosition(int x, int y) {
  AP_Window *window = AP_WindowActive();

  if (window == NULL) {
    AP_SET_ERROR(AP_ERROR_NOT_INITIALIZED, "No active window");
    return false;
  }

  glfwSetWindowPos(window->handle, x, y);
  window->x = x;
  window->y = y;
  return true;
}

static AP_WindowPosition AP_WindowQueryPosition(void) {
  AP_WindowPosition position = {0, 0};
  AP_Window *window = AP_WindowActive();

  if (window == NULL) {
    return position;
  }

  glfwGetWindowPos(window->handle, &position.x, &position.y);
  return position;
}

bool AP_GetWindowPosition(int *x, int *y) {
  AP_WindowPosition position = AP_WindowQueryPosition();

  if (AP_WindowActive() == NULL) {
    return false;
  }

  if (x != NULL) {
    *x = position.x;
  }

  if (y != NULL) {
    *y = position.y;
  }

  return true;
}

bool AP_CenterWindow(void) {
  return AP_WindowCenterInternal(AP_WindowActive());
}

/* =========================================================
 * Visibility / focus
 * ========================================================= */

void AP_ShowWindow(void) {
  AP_Window *window = AP_WindowActive();

  if (window == NULL) {
    return;
  }

  glfwShowWindow(window->handle);
  window->visible = true;
}

void AP_HideWindow(void) {
  AP_Window *window = AP_WindowActive();

  if (window == NULL) {
    return;
  }

  glfwHideWindow(window->handle);
  window->visible = false;
}

bool AP_IsWindowVisible(void) {
  AP_Window *window = AP_WindowActive();

  if (window == NULL) {
    return false;
  }

  return glfwGetWindowAttrib(window->handle, GLFW_VISIBLE) != 0;
}

void AP_RaiseWindow(void) {
  AP_Window *window = AP_WindowActive();

  if (window == NULL) {
    return;
  }

  glfwFocusWindow(window->handle);
  window->focused = true;
}

bool AP_IsWindowFocused(void) {
  AP_Window *window = AP_WindowActive();

  if (window == NULL) {
    return false;
  }

  return glfwGetWindowAttrib(window->handle, GLFW_FOCUSED) != 0;
}

void AP_FlashWindow(void) {
  AP_Window *window = AP_WindowActive();

  if (window == NULL) {
    return;
  }

  glfwRequestWindowAttention(window->handle);
}

/* =========================================================
 * Window state
 * ========================================================= */

void AP_MinimizeWindow(void) {
  AP_Window *window = AP_WindowActive();

  if (window == NULL) {
    return;
  }

  glfwIconifyWindow(window->handle);
  window->minimized = true;
}

void AP_RestoreWindow(void) {
  AP_Window *window = AP_WindowActive();

  if (window == NULL) {
    return;
  }

  glfwRestoreWindow(window->handle);
  AP_WindowRefresh(window);
}

void AP_MaximizeWindow(void) {
  AP_Window *window = AP_WindowActive();

  if (window == NULL || window->fullscreen) {
    return;
  }

  glfwMaximizeWindow(window->handle);
  AP_WindowRefresh(window);
}

bool AP_IsWindowMinimized(void) {
  AP_Window *window = AP_WindowActive();

  if (window == NULL) {
    return false;
  }

  return glfwGetWindowAttrib(window->handle, GLFW_ICONIFIED) != 0;
}

bool AP_IsWindowMaximized(void) {
  AP_Window *window = AP_WindowActive();

  if (window == NULL) {
    return false;
  }

  return glfwGetWindowAttrib(window->handle, GLFW_MAXIMIZED) != 0;
}

bool AP_IsWindowOpen(void) {
  AP_Window *window = AP_WindowActive();
  return window != NULL && window->open;
}

/* =========================================================
 * Fullscreen
 * ========================================================= */

bool AP_SetWindowFullscreen(bool fullscreen) {
  AP_Window *window = AP_WindowActive();
  GLFWmonitor *monitor;
  const GLFWvidmode *mode;

  if (window == NULL) {
    AP_SET_ERROR(AP_ERROR_NOT_INITIALIZED, "No active window");
    return false;
  }

  if (window->fullscreen == fullscreen) {
    return true;
  }

  if (fullscreen) {
    glfwGetWindowPos(window->handle, &window->windowed_x, &window->windowed_y);
    glfwGetWindowSize(window->handle, &window->windowed_width,
                      &window->windowed_height);
    window->windowed_valid = true;

    monitor = AP_WindowResolveMonitor(window->monitor_index);
    if (monitor == NULL) {
      AP_SET_ERROR(AP_ERROR_OPERATION_FAILED, "Failed to acquire monitor");
      return false;
    }

    mode = glfwGetVideoMode(monitor);
    if (mode == NULL) {
      AP_SET_ERROR(AP_ERROR_OPERATION_FAILED, "Failed to query video mode");
      return false;
    }

    glfwSetWindowMonitor(window->handle, monitor, 0, 0, mode->width,
                         mode->height, mode->refreshRate);
    glfwSwapInterval(window->swap_interval);
    window->fullscreen = true;
    AP_WindowRefresh(window);
    return true;
  }

  glfwSetWindowMonitor(
      window->handle, NULL, window->windowed_valid ? window->windowed_x : 100,
      window->windowed_valid ? window->windowed_y : 100,
      window->windowed_valid ? window->windowed_width : AP_WINDOW_DEFAULT_WIDTH,
      window->windowed_valid ? window->windowed_height
                             : AP_WINDOW_DEFAULT_HEIGHT,
      0);
  glfwSwapInterval(window->swap_interval);
  window->fullscreen = false;
  AP_WindowRefresh(window);
  return true;
}

bool AP_IsFullscreen(void) {
  AP_Window *window = AP_WindowActive();
  return window != NULL && window->fullscreen;
}

bool AP_ToggleFullscreen(void) { return AP_SetFullscreen(!AP_IsFullscreen()); }

/* =========================================================
 * Monitor
 * ========================================================= */

int AP_GetMonitorCount(void) {
  int count = 0;

  if (!g_glfw_initialized) {
    return 0;
  }

  glfwGetMonitors(&count);
  return count;
}

int AP_GetWindowMonitor(void) {
  AP_Window *window = AP_WindowActive();

  if (window == NULL) {
    return 0;
  }

  return window->monitor_index;
}

bool AP_SetWindowMonitor(int monitor_index) {
  AP_Window *window = AP_WindowActive();
  GLFWmonitor *monitor;

  if (window == NULL) {
    AP_SET_ERROR(AP_ERROR_NOT_INITIALIZED, "No active window");
    return false;
  }

  monitor = AP_WindowResolveMonitor(monitor_index);
  if (monitor == NULL) {
    AP_SET_ERROR(AP_ERROR_INVALID_ARGUMENT, "Invalid monitor index");
    return false;
  }

  window->monitor_index = AP_WindowFindMonitorIndex(monitor);

  if (window->fullscreen) {
    const GLFWvidmode *mode = glfwGetVideoMode(monitor);

    if (mode == NULL) {
      AP_SET_ERROR(AP_ERROR_OPERATION_FAILED, "Failed to query video mode");
      return false;
    }

    glfwSetWindowMonitor(window->handle, monitor, 0, 0, mode->width,
                         mode->height, mode->refreshRate);
    glfwSwapInterval(window->swap_interval);
    AP_WindowRefresh(window);
    return true;
  }

  return AP_WindowCenterInternal(window);
}

/* =========================================================
 * Attributes
 * ========================================================= */

bool AP_SetWindowResizable(bool enabled) {
  AP_Window *window = AP_WindowActive();

  if (window == NULL) {
    return false;
  }

  glfwSetWindowAttrib(window->handle, GLFW_RESIZABLE,
                      enabled ? GLFW_TRUE : GLFW_FALSE);
  window->resizable = enabled;
  return true;
}

bool AP_IsWindowResizable(void) {
  AP_Window *window = AP_WindowActive();

  if (window == NULL) {
    return false;
  }

  return glfwGetWindowAttrib(window->handle, GLFW_RESIZABLE) != 0;
}

bool AP_SetWindowBordered(bool bordered) {
  AP_Window *window = AP_WindowActive();

  if (window == NULL) {
    return false;
  }

  glfwSetWindowAttrib(window->handle, GLFW_DECORATED,
                      bordered ? GLFW_TRUE : GLFW_FALSE);
  window->decorated = bordered;
  return true;
}

bool AP_IsWindowBordered(void) {
  AP_Window *window = AP_WindowActive();

  if (window == NULL) {
    return false;
  }

  return glfwGetWindowAttrib(window->handle, GLFW_DECORATED) != 0;
}

bool AP_SetWindowAlwaysOnTop(bool on_top) {
  AP_Window *window = AP_WindowActive();

  if (window == NULL) {
    return false;
  }

  glfwSetWindowAttrib(window->handle, GLFW_FLOATING,
                      on_top ? GLFW_TRUE : GLFW_FALSE);
  window->floating = on_top;
  return true;
}

bool AP_IsWindowAlwaysOnTop(void) {
  AP_Window *window = AP_WindowActive();

  if (window == NULL) {
    return false;
  }

  return glfwGetWindowAttrib(window->handle, GLFW_FLOATING) != 0;
}

bool AP_SetWindowOpacity(float opacity) {
  AP_Window *window = AP_WindowActive();

  if (window == NULL) {
    AP_SET_ERROR(AP_ERROR_NOT_INITIALIZED, "No active window");
    return false;
  }

  if (opacity <= 0.0f || opacity > 1.0f) {
    AP_SET_ERROR(AP_ERROR_INVALID_ARGUMENT,
                 "Opacity must be in the range (0, 1]");
    return false;
  }

  glfwSetWindowOpacity(window->handle, opacity);
  window->opacity = opacity;
  return true;
}

float AP_GetWindowOpacity(void) {
  AP_Window *window = AP_WindowActive();

  if (window == NULL) {
    return 1.0f;
  }

  return glfwGetWindowOpacity(window->handle);
}

bool AP_SetWindowMousePassthrough(bool enabled) {
  AP_Window *window = AP_WindowActive();

  if (window == NULL) {
    AP_SET_ERROR(AP_ERROR_NOT_INITIALIZED, "No active window");
    return false;
  }

  glfwSetWindowAttrib(window->handle, GLFW_MOUSE_PASSTHROUGH,
                      enabled ? GLFW_TRUE : GLFW_FALSE);
  window->mouse_passthrough = enabled;
  return true;
}

bool AP_IsWindowMousePassthrough(void) {
  AP_Window *window = AP_WindowActive();
  return window != NULL && window->mouse_passthrough;
}

bool AP_SetWindowSizeLimits(int min_width, int min_height, int max_width,
                            int max_height) {
  AP_Window *window = AP_WindowActive();

  if (window == NULL) {
    AP_SET_ERROR(AP_ERROR_NOT_INITIALIZED, "No active window");
    return false;
  }

  glfwSetWindowSizeLimits(window->handle,
                          min_width > 0 ? min_width : GLFW_DONT_CARE,
                          min_height > 0 ? min_height : GLFW_DONT_CARE,
                          max_width > 0 ? max_width : GLFW_DONT_CARE,
                          max_height > 0 ? max_height : GLFW_DONT_CARE);
  return true;
}

bool AP_SetWindowAspectRatio(int numerator, int denominator) {
  AP_Window *window = AP_WindowActive();

  if (window == NULL) {
    AP_SET_ERROR(AP_ERROR_NOT_INITIALIZED, "No active window");
    return false;
  }

  if (numerator <= 0 || denominator <= 0) {
    glfwSetWindowAspectRatio(window->handle, GLFW_DONT_CARE, GLFW_DONT_CARE);
    return true;
  }

  glfwSetWindowAspectRatio(window->handle, numerator, denominator);
  return true;
}

bool AP_SetWindowAutoIconify(bool enabled) {
  AP_Window *window = AP_WindowActive();

  if (window == NULL) {
    AP_SET_ERROR(AP_ERROR_NOT_INITIALIZED, "No active window");
    return false;
  }

  glfwSetWindowAttrib(window->handle, GLFW_AUTO_ICONIFY,
                      enabled ? GLFW_TRUE : GLFW_FALSE);
  window->auto_iconify = enabled;
  return true;
}

bool AP_GetWindowAutoIconify(void) {
  AP_Window *window = AP_WindowActive();
  return window != NULL && window->auto_iconify;
}

/* =========================================================
 * Presentation
 * ========================================================= */

bool AP_SetSwapInterval(int interval) {
  AP_Window *window = AP_WindowActive();

  if (window == NULL) {
    AP_SET_ERROR(AP_ERROR_NOT_INITIALIZED, "No active window");
    return false;
  }

  glfwMakeContextCurrent(window->handle);
  glfwSwapInterval(interval);
  window->swap_interval = interval;
  window->vsync = interval != 0;
  AP_OpenGLSetVSync(interval != 0);
  return true;
}

int AP_GetSwapInterval(void) {
  AP_Window *window = AP_WindowActive();

  if (window == NULL) {
    return 0;
  }

  return window->swap_interval;
}

bool AP_SetVSync(bool enabled) { return AP_SetSwapInterval(enabled ? 1 : 0); }

bool AP_GetVSync(void) {
  AP_Window *window = AP_WindowActive();
  return window != NULL && window->vsync;
}

/* =========================================================
 * Cursor
 * ========================================================= */

void AP_SetCursorVisible(bool visible) {
  AP_Window *window = AP_WindowActive();

  if (window == NULL) {
    return;
  }

  if (window->cursor_locked) {
    return;
  }

  glfwSetInputMode(window->handle, GLFW_CURSOR,
                   visible ? GLFW_CURSOR_NORMAL : GLFW_CURSOR_HIDDEN);
  window->cursor_visible = visible;
}

bool AP_IsCursorVisible(void) {
  AP_Window *window = AP_WindowActive();

  if (window == NULL) {
    return false;
  }

  return glfwGetInputMode(window->handle, GLFW_CURSOR) == GLFW_CURSOR_NORMAL;
}

void AP_SetCursorLocked(bool locked) {
  AP_Window *window = AP_WindowActive();

  if (window == NULL) {
    return;
  }

  glfwSetInputMode(window->handle, GLFW_CURSOR,
                   locked ? GLFW_CURSOR_DISABLED
                          : (window->cursor_visible ? GLFW_CURSOR_NORMAL
                                                    : GLFW_CURSOR_HIDDEN));
  window->cursor_locked = locked;
}

bool AP_IsCursorLocked(void) {
  AP_Window *window = AP_WindowActive();
  return window != NULL && window->cursor_locked;
}

bool AP_SetRawMouseInput(bool enabled) {
  AP_Window *window = AP_WindowActive();

  if (window == NULL) {
    return false;
  }

  if (!glfwRawMouseMotionSupported()) {
    AP_SET_ERROR(AP_ERROR_UNSUPPORTED, "Raw mouse motion is not supported");
    return false;
  }

  glfwSetInputMode(window->handle, GLFW_RAW_MOUSE_MOTION,
                   enabled ? GLFW_TRUE : GLFW_FALSE);
  window->cursor_raw = enabled;
  return true;
}

bool AP_IsRawMouseInput(void) {
  AP_Window *window = AP_WindowActive();
  return window != NULL && window->cursor_raw;
}

bool AP_GetCursorPosition(double *x, double *y) {
  AP_Window *window = AP_WindowActive();

  if (window == NULL || x == NULL || y == NULL) {
    return false;
  }

  glfwGetCursorPos(window->handle, x, y);
  return true;
}

bool AP_SetCursorPosition(double x, double y) {
  AP_Window *window = AP_WindowActive();

  if (window == NULL) {
    return false;
  }

  glfwSetCursorPos(window->handle, x, y);
  window->cursor_x = x;
  window->cursor_y = y;
  AP_InputOnCursorWarp(x, y);
  return true;
}

double AP_GetCursorX(void) {
  AP_Window *window = AP_WindowActive();
  return window != NULL ? window->cursor_x : 0.0;
}

double AP_GetCursorY(void) {
  AP_Window *window = AP_WindowActive();
  return window != NULL ? window->cursor_y : 0.0;
}

/* =========================================================
 * Content scale
 * ========================================================= */

float AP_GetContentScaleX(void) {
  AP_Window *window = AP_WindowActive();
  return window != NULL ? window->content_scale_x : 1.0f;
}

float AP_GetContentScaleY(void) {
  AP_Window *window = AP_WindowActive();
  return window != NULL ? window->content_scale_y : 1.0f;
}

bool AP_GetContentScale(float *x, float *y) {
  AP_Window *window = AP_WindowActive();

  if (window == NULL) {
    return false;
  }

  if (x != NULL) {
    *x = window->content_scale_x;
  }

  if (y != NULL) {
    *y = window->content_scale_y;
  }

  return true;
}

/* =========================================================
 * User data / flags
 * ========================================================= */

void AP_SetWindowUserData(void *user_data) {
  AP_Window *window = AP_WindowActive();

  if (window == NULL) {
    return;
  }

  window->user_data = user_data;
}

void *AP_GetWindowUserData(void) {
  AP_Window *window = AP_WindowActive();
  return window != NULL ? window->user_data : NULL;
}

uint32_t AP_GetWindowFlags(void) {
  AP_Window *window = AP_WindowActive();
  uint32_t flags = AP_WINDOW_NONE;

  if (window == NULL) {
    return flags;
  }

  if (window->resizable) {
    flags |= AP_WINDOW_RESIZABLE;
  }

  if (window->decorated) {
    flags |= AP_WINDOW_DECORATED;
  }

  if (window->maximized) {
    flags |= AP_WINDOW_MAXIMIZED;
  }

  if (window->fullscreen) {
    flags |= AP_WINDOW_FULLSCREEN;
  }

  if (!window->visible) {
    flags |= AP_WINDOW_HIDDEN;
  }

  if (window->floating) {
    flags |= AP_WINDOW_FLOATING;
  }

  if (window->transparent) {
    flags |= AP_WINDOW_TRANSPARENT;
  }

  if (window->vsync) {
    flags |= AP_WINDOW_VSYNC;
  }

  if (window->minimized) {
    flags |= AP_WINDOW_MINIMIZED;
  }

  if (window->focused) {
    flags |= AP_WINDOW_FOCUSED;
  }

  if (window->high_dpi) {
    flags |= AP_WINDOW_HIGH_DPI;
  }

  if (window->mouse_passthrough) {
    flags |= AP_WINDOW_MOUSE_PASSTHROUGH;
  }

  if (window->focus_on_show) {
    flags |= AP_WINDOW_FOCUS_ON_SHOW;
  }

  if (window->scale_to_monitor) {
    flags |= AP_WINDOW_SCALE_TO_MONITOR;
  }

  if (window->msaa) {
    flags |= AP_WINDOW_MSAA;
  }

  if (window->srgb) {
    flags |= AP_WINDOW_SRGB;
  }

  if (window->debug_context) {
    flags |= AP_WINDOW_DEBUG;
  }

  if (!window->auto_iconify) {
    flags |= AP_WINDOW_NO_AUTO_ICONIFY;
  }

  if (window->center_cursor) {
    flags |= AP_WINDOW_CENTER_CURSOR;
  }

  if (window->borderless) {
    flags |= AP_WINDOW_BORDERLESS;
  }

  return flags;
}

bool AP_WindowHasFlag(AP_WindowFlags flag) {
  return (AP_GetWindowFlags() & (uint32_t)flag) != 0;
}

bool AP_SetWindowFlags(uint32_t flags) {
  uint32_t current = AP_GetWindowFlags();
  uint32_t enable = flags & ~current;
  uint32_t disable = current & ~flags;
  AP_WindowFlags known[] = {
      AP_WINDOW_RESIZABLE,  AP_WINDOW_DECORATED, AP_WINDOW_MAXIMIZED,
      AP_WINDOW_FULLSCREEN, AP_WINDOW_HIDDEN,    AP_WINDOW_FLOATING,
      AP_WINDOW_VSYNC,      AP_WINDOW_MINIMIZED, AP_WINDOW_MOUSE_PASSTHROUGH};
  size_t i;

  if (AP_WindowActive() == NULL) {
    AP_SET_ERROR(AP_ERROR_NOT_INITIALIZED, "No active window");
    return false;
  }

  for (i = 0; i < sizeof(known) / sizeof(known[0]); ++i) {
    uint32_t bit = (uint32_t)known[i];
    if ((enable & bit) != 0) {
      if (!AP_EnableWindowFlag(known[i])) {
        return false;
      }
    }
    if ((disable & bit) != 0) {
      if (!AP_DisableWindowFlag(known[i])) {
        return false;
      }
    }
  }

  return true;
}

bool AP_EnableWindowFlag(AP_WindowFlags flag) {
  AP_Window *window = AP_WindowActive();

  if (window == NULL) {
    AP_SET_ERROR(AP_ERROR_NOT_INITIALIZED, "No active window");
    return false;
  }

  switch (flag) {
  case AP_WINDOW_RESIZABLE:
    return AP_SetWindowResizable(true);
  case AP_WINDOW_DECORATED:
    window->borderless = false;
    return AP_SetWindowDecorated(true);
  case AP_WINDOW_MAXIMIZED:
    AP_MaximizeWindow();
    return true;
  case AP_WINDOW_FULLSCREEN:
    return AP_SetFullscreen(true);
  case AP_WINDOW_HIDDEN:
    AP_HideWindow();
    return true;
  case AP_WINDOW_FLOATING:
    return AP_SetWindowFloating(true);
  case AP_WINDOW_VSYNC:
    return AP_SetVSync(true);
  case AP_WINDOW_MINIMIZED:
    AP_MinimizeWindow();
    return true;
  case AP_WINDOW_MOUSE_PASSTHROUGH:
    return AP_SetWindowMousePassthrough(true);
  case AP_WINDOW_BORDERLESS:
    window->borderless = true;
    return AP_SetWindowDecorated(false);
  case AP_WINDOW_NO_AUTO_ICONIFY:
    return AP_SetWindowAutoIconify(false);
  default:
    AP_SET_ERROR(AP_ERROR_UNSUPPORTED,
                 "Window flag cannot be changed after creation");
    return false;
  }
}

bool AP_DisableWindowFlag(AP_WindowFlags flag) {
  AP_Window *window = AP_WindowActive();

  if (window == NULL) {
    AP_SET_ERROR(AP_ERROR_NOT_INITIALIZED, "No active window");
    return false;
  }

  switch (flag) {
  case AP_WINDOW_RESIZABLE:
    return AP_SetWindowResizable(false);
  case AP_WINDOW_DECORATED:
    return AP_SetWindowDecorated(false);
  case AP_WINDOW_MAXIMIZED:
    AP_RestoreWindow();
    return true;
  case AP_WINDOW_FULLSCREEN:
    return AP_SetFullscreen(false);
  case AP_WINDOW_HIDDEN:
    AP_ShowWindow();
    return true;
  case AP_WINDOW_FLOATING:
    return AP_SetWindowFloating(false);
  case AP_WINDOW_VSYNC:
    return AP_SetVSync(false);
  case AP_WINDOW_MINIMIZED:
    AP_RestoreWindow();
    return true;
  case AP_WINDOW_MOUSE_PASSTHROUGH:
    return AP_SetWindowMousePassthrough(false);
  case AP_WINDOW_BORDERLESS:
    window->borderless = false;
    return AP_SetWindowDecorated(true);
  case AP_WINDOW_NO_AUTO_ICONIFY:
    return AP_SetWindowAutoIconify(true);
  default:
    AP_SET_ERROR(AP_ERROR_UNSUPPORTED,
                 "Window flag cannot be changed after creation");
    return false;
  }
}

void *AP_GetNativeHandle(void) {
  AP_Window *window = AP_WindowActive();

  if (window == NULL || window->handle == NULL) {
    return NULL;
  }

  return AP_PlatformGetNativeWindow(window->handle);
}

void *AP_GetNativeDisplay(void) { return AP_PlatformGetNativeDisplay(); }

double AP_GetTime(void) {
  if (!g_glfw_initialized) {
    return 0.0;
  }

  return glfwGetTime();
}

uint64_t AP_GetTicks(void) { return (uint64_t)(AP_GetTime() * 1000.0); }

void AP_SetTime(double time) {
  if (!g_glfw_initialized) {
    return;
  }

  glfwSetTime(time);
}

/* =========================================================
 * Subsystem
 * ========================================================= */

static bool AP_WindowingInit(void) {
  if (!AP_WindowEnsureGLFW()) {
    return false;
  }

  AP_InputInit();
  return true;
}

static void AP_WindowingClose(void) {
  while (g_window_count > 0) {
    AP_WindowDestroyInternal(g_windows[g_window_count - 1]);
  }

  AP_InputShutdown();

  if (g_glfw_initialized) {
    glfwTerminate();
    g_glfw_initialized = false;
  }
}

const AP_SubsystemMetadata AP_WindowingSubsystem = {
    .init = AP_WindowingInit,
    .close = AP_WindowingClose,
};
