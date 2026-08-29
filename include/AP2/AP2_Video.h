/*
 * AP2 — Application Primitives
 * Copyright (c) 2026 Jack Waechter
 *
 * Licensed under the MIT License.
 * See LICENSE in the project root for full terms.
 */

#ifndef AP2_VIDEO_H
#define AP2_VIDEO_H

#include "AP2/AP2_Init.h"

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ---------------------------------------------------------
 * Video API
 * --------------------------------------------------------- */

typedef enum AP_VideoAPI {
  AP_VIDEO_API_NONE = 0,

  AP_VIDEO_API_OPENGL,
  AP_VIDEO_API_VULKAN,
  AP_VIDEO_API_D3D11,
  AP_VIDEO_API_D3D12,

  AP_VIDEO_API_COUNT
} AP_VideoAPI;

/* ---------------------------------------------------------
 * Video configuration
 * --------------------------------------------------------- */

typedef struct AP_VideoConfig {
  AP_VideoAPI api;

  uint32_t major_version;
  uint32_t minor_version;

  bool debug;
  bool validation;
  bool vsync;
} AP_VideoConfig;

/* ---------------------------------------------------------
 * Video information
 * --------------------------------------------------------- */

typedef struct AP_VideoInfo {
  AP_VideoAPI api;

  const char *name;
  const char *vendor;
  const char *version;

  uint32_t major_version;
  uint32_t minor_version;
} AP_VideoInfo;

/* ---------------------------------------------------------
 * Video subsystem
 * --------------------------------------------------------- */

/*
 * Initializes the video subsystem using the supplied
 * configuration.
 *
 * Passing NULL uses the platform default (OpenGL, vsync on,
 * highest core version this OS can request).
 *
 * D3D11/D3D12 are rejected on non-Windows. Vulkan and D3D
 * are recognized but not implemented yet.
 */
bool AP_VideoInit(const AP_VideoConfig *config);

/*
 * Updates the pending video configuration. Fails if a
 * graphics device already exists.
 */
bool AP_VideoSetConfig(const AP_VideoConfig *config);

/*
 * Shuts down the video subsystem.
 */
void AP_VideoClose(void);

/* ---------------------------------------------------------
 * Video state
 * --------------------------------------------------------- */

/*
 * Returns whether the video subsystem is initialized.
 */
bool AP_VideoIsInitialized(void);

/*
 * Returns the active video API.
 */
AP_VideoAPI AP_VideoGetAPI(void);

/*
 * Returns information about the active graphics device/API.
 *
 * Returns NULL if the video subsystem isn't initialized.
 */
const AP_VideoInfo *AP_VideoGetInfo(void);

const AP_VideoConfig *AP_VideoGetConfig(void);
bool AP_VideoUpdateDeviceInfo(void);

/* ---------------------------------------------------------
 * Video API utilities
 * --------------------------------------------------------- */

/*
 * Returns a human-readable name for a video API.
 */
const char *AP_VideoAPIName(AP_VideoAPI api);

bool AP_VideoAPIAvailable(AP_VideoAPI api);

bool AP_VideoAPIImplemented(AP_VideoAPI api);

AP_VideoConfig AP_VideoDefaultConfig(void);

/* ---------------------------------------------------------
 * Subsystem metadata
 * --------------------------------------------------------- */

extern const AP_SubsystemMetadata AP_VideoSubsystem;

#ifdef __cplusplus
}
#endif

#endif /* AP2_VIDEO_H */
