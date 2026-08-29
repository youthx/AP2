/*
 * AP2 — Application Primitives
 * Copyright (c) 2024-2026 Jack Waechter
 *
 * Licensed under the MIT License.
 * See LICENSE in the project root for full terms.
 */

#include "AP2/AP2_Device.h"

#include "AP2/AP2_Error.h"
#include "AP2/AP2_Logger.h"
#include "AP2/AP2_Opengl.h"
#include "AP2/AP2_Platform.h"

#include <stdlib.h>
#include <string.h>

#define AP_DEVICE_STRING_MAX 256

/* ---------------------------------------------------------
 * Internal device structure
 * --------------------------------------------------------- */

struct AP_Device {
  AP_DeviceInfo info;
  char name[AP_DEVICE_STRING_MAX];
  char vendor[AP_DEVICE_STRING_MAX];
  char driver[AP_DEVICE_STRING_MAX];
  bool vsync;
  bool validation;
  bool initialized;
};

/* ---------------------------------------------------------
 * Global device
 * --------------------------------------------------------- */

static AP_Device *g_device = NULL;

static void AP_DeviceCopyString(char *destination, size_t capacity,
                                const char *source) {
  memset(destination, 0, capacity);

  if (source == NULL || capacity == 0) {
    return;
  }

  strncpy(destination, source, capacity - 1);
}

static bool AP_DeviceBackendAvailable(AP_GraphicsBackend backend) {
  switch (backend) {
  case AP_GRAPHICS_BACKEND_OPENGL:
    return AP_PlatformHasOpenGL();
  case AP_GRAPHICS_BACKEND_VULKAN:
    return AP_PlatformHasVulkan();
  case AP_GRAPHICS_BACKEND_D3D11:
    return AP_PlatformHasD3D11();
  case AP_GRAPHICS_BACKEND_D3D12:
    return AP_PlatformHasD3D12();
  default:
    return false;
  }
}

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

bool AP_GraphicsBackendAvailable(AP_GraphicsBackend backend) {
  return AP_DeviceBackendAvailable(backend);
}

AP_DeviceConfig AP_DeviceDefaultConfig(void) {
  AP_DeviceConfig config;

  memset(&config, 0, sizeof(config));
  config.backend = AP_GRAPHICS_BACKEND_OPENGL;
  config.validation = false;
  config.vsync = true;
  return config;
}

/* ---------------------------------------------------------
 * Device initialization
 * --------------------------------------------------------- */

bool AP_DeviceInit(const AP_DeviceConfig *config) {
  AP_Device *device;
  const AP_OpenGLInfo *gl_info;
  AP_OpenGLVersion gl_version;

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

  if (!AP_DeviceBackendAvailable(config->backend)) {
    AP_SET_ERROR(AP_ERROR_UNSUPPORTED,
                 "Requested graphics backend is not available on this "
                 "platform");
    return false;
  }

  if (g_device) {
    AP_SET_ERROR(AP_ERROR_ALREADY_INITIALIZED,
                 "Graphics device is already initialized");
    return false;
  }

  if (config->backend != AP_GRAPHICS_BACKEND_OPENGL) {
    AP_SET_ERROR(AP_ERROR_UNSUPPORTED,
                 "Requested graphics backend is not implemented");
    return false;
  }

  if (!AP_OpenGLIsInitialized() || !AP_OpenGLHasContext()) {
    AP_SET_ERROR(AP_ERROR_NOT_INITIALIZED,
                 "OpenGL backend must be initialized before the device");
    return false;
  }

  gl_info = AP_OpenGLGetInfo();
  if (gl_info == NULL || gl_info->renderer == NULL || gl_info->vendor == NULL ||
      gl_info->version == NULL) {
    AP_SET_ERROR(AP_ERROR_OPERATION_FAILED,
                 "Failed to query OpenGL device information");
    return false;
  }

  device = calloc(1, sizeof(AP_Device));
  if (!device) {
    AP_SET_ERROR(AP_ERROR_OUT_OF_MEMORY, "Failed to allocate graphics device");
    return false;
  }

  gl_version = AP_OpenGLGetVersion();

  AP_DeviceCopyString(device->name, sizeof(device->name), gl_info->renderer);
  AP_DeviceCopyString(device->vendor, sizeof(device->vendor), gl_info->vendor);
  AP_DeviceCopyString(device->driver, sizeof(device->driver), gl_info->version);

  device->info.backend = AP_GRAPHICS_BACKEND_OPENGL;
  device->info.name = device->name;
  device->info.vendor = device->vendor;
  device->info.driver = device->driver;
  device->info.api_major = gl_version.major;
  device->info.api_minor = gl_version.minor;
  device->vsync = config->vsync;
  device->validation = config->validation;
  device->initialized = true;

  g_device = device;

  AP_INFO("Graphics device initialized: %s", device->name);
  AP_INFO("Graphics vendor: %s", device->vendor);
  AP_INFO("Graphics API: %s (%u.%u)", device->driver, device->info.api_major,
          device->info.api_minor);

  return true;
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

bool AP_DeviceIsInitialized(void) {
  return g_device != NULL && g_device->initialized;
}

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
  /*
   * The device is created when a window provides a graphics
   * context. Registering this subsystem must not try to
   * create a context-less device.
   */
  return true;
}

const AP_SubsystemMetadata AP_DeviceSubsystem = {
    .init = AP_DeviceSubsystemInit,
    .close = AP_DeviceClose,
};
