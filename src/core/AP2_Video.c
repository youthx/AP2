#include "AP2/AP2_Video.h"

#include "AP2/AP2_Device.h"
#include "AP2/AP2_Error.h"
#include "AP2/AP2_Logger.h"

#include <stddef.h>

/* ---------------------------------------------------------
 * Internal state
 * --------------------------------------------------------- */

static bool g_video_initialized = false;

static AP_VideoConfig g_video_config = {
    .api = AP_VIDEO_API_OPENGL,

    .major_version = 4,
    .minor_version = 6,

    .debug = false,
    .validation = false,
    .vsync = true,
};

static AP_VideoInfo g_video_info = {
    .api = AP_VIDEO_API_NONE,
    .name = NULL,
    .vendor = NULL,
    .version = NULL,
    .major_version = 0,
    .minor_version = 0,
};

/* ---------------------------------------------------------
 * API name
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

/* ---------------------------------------------------------
 * Default configuration
 * --------------------------------------------------------- */

static AP_VideoConfig AP_VideoDefaultConfig(void) {
  AP_VideoConfig config = {
      .api = AP_VIDEO_API_OPENGL,

      .major_version = 4,
      .minor_version = 6,

      .debug = false,
      .validation = false,
      .vsync = true,
  };

  return config;
}

/* ---------------------------------------------------------
 * Configuration validation
 * --------------------------------------------------------- */

static bool AP_VideoValidateConfig(const AP_VideoConfig *config) {
  if (!config) {
    AP_SET_ERROR(AP_ERROR_INVALID_ARGUMENT,
                 "Video configuration cannot be NULL");

    return false;
  }

  if (config->api <= AP_VIDEO_API_NONE || config->api >= AP_VIDEO_API_COUNT) {

    AP_SET_ERROR(AP_ERROR_INVALID_ARGUMENT, "Invalid video API");

    return false;
  }

  /*
   * OpenGL version validation.
   */

  if (config->api == AP_VIDEO_API_OPENGL) {
    if (config->major_version < 1) {
      AP_SET_ERROR(AP_ERROR_INVALID_ARGUMENT, "Invalid OpenGL major version");

      return false;
    }
  }

  return true;
}

/* ---------------------------------------------------------
 * Video initialization
 * --------------------------------------------------------- */

bool AP_VideoInit(const AP_VideoConfig *config) {

  if (g_video_initialized) {
    return true;
  }

  /* -------------------------------------------------------
   * Configuration
   * ------------------------------------------------------- */

  if (!config) {
    g_video_config = AP_VideoDefaultConfig();
  } else {
    if (!AP_VideoValidateConfig(config)) {
      return false;
    }

    g_video_config = *config;
  }

  AP_INFO("Initializing video: %s", AP_VideoAPIName(g_video_config.api));

  /*
   * IMPORTANT:
   *
   * Video does NOT initialize AP_Device here.
   *
   * AP_Device requires a graphics context, and the context
   * belongs to a window.
   *
   * AP_CreateWindow() will create the context and initialize
   * the graphics device afterward.
   */

  g_video_info.api = g_video_config.api;

  /*
   * Device information is not available yet because no
   * graphics context exists.
   */

  g_video_info.name = NULL;
  g_video_info.vendor = NULL;
  g_video_info.version = NULL;

  g_video_info.major_version = 0;
  g_video_info.minor_version = 0;

  g_video_initialized = true;

  AP_INFO("Video subsystem initialized: %s",
          AP_VideoAPIName(g_video_config.api));

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

  /*
   * The graphics device is normally closed by the windowing
   * layer after its context is no longer needed.
   *
   * However, if a device exists here, clean it up.
   */

  if (AP_DeviceIsInitialized()) {
    AP_DeviceClose();
  }

  g_video_initialized = false;

  g_video_info.api = AP_VIDEO_API_NONE;

  g_video_info.name = NULL;
  g_video_info.vendor = NULL;
  g_video_info.version = NULL;

  g_video_info.major_version = 0;
  g_video_info.minor_version = 0;
}

/* ---------------------------------------------------------
 * Video state
 * --------------------------------------------------------- */

bool AP_VideoIsInitialized(void) { return g_video_initialized; }

/* ---------------------------------------------------------
 * Current API
 * --------------------------------------------------------- */

AP_VideoAPI AP_VideoGetAPI(void) {

  if (!g_video_initialized) {
    return AP_VIDEO_API_NONE;
  }

  return g_video_info.api;
}

/* ---------------------------------------------------------
 * Video information
 * --------------------------------------------------------- */

const AP_VideoInfo *AP_VideoGetInfo(void) {

  if (!g_video_initialized) {
    return NULL;
  }

  return &g_video_info;
}

/* ---------------------------------------------------------
 * Internal configuration
 * --------------------------------------------------------- */

/*
 * Used internally by the Window subsystem when creating
 * the graphics context.
 *
 * This keeps the OpenGL configuration hidden from the user.
 */

const AP_VideoConfig *AP_VideoGetConfig(void) {

  if (!g_video_initialized) {
    return NULL;
  }

  return &g_video_config;
}

/* ---------------------------------------------------------
 * Update device information
 * --------------------------------------------------------- */

/*
 * Called after a graphics context and AP_Device have been
 * successfully created.
 */

bool AP_VideoUpdateDeviceInfo(void) {

  if (!g_video_initialized) {
    AP_SET_ERROR(AP_ERROR_NOT_INITIALIZED,
                 "Video subsystem is not initialized");

    return false;
  }

  AP_Device *device = AP_GetDevice();

  if (!device) {
    AP_SET_ERROR(AP_ERROR_NOT_INITIALIZED,
                 "Graphics device has not been initialized");

    return false;
  }

  const AP_DeviceInfo *device_info = AP_DeviceGetInfo(device);

  if (!device_info) {
    AP_SET_ERROR(AP_ERROR_OPERATION_FAILED,
                 "Failed to retrieve graphics device information");

    return false;
  }

  g_video_info.name = device_info->name;
  g_video_info.vendor = device_info->vendor;
  g_video_info.version = device_info->driver;

  g_video_info.major_version = device_info->api_major;

  g_video_info.minor_version = device_info->api_minor;

  return true;
}

/* ---------------------------------------------------------
 * AP2 subsystem entry point
 * --------------------------------------------------------- */

static bool AP_VideoSubsystemInit(void) { return AP_VideoInit(NULL); }

/* ---------------------------------------------------------
 * Subsystem metadata
 * --------------------------------------------------------- */

const AP_SubsystemMetadata AP_VideoSubsystem = {
    .init = AP_VideoSubsystemInit,
    .close = AP_VideoClose,
};
