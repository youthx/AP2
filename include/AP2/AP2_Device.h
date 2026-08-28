#ifndef AP2_DEVICE_H
#define AP2_DEVICE_H

#include "AP2/AP2_Init.h"

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ---------------------------------------------------------
 * Graphics backend
 * --------------------------------------------------------- */

typedef enum AP_GraphicsBackend {
  AP_GRAPHICS_BACKEND_NONE = 0,

  AP_GRAPHICS_BACKEND_OPENGL,
  AP_GRAPHICS_BACKEND_VULKAN,
  AP_GRAPHICS_BACKEND_D3D11,
  AP_GRAPHICS_BACKEND_D3D12,

  AP_GRAPHICS_BACKEND_COUNT
} AP_GraphicsBackend;

/* ---------------------------------------------------------
 * Device
 * --------------------------------------------------------- */

/*
 * The actual device implementation is private to AP2.
 *
 * This prevents OpenGL/Vulkan/D3D-specific types from
 * leaking into the public AP2 API.
 */
typedef struct AP_Device AP_Device;

/* ---------------------------------------------------------
 * Device information
 * --------------------------------------------------------- */

typedef struct AP_DeviceInfo {
  AP_GraphicsBackend backend;

  const char *name;
  const char *vendor;
  const char *driver;

  uint32_t api_major;
  uint32_t api_minor;
} AP_DeviceInfo;

/* ---------------------------------------------------------
 * Device configuration
 * --------------------------------------------------------- */

typedef struct AP_DeviceConfig {
  /*
   * Graphics backend to create.
   */
  AP_GraphicsBackend backend;

  /*
   * Enable graphics API debugging/validation where
   * supported by the backend.
   */
  bool validation;

  /*
   * Enable vertical synchronization.
   */
  bool vsync;
} AP_DeviceConfig;

/* ---------------------------------------------------------
 * Device lifecycle
 * --------------------------------------------------------- */

/*
 * Creates and initializes the graphics device.
 *
 * Returns false if the requested backend cannot be
 * initialized.
 */
bool AP_DeviceInit(const AP_DeviceConfig *config);

/*
 * Destroys the active graphics device.
 */
void AP_DeviceClose(void);

/* ---------------------------------------------------------
 * Device access
 * --------------------------------------------------------- */

/*
 * Returns the currently active device.
 *
 * Returns NULL if no device exists.
 */
AP_Device *AP_GetDevice(void);

/*
 * Returns information about a device.
 *
 * Returns NULL if `device` is NULL.
 */
const AP_DeviceInfo *AP_DeviceGetInfo(const AP_Device *device);

/*
 * Returns whether a graphics device currently exists.
 */
bool AP_DeviceIsInitialized(void);

/* ---------------------------------------------------------
 * Backend
 * --------------------------------------------------------- */

/*
 * Returns the backend used by the device.
 */
AP_GraphicsBackend AP_DeviceGetBackend(const AP_Device *device);

/*
 * Returns a human-readable backend name.
 */
const char *AP_GraphicsBackendName(AP_GraphicsBackend backend);

/* ---------------------------------------------------------
 * Subsystem metadata
 * --------------------------------------------------------- */

extern const AP_SubsystemMetadata AP_DeviceSubsystem;

#ifdef __cplusplus
}
#endif

#endif /* AP2_DEVICE_H */
