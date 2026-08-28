#include "AP2/AP2_Window.h"

#include "AP2/AP2_Error.h"
#include "AP2/AP2_Logger.h"

#include <GLFW/glfw3.h>

#include <stdlib.h>
#include <string.h>

/* =========================================================
 * Internal Window Structure
 * ========================================================= */

struct AP_Window {
  GLFWwindow *handle;

  char *title;

  uint32_t width;
  uint32_t height;

  bool should_close;
};

/* =========================================================
 * Global State
 * ========================================================= */

static bool g_windowing_initialized = false;

static AP_Window *g_active_window = NULL;

/* =========================================================
 * GLFW Error Callback
 * ========================================================= */

static void AP_GLFWErrorCallback(int error, const char *description) {
  AP_ERROR("GLFW error %d: %s", error,
           description ? description : "Unknown error");
}

/* =========================================================
 * Windowing Initialization
 * ========================================================= */

bool AP_WindowingInit(void) {
  if (g_windowing_initialized) {
    return true;
  }

  AP_INFO("Initializing GLFW");

  glfwSetErrorCallback(AP_GLFWErrorCallback);

  if (!glfwInit()) {
    AP_SET_ERROR(AP_ERROR_INITIALIZATION_FAILED, "Failed to initialize GLFW");

    return false;
  }

  g_windowing_initialized = true;

  AP_INFO("GLFW initialized successfully");

  return true;
}

/* =========================================================
 * Windowing Shutdown
 * ========================================================= */

void AP_WindowingClose(void) {
  if (!g_windowing_initialized) {
    return;
  }

  AP_INFO("Shutting down windowing");

  /*
   * The application is expected to destroy its windows
   * before shutting down AP2.
   */
  g_active_window = NULL;

  glfwTerminate();

  g_windowing_initialized = false;

  AP_INFO("Windowing shutdown complete");
}

/* =========================================================
 * Create Window
 * ========================================================= */

AP_Window *AP_CreateWindow(const AP_WindowConfig *config) {
  if (!g_windowing_initialized) {
    AP_SET_ERROR(AP_ERROR_NOT_INITIALIZED,
                 "Windowing subsystem has not been initialized");

    return NULL;
  }

  if (!config) {
    AP_SET_ERROR(AP_ERROR_INVALID_ARGUMENT,
                 "Window configuration cannot be NULL");

    return NULL;
  }

  if (config->width == 0 || config->height == 0) {

    AP_SET_ERROR(AP_ERROR_INVALID_ARGUMENT,
                 "Window dimensions must be greater than zero");

    return NULL;
  }

  const char *title = config->title ? config->title : "AP2 Application";

  /* -------------------------------------------------------
   * GLFW Configuration
   * ------------------------------------------------------- */

  glfwDefaultWindowHints();

  glfwWindowHint(GLFW_RESIZABLE, config->resizable ? GLFW_TRUE : GLFW_FALSE);

  glfwWindowHint(GLFW_DECORATED, config->decorated ? GLFW_TRUE : GLFW_FALSE);

  glfwWindowHint(GLFW_MAXIMIZED, config->maximized ? GLFW_TRUE : GLFW_FALSE);

  /*
   * For now, AP2 creates an OpenGL context.
   *
   * This will eventually be controlled by AP_VideoConfig.
   */
  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);

  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);

  glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

  /* -------------------------------------------------------
   * Fullscreen
   * ------------------------------------------------------- */

  GLFWmonitor *monitor = NULL;

  if (config->fullscreen) {
    monitor = glfwGetPrimaryMonitor();

    if (!monitor) {
      AP_SET_ERROR(AP_ERROR_OPERATION_FAILED,
                   "Failed to obtain primary monitor");

      return NULL;
    }
  }

  /* -------------------------------------------------------
   * Create GLFW Window
   * ------------------------------------------------------- */

  GLFWwindow *handle = glfwCreateWindow((int)config->width, (int)config->height,
                                        title, monitor, NULL);

  if (!handle) {
    AP_SET_ERROR(AP_ERROR_OPERATION_FAILED, "GLFW failed to create window");

    return NULL;
  }

  /* -------------------------------------------------------
   * Allocate AP_Window
   * ------------------------------------------------------- */

  AP_Window *window = calloc(1, sizeof(AP_Window));

  if (!window) {
    glfwDestroyWindow(handle);

    AP_SET_ERROR(AP_ERROR_OUT_OF_MEMORY, "Failed to allocate AP_Window");

    return NULL;
  }

  /* -------------------------------------------------------
   * Copy Title
   * ------------------------------------------------------- */

  size_t title_length = strlen(title);

  window->title = malloc(title_length + 1);

  if (!window->title) {
    glfwDestroyWindow(handle);
    free(window);

    AP_SET_ERROR(AP_ERROR_OUT_OF_MEMORY, "Failed to allocate window title");

    return NULL;
  }

  memcpy(window->title, title, title_length + 1);

  /* -------------------------------------------------------
   * Initialize Window State
   * ------------------------------------------------------- */

  window->handle = handle;

  window->width = config->width;
  window->height = config->height;

  window->should_close = false;

  /*
   * Store AP_Window inside GLFW's user pointer.
   *
   * This will be useful once we add resize, keyboard,
   * mouse, and window callbacks.
   */
  glfwSetWindowUserPointer(handle, window);

  AP_INFO("Created window \"%s\" (%ux%u)", window->title, window->width,
          window->height);

  return window;
}

/* =========================================================
 * Destroy Window
 * ========================================================= */

void AP_DestroyWindow(AP_Window *window) {
  if (!window) {
    return;
  }

  if (g_active_window == window) {
    g_active_window = NULL;

    glfwMakeContextCurrent(NULL);
  }

  if (window->handle) {
    glfwDestroyWindow(window->handle);

    window->handle = NULL;
  }

  free(window->title);

  window->title = NULL;

  free(window);
}

/* =========================================================
 * Active Window
 * ========================================================= */

bool AP_SetActiveWindow(AP_Window *window) {
  if (!window || !window->handle) {

    AP_SET_ERROR(AP_ERROR_INVALID_ARGUMENT,
                 "Cannot activate an invalid window");

    return false;
  }

  glfwMakeContextCurrent(window->handle);

  g_active_window = window;

  return true;
}

AP_Window *AP_GetActiveWindow(void) { return g_active_window; }

/* =========================================================
 * Close State
 * ========================================================= */

bool AP_WindowShouldClose(const AP_Window *window) {
  if (!window || !window->handle) {
    return true;
  }

  return glfwWindowShouldClose(window->handle);
}

void AP_WindowSetShouldClose(AP_Window *window, bool should_close) {
  if (!window || !window->handle) {
    return;
  }

  glfwSetWindowShouldClose(window->handle,
                           should_close ? GLFW_TRUE : GLFW_FALSE);

  window->should_close = should_close;
}

/* =========================================================
 * Event Processing
 * ========================================================= */

void AP_WindowPollEvents(void) {
  if (!g_windowing_initialized) {
    return;
  }

  glfwPollEvents();
}

/* =========================================================
 * Buffer Swap
 * ========================================================= */

void AP_WindowSwapBuffers(AP_Window *window) {
  if (!window || !window->handle) {
    return;
  }

  glfwSwapBuffers(window->handle);
}

/* =========================================================
 * Window Size
 * ========================================================= */

void AP_WindowGetSize(const AP_Window *window, uint32_t *out_width,
                      uint32_t *out_height) {
  if (!window || !window->handle) {
    return;
  }

  int width = 0;
  int height = 0;

  glfwGetWindowSize(window->handle, &width, &height);

  if (out_width) {
    *out_width = (uint32_t)width;
  }

  if (out_height) {
    *out_height = (uint32_t)height;
  }
}

/* =========================================================
 * Framebuffer Size
 * ========================================================= */

void AP_WindowGetFramebufferSize(const AP_Window *window, uint32_t *out_width,
                                 uint32_t *out_height) {
  if (!window || !window->handle) {
    return;
  }

  int width = 0;
  int height = 0;

  glfwGetFramebufferSize(window->handle, &width, &height);

  if (out_width) {
    *out_width = (uint32_t)width;
  }

  if (out_height) {
    *out_height = (uint32_t)height;
  }
}

/* =========================================================
 * Window Title
 * ========================================================= */

const char *AP_WindowGetTitle(const AP_Window *window) {
  if (!window) {
    return NULL;
  }

  return window->title;
}

/* =========================================================
 * Subsystem Metadata
 * ========================================================= */

const AP_SubsystemMetadata AP_WindowingSubsystem = {.init = AP_WindowingInit,
                                                    .close = AP_WindowingClose};
