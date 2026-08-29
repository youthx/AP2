/*
 * AP2 — Application Primitives
 * Copyright (c) 2024-2026 Jack Waechter
 *
 * Licensed under the MIT License.
 * See LICENSE in the project root for full terms.
 */

#include "AP2/AP2_Video.h"

#include "AP2/AP2_Device.h"
#include "AP2/AP2_Error.h"
#include "AP2/AP2_Logger.h"
#include "AP2/AP2_Platform.h"

#include <stddef.h>
#include <string.h>

#define AP_VIDEO_STRING_MAX 256

/* ---------------------------------------------------------
 * Internal state
 * --------------------------------------------------------- */

static bool g_video_initialized = false;

static AP_VideoConfig g_video_config;

static char g_video_name[AP_VIDEO_STRING_MAX];
static char g_video_vendor[AP_VIDEO_STRING_MAX];
static char g_video_version[AP_VIDEO_STRING_MAX];

static AP_VideoInfo g_video_info = {
    .api = AP_VIDEO_API_NONE,
    .name = NULL,
    .vendor = NULL,
    .version = NULL,
    .major_version = 0,
    .minor_version = 0,
};

/* ---------------------------------------------------------
 * Helpers
 * --------------------------------------------------------- */

static void AP_VideoCopyString(char *destination, size_t capacity,
                               const char *source) {
  memset(destination, 0, capacity);

  if (source == NULL || capacity == 0) {
    return;
  }

  strncpy(destination, source, capacity - 1);
}

static void AP_VideoClearInfo(void) {
  memset(g_video_name, 0, sizeof(g_video_name));
  memset(g_video_vendor, 0, sizeof(g_video_vendor));
  memset(g_video_version, 0, sizeof(g_video_version));

  g_video_info.api = AP_VIDEO_API_NONE;
  g_video_info.name = NULL;
  g_video_info.vendor = NULL;
  g_video_info.version = NULL;
  g_video_info.major_version = 0;
  g_video_info.minor_version = 0;
}

static void AP_VideoBindInfoPointers(void) {
  g_video_info.name = g_video_name[0] != '\0' ? g_video_name : NULL;
  g_video_info.vendor = g_video_vendor[0] != '\0' ? g_video_vendor : NULL;
  g_video_info.version = g_video_version[0] != '\0' ? g_video_version : NULL;
}

/* ---------------------------------------------------------
 * API name / availability
 * --------------------------------------------------------- */

const char *AP_VideoAPIName(AP_VideoAPI api) {
  switch (api) {
  case AP_VIDEO_API_NONE:
    return "None";

  case AP_VIDEO_API_OPENGL:
    return "OpenGL";

  case AP_VIDEO_API_VULKAN:
    return "Vulkan";

  case AP_VIDEO_API_D3D11:
    return "Direct3D 11";

  case AP_VIDEO_API_D3D12:
    return "Direct3D 12";

  default:
    return "Unknown";
  }
}

bool AP_VideoAPIAvailable(AP_VideoAPI api) {
  switch (api) {
  case AP_VIDEO_API_OPENGL:
    return AP_PlatformHasOpenGL();

  case AP_VIDEO_API_VULKAN:
    return AP_PlatformHasVulkan();

  case AP_VIDEO_API_D3D11:
    return AP_PlatformHasD3D11();

  case AP_VIDEO_API_D3D12:
    return AP_PlatformHasD3D12();

  default:
    return false;
  }
}

bool AP_VideoAPIImplemented(AP_VideoAPI api) {
  return api == AP_VIDEO_API_OPENGL;
}

/* ---------------------------------------------------------
 * Default configuration
 * --------------------------------------------------------- */

AP_VideoConfig AP_VideoDefaultConfig(void) {
  AP_VideoConfig config;

  memset(&config, 0, sizeof(config));
  config.api = AP_VIDEO_API_OPENGL;
  AP_PlatformRecommendedOpenGLVersion(&config.major_version,
                                      &config.minor_version);
  config.debug = false;
  config.validation = false;
  config.vsync = true;

  return config;
}

/* ---------------------------------------------------------
 * Configuration validation
 * --------------------------------------------------------- */

static bool AP_VideoValidateConfig(const AP_VideoConfig *config,
                                   AP_VideoConfig *normalized) {
  AP_VideoConfig actual;

  if (!config) {
    AP_SET_ERROR(AP_ERROR_INVALID_ARGUMENT,
                 "Video configuration cannot be NULL");
    return false;
  }

  actual = *config;

  if (actual.api <= AP_VIDEO_API_NONE || actual.api >= AP_VIDEO_API_COUNT) {
    AP_SET_ERROR(AP_ERROR_INVALID_ARGUMENT, "Invalid video API");
    return false;
  }

  if (!AP_VideoAPIAvailable(actual.api)) {
    AP_SET_ERROR(AP_ERROR_UNSUPPORTED,
                 "Requested video API is not available on this platform");
    return false;
  }

  if (!AP_VideoAPIImplemented(actual.api)) {
    AP_SET_ERROR(AP_ERROR_UNSUPPORTED,
                 "Requested video API is not implemented");
    return false;
  }

  if (actual.api == AP_VIDEO_API_OPENGL) {
    uint32_t max_major = 0;
    uint32_t max_minor = 0;

    AP_PlatformMaxOpenGLVersion(&max_major, &max_minor);

    if (actual.major_version == 0 && actual.minor_version == 0) {
      AP_PlatformRecommendedOpenGLVersion(&actual.major_version,
                                          &actual.minor_version);
    }

    if (actual.major_version < AP_PLATFORM_OPENGL_MIN_MAJOR ||
        (actual.major_version == AP_PLATFORM_OPENGL_MIN_MAJOR &&
         actual.minor_version < AP_PLATFORM_OPENGL_MIN_MINOR)) {
      AP_SET_ERROR(AP_ERROR_UNSUPPORTED, "OpenGL 3.3 or newer is required");
      return false;
    }

    if (actual.major_version > max_major ||
        (actual.major_version == max_major &&
         actual.minor_version > max_minor)) {
      AP_WARN("Requested OpenGL %u.%u exceeds %s maximum %u.%u; clamping",
              actual.major_version, actual.minor_version,
              AP_PlatformOSName(AP_PlatformGetOS()), max_major, max_minor);
      actual.major_version = max_major;
      actual.minor_version = max_minor;
    }
  }

  if (normalized != NULL) {
    *normalized = actual;
  }

  return true;
}

/* ---------------------------------------------------------
 * Video initialization
 * --------------------------------------------------------- */

bool AP_VideoInit(const AP_VideoConfig *config) {
  AP_VideoConfig actual;

  if (g_video_initialized) {
    return true;
  }

  if (!config) {
    actual = AP_VideoDefaultConfig();
  } else if (!AP_VideoValidateConfig(config, &actual)) {
    return false;
  }

  g_video_config = actual;
  AP_VideoClearInfo();
  g_video_info.api = g_video_config.api;
  g_video_initialized = true;

  AP_INFO("Video subsystem initialized: %s %u.%u (%s/%s)",
          AP_VideoAPIName(g_video_config.api), g_video_config.major_version,
          g_video_config.minor_version,
          AP_PlatformOSName(AP_PlatformGetOS()),
          AP_PlatformWindowSystemName(AP_PlatformGetWindowSystem()));

  return true;
}

bool AP_VideoSetConfig(const AP_VideoConfig *config) {
  AP_VideoConfig actual;

  if (AP_DeviceIsInitialized()) {
    AP_SET_ERROR(AP_ERROR_INVALID_STATE,
                 "Cannot change video configuration after the graphics device "
                 "exists");
    return false;
  }

  if (!AP_VideoValidateConfig(config, &actual)) {
    return false;
  }

  g_video_config = actual;
  g_video_info.api = actual.api;
  g_video_initialized = true;

  AP_INFO("Video configuration set: %s %u.%u", AP_VideoAPIName(actual.api),
          actual.major_version, actual.minor_version);

  return true;
}

/* ---------------------------------------------------------
 * Video shutdown
 * --------------------------------------------------------- */

void AP_VideoClose(void) {
  if (!g_video_initialized) {
    return;
  }

  AP_INFO("Closing video subsystem");

  if (AP_DeviceIsInitialized()) {
    AP_DeviceClose();
  }

  g_video_initialized = false;
  memset(&g_video_config, 0, sizeof(g_video_config));
  AP_VideoClearInfo();
}

/* ---------------------------------------------------------
 * Video state
 * --------------------------------------------------------- */

bool AP_VideoIsInitialized(void) { return g_video_initialized; }

AP_VideoAPI AP_VideoGetAPI(void) {
  if (!g_video_initialized) {
    return AP_VIDEO_API_NONE;
  }

  return g_video_info.api;
}

const AP_VideoInfo *AP_VideoGetInfo(void) {
  if (!g_video_initialized) {
    return NULL;
  }

  return &g_video_info;
}

const AP_VideoConfig *AP_VideoGetConfig(void) {
  if (!g_video_initialized) {
    return NULL;
  }

  return &g_video_config;
}

bool AP_VideoUpdateDeviceInfo(void) {
  const AP_DeviceInfo *device_info;
  AP_Device *device;

  if (!g_video_initialized) {
    AP_SET_ERROR(AP_ERROR_NOT_INITIALIZED,
                 "Video subsystem is not initialized");
    return false;
  }

  device = AP_GetDevice();

  if (!device) {
    AP_SET_ERROR(AP_ERROR_NOT_INITIALIZED,
                 "Graphics device has not been initialized");
    return false;
  }

  device_info = AP_DeviceGetInfo(device);

  if (!device_info) {
    AP_SET_ERROR(AP_ERROR_OPERATION_FAILED,
                 "Failed to retrieve graphics device information");
    return false;
  }

  AP_VideoCopyString(g_video_name, sizeof(g_video_name), device_info->name);
  AP_VideoCopyString(g_video_vendor, sizeof(g_video_vendor),
                     device_info->vendor);
  AP_VideoCopyString(g_video_version, sizeof(g_video_version),
                     device_info->driver);
  AP_VideoBindInfoPointers();

  g_video_info.api = g_video_config.api;
  g_video_info.major_version = device_info->api_major;
  g_video_info.minor_version = device_info->api_minor;

  return true;
}

/* ---------------------------------------------------------
 * AP2 subsystem entry point
 * --------------------------------------------------------- */

static bool AP_VideoSubsystemInit(void) { return AP_VideoInit(NULL); }

const AP_SubsystemMetadata AP_VideoSubsystem = {
    .init = AP_VideoSubsystemInit,
    .close = AP_VideoClose,
};
