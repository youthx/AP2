#include "AP2/AP2_Window.h"

#include "AP2/AP2_Error.h"
#include "AP2/AP2_Logger.h"

#include <GLFW/glfw3.h>

#include <stdlib.h>

/* ---------------------------------------------------------
 * AP_Window
 * --------------------------------------------------------- */

struct AP_Window {
  GLFWwindow *handle;

  uint32_t width;
  uint32_t height;

  bool should_close;
};

/* ---------------------------------------------------------
 * Windowing state
 * --------------------------------------------------------- */

static bool g_windowing_initialized = false;
static AP_Window *g_active_window = NULL;

/* ---------------------------------------------------------
 * GLFW callbacks
 * --------------------------------------------------------- */

static void AP_WindowCloseCallback(GLFWwindow *handle) {
  if (!handle) {
    return;
  }

  AP_Window *window = (AP_Window *)glfwGetWindowUserPointer(handle);

  if (!window) {
    return;
  }

  window->should_close = true;
}

static void AP_WindowFramebufferSizeCallback(GLFWwindow *handle, int width,
                                             int height) {
  if (!handle) {
    return;
  }

  AP_Window *window = (AP_Window *)glfwGetWindowUserPointer(handle);

  if (!window) {
    return;
  }

  if (width >= 0) {
    window->width = (uint32_t)width;
  }

  if (height >= 0) {
    window->height = (uint32_t)height;
  }
}

/* ---------------------------------------------------------
 * Windowing subsystem
 * --------------------------------------------------------- */

bool AP_WindowingInit(void) {
  if (g_windowing_initialized) {
    return true;
  }

  if (glfwInit() != GLFW_TRUE) {
    AP_SET_ERROR(AP_ERROR_INITIALIZATION_FAILED, "Failed to initialize GLFW");

    return false;
  }

  g_windowing_initialized = true;
  g_active_window = NULL;

  AP_INFO("GLFW initialized");

  return true;
}

void AP_WindowingClose(void) {
  if (!g_windowing_initialized) {
    return;
  }

  /*
   * AP_Quit() should normally be called after all AP_Window
   * objects have been destroyed.
   */
  g_active_window = NULL;

  glfwTerminate();

  g_windowing_initialized = false;

  AP_INFO("GLFW terminated");
}

/* ---------------------------------------------------------
 * Subsystem metadata
 * --------------------------------------------------------- */

const AP_SubsystemMetadata AP_WindowingSubsystem = {.init = AP_WindowingInit,
                                                    .close = AP_WindowingClose};

/* ---------------------------------------------------------
 * Window creation
 * --------------------------------------------------------- */

AP_Window *AP_CreateWindow(const char *title, uint32_t width, uint32_t height) {
  if (!g_windowing_initialized) {
    AP_SET_ERROR(AP_ERROR_NOT_INITIALIZED,
                 "Windowing subsystem has not been initialized");

    return NULL;
  }

  if (!title) {
    AP_SET_ERROR(AP_ERROR_INVALID_ARGUMENT, "Window title cannot be NULL");

    return NULL;
  }

  if (width == 0 || height == 0) {
    AP_SET_ERROR(AP_ERROR_INVALID_ARGUMENT, "Window dimensions cannot be zero");

    return NULL;
  }

  if (width > INT_MAX || height > INT_MAX) {
    AP_SET_ERROR(AP_ERROR_INVALID_ARGUMENT,
                 "Window dimensions exceed GLFW limits");

    return NULL;
  }

  AP_Window *window = malloc(sizeof(AP_Window));

  if (!window) {
    AP_SET_ERROR(AP_ERROR_OUT_OF_MEMORY, "Failed to allocate AP_Window");

    return NULL;
  }

  window->handle = NULL;
  window->width = width;
  window->height = height;
  window->should_close = false;

  /*
   * Default GLFW window.
   *
   * Renderer-specific hints should eventually be configured
   * by the AP2 renderer before creating the window.
   */
  window->handle = glfwCreateWindow((int)width, (int)height, title, NULL, NULL);

  if (!window->handle) {
    free(window);

    AP_SET_ERROR(AP_ERROR_OPERATION_FAILED, "GLFW failed to create window");

    return NULL;
  }

  /*
   * Allow callbacks to retrieve the AP_Window.
   */
  glfwSetWindowUserPointer(window->handle, window);

  /*
   * Register callbacks.
   */
  glfwSetWindowCloseCallback(window->handle, AP_WindowCloseCallback);

  glfwSetFramebufferSizeCallback(window->handle,
                                 AP_WindowFramebufferSizeCallback);

  AP_INFO("Created window \"%s\" (%ux%u)", title, width, height);

  return window;
}

/* ---------------------------------------------------------
 * Window destruction
 * --------------------------------------------------------- */

void AP_DestroyWindow(AP_Window *window) {
  if (!window) {
    return;
  }

  if (g_active_window == window) {
    g_active_window = NULL;
  }

  if (window->handle) {
    glfwDestroyWindow(window->handle);
    window->handle = NULL;
  }

  AP_DEBUG("Destroyed window");

  free(window);
}

/* ---------------------------------------------------------
 * Active window
 * --------------------------------------------------------- */

bool AP_SetActiveWindow(AP_Window *window) {
  if (!window || !window->handle) {
    AP_SET_ERROR(AP_ERROR_INVALID_ARGUMENT,
                 "Cannot activate an invalid window");

    return false;
  }

  if (!g_windowing_initialized) {
    AP_SET_ERROR(AP_ERROR_NOT_INITIALIZED,
                 "Windowing subsystem has not been initialized");

    return false;
  }

  g_active_window = window;

  /*
   * Currently AP2 uses GLFW/OpenGL-style context handling.
   *
   * If AP2 gets Vulkan/D3D/SDL_GPU backends, this should
   * move into the renderer layer.
   */
  glfwMakeContextCurrent(window->handle);

  return true;
}

AP_Window *AP_GetActiveWindow(void) { return g_active_window; }

/* ---------------------------------------------------------
 * Window state
 * --------------------------------------------------------- */

bool AP_WindowShouldClose(const AP_Window *window) {
  if (!window || !window->handle) {
    return true;
  }

  return window->should_close || glfwWindowShouldClose(window->handle);
}

void AP_WindowGetSize(const AP_Window *window, uint32_t *out_width,
                      uint32_t *out_height) {
  if (!window) {
    return;
  }

  if (out_width) {
    *out_width = window->width;
  }

  if (out_height) {
    *out_height = window->height;
  }
}

/* ---------------------------------------------------------
 * Event processing
 * --------------------------------------------------------- */

void AP_WindowPollEvents(void) {
  if (!g_windowing_initialized) {
    return;
  }

  glfwPollEvents();
}

/* ---------------------------------------------------------
 * Buffer presentation
 * --------------------------------------------------------- */

void AP_WindowSwapBuffers(AP_Window *window) {
  if (!window || !window->handle) {
    return;
  }

  glfwSwapBuffers(window->handle);
}
