#include "AP2/AP2_Device.h"

#include "AP2/AP2_Error.h"
#include "AP2/AP2_Logger.h"

#define GLFW_INCLUDE_NONE
#include <glad/gl.h>
#include <GLFW/glfw3.h>

#include <stdlib.h>

/* ---------------------------------------------------------
 * Internal device structure
 * --------------------------------------------------------- */

struct AP_Device {
  AP_DeviceInfo info;
  bool initialized;
};

/* ---------------------------------------------------------
 * Global device
 * --------------------------------------------------------- */

static AP_Device *g_device = NULL;

/* ---------------------------------------------------------
 * Backend names
 * --------------------------------------------------------- */

const char *AP_GraphicsBackendName(AP_GraphicsBackend backend) {
  switch (backend) {
  case AP_GRAPHICS_BACKEND_NONE:
    return "None";

  case AP_GRAPHICS_BACKEND_OPENGL:
    return "OpenGL";

  case AP_GRAPHICS_BACKEND_VULKAN:
    return "Vulkan";

  case AP_GRAPHICS_BACKEND_D3D11:
    return "Direct3D 11";

  case AP_GRAPHICS_BACKEND_D3D12:
    return "Direct3D 12";

  default:
    return "Unknown";
  }
}

/* ---------------------------------------------------------
 * Device initialization
 * --------------------------------------------------------- */

bool AP_DeviceInit(const AP_DeviceConfig *config) {

  if (!config) {
    AP_SET_ERROR(AP_ERROR_INVALID_ARGUMENT,
                 "Device configuration cannot be NULL");

    return false;
  }

  if (config->backend <= AP_GRAPHICS_BACKEND_NONE ||
      config->backend >= AP_GRAPHICS_BACKEND_COUNT) {

    AP_SET_ERROR(AP_ERROR_INVALID_ARGUMENT, "Invalid graphics backend");

    return false;
  }

  if (g_device) {
    AP_SET_ERROR(AP_ERROR_ALREADY_INITIALIZED,
                 "Graphics device is already initialized");

    return false;
  }

  /* -------------------------------------------------------
   * OpenGL
   * ------------------------------------------------------- */

  if (config->backend == AP_GRAPHICS_BACKEND_OPENGL) {

    /*
     * GLFW must already have an active OpenGL context.
     *
     * The context belongs to AP_Window.
     */

    GLFWwindow *context = glfwGetCurrentContext();

    if (!context) {
      AP_SET_ERROR(AP_ERROR_OPERATION_FAILED, "No active OpenGL context");

      return false;
    }

    /* -----------------------------------------------------
     * Query OpenGL information
     * ----------------------------------------------------- */

    const char *vendor = (const char *)glGetString(GL_VENDOR);

    const char *renderer = (const char *)glGetString(GL_RENDERER);

    const char *version = (const char *)glGetString(GL_VERSION);

    if (!vendor || !renderer || !version) {
      AP_SET_ERROR(AP_ERROR_OPERATION_FAILED,
                   "Failed to query OpenGL device information");

      return false;
    }

    /* -----------------------------------------------------
     * Allocate device
     * ----------------------------------------------------- */

    AP_Device *device = calloc(1, sizeof(AP_Device));

    if (!device) {
      AP_SET_ERROR(AP_ERROR_OUT_OF_MEMORY,
                   "Failed to allocate graphics device");

      return false;
    }

    /* -----------------------------------------------------
     * Device information
     * ----------------------------------------------------- */

    device->info.backend = AP_GRAPHICS_BACKEND_OPENGL;

    /*
     * These strings are owned by OpenGL.
     * AP_Device does not free them.
     */

    device->info.name = renderer;
    device->info.vendor = vendor;
    device->info.driver = version;

    /*
     * OpenGL version parsing can be implemented later.
     */

    device->info.api_major = 0;
    device->info.api_minor = 0;

    device->initialized = true;

    g_device = device;

    /* -----------------------------------------------------
     * Logging
     * ----------------------------------------------------- */

    AP_INFO("Graphics device initialized: %s", renderer);

    AP_INFO("Graphics vendor: %s", vendor);

    AP_INFO("Graphics API: %s", version);

    return true;
  }

  /* -------------------------------------------------------
   * Unsupported backends
   * ------------------------------------------------------- */

  switch (config->backend) {

  case AP_GRAPHICS_BACKEND_VULKAN:

    AP_SET_ERROR(AP_ERROR_OPERATION_FAILED,
                 "Vulkan backend is not implemented");

    return false;

  case AP_GRAPHICS_BACKEND_D3D11:

    AP_SET_ERROR(AP_ERROR_OPERATION_FAILED,
                 "Direct3D 11 backend is not implemented");

    return false;

  case AP_GRAPHICS_BACKEND_D3D12:

    AP_SET_ERROR(AP_ERROR_OPERATION_FAILED,
                 "Direct3D 12 backend is not implemented");

    return false;

  default:

    AP_SET_ERROR(AP_ERROR_INVALID_ARGUMENT, "Unknown graphics backend");

    return false;
  }
}

/* ---------------------------------------------------------
 * Device shutdown
 * --------------------------------------------------------- */

void AP_DeviceClose(void) {

  if (!g_device) {
    return;
  }

  AP_INFO("Closing graphics device: %s",
          AP_GraphicsBackendName(g_device->info.backend));

  /*
   * AP_Device does not destroy the GLFW window or
   * OpenGL context.
   *
   * AP_Window owns those resources.
   */

  g_device->initialized = false;

  free(g_device);

  g_device = NULL;
}

/* ---------------------------------------------------------
 * Device access
 * --------------------------------------------------------- */

AP_Device *AP_GetDevice(void) { return g_device; }

const AP_DeviceInfo *AP_DeviceGetInfo(const AP_Device *device) {

  if (!device || !device->initialized) {
    return NULL;
  }

  return &device->info;
}

/* ---------------------------------------------------------
 * Device state
 * --------------------------------------------------------- */

bool AP_DeviceIsInitialized(void) {
  return g_device != NULL && g_device->initialized;
}

/* ---------------------------------------------------------
 * Backend
 * --------------------------------------------------------- */

AP_GraphicsBackend AP_DeviceGetBackend(const AP_Device *device) {

  if (!device || !device->initialized) {
    return AP_GRAPHICS_BACKEND_NONE;
  }

  return device->info.backend;
}

/* ---------------------------------------------------------
 * Subsystem metadata
 * --------------------------------------------------------- */

static bool AP_DeviceSubsystemInit(void) {

  AP_DeviceConfig config = {
      .backend = AP_GRAPHICS_BACKEND_OPENGL,
      .validation = false,
      .vsync = true,
  };

  return AP_DeviceInit(&config);
}

const AP_SubsystemMetadata AP_DeviceSubsystem = {
    .init = AP_DeviceSubsystemInit,
    .close = AP_DeviceClose,
};
