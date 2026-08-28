#include "AP2/AP2_Init.h"

#include "AP2/AP2_Error.h"
#include "AP2/AP2_Logger.h"
#include "AP2/AP2_Video.h"
#include "AP2/AP2_Window.h"

#include <stddef.h>
#include <string.h>

/* ---------------------------------------------------------
 * Internal subsystem state
 * --------------------------------------------------------- */

typedef struct AP_SubsystemEntry {
  const char *name;
  AP_SubsystemMetadata metadata;
  bool registered;
  bool initialized;
} AP_SubsystemEntry;

static AP_SubsystemEntry g_subsystems[AP_SUBSYSTEM_COUNT];

/* ---------------------------------------------------------
 * Built-in subsystem registration
 * --------------------------------------------------------- */

static void AP_RegisterBuiltInSubsystems(void) {
  /*
   * Windowing
   */

  AP_RegisterSubsystem(AP_SUBSYSTEM_WINDOWING, "Windowing",
                       AP_WindowingSubsystem);

  /*
   * Video
   */

  AP_RegisterSubsystem(AP_SUBSYSTEM_VIDEO, "Video", AP_VideoSubsystem);

  /*
   * Audio is intentionally not registered yet.
   */
}

/* ---------------------------------------------------------
 * Register subsystem
 * --------------------------------------------------------- */

bool AP_RegisterSubsystem(AP_Subsystem subsystem, const char *name,
                          AP_SubsystemMetadata metadata) {
  if (subsystem < 0 || subsystem >= AP_SUBSYSTEM_COUNT) {

    AP_SET_ERROR(AP_ERROR_INVALID_ARGUMENT, "Invalid subsystem");

    return false;
  }

  if (!name) {
    AP_SET_ERROR(AP_ERROR_INVALID_ARGUMENT, "Subsystem name cannot be NULL");

    return false;
  }

  if (!metadata.init || !metadata.close) {
    AP_SET_ERROR(AP_ERROR_INVALID_ARGUMENT,
                 "Subsystem requires init and close callbacks");

    return false;
  }

  g_subsystems[subsystem].name = name;
  g_subsystems[subsystem].metadata = metadata;
  g_subsystems[subsystem].registered = true;
  g_subsystems[subsystem].initialized = false;

  return true;
}

/* ---------------------------------------------------------
 * Initialize subsystem
 * --------------------------------------------------------- */

bool AP_InitSubsystem(AP_Subsystem subsystem) {
  if (subsystem < 0 || subsystem >= AP_SUBSYSTEM_COUNT) {

    AP_SET_ERROR(AP_ERROR_INVALID_ARGUMENT, "Invalid subsystem");

    return false;
  }

  AP_SubsystemEntry *entry = &g_subsystems[subsystem];

  if (!entry->registered) {

    AP_SET_ERROR(AP_ERROR_NOT_INITIALIZED, "Subsystem has not been registered");

    return false;
  }

  if (entry->initialized) {
    return true;
  }

  AP_INFO("Initializing subsystem: %s", entry->name);

  if (!entry->metadata.init()) {

    AP_ERROR("Failed to initialize subsystem: %s", entry->name);

    return false;
  }

  entry->initialized = true;

  AP_INFO("Initialized subsystem: %s", entry->name);

  return true;
}

/* ---------------------------------------------------------
 * Query state
 * --------------------------------------------------------- */

bool AP_GetSubsystemInitialized(AP_Subsystem subsystem) {
  if (subsystem < 0 || subsystem >= AP_SUBSYSTEM_COUNT) {

    return false;
  }

  return g_subsystems[subsystem].initialized;
}

/* ---------------------------------------------------------
 * Close subsystem
 * --------------------------------------------------------- */

bool AP_CloseSubsystem(AP_Subsystem subsystem) {
  if (subsystem < 0 || subsystem >= AP_SUBSYSTEM_COUNT) {

    return false;
  }

  AP_SubsystemEntry *entry = &g_subsystems[subsystem];

  if (!entry->registered || !entry->initialized) {

    return true;
  }

  if (entry->metadata.close) {
    entry->metadata.close();
  }

  entry->initialized = false;

  return true;
}

/* ---------------------------------------------------------
 * AP2 initialization
 * --------------------------------------------------------- */

bool AP_Init(AP_InitFlags flags) {
  AP_INFO("Initializing AP2");

  /*
   * Register all built-in subsystems before attempting
   * to initialize anything.
   */

  AP_RegisterBuiltInSubsystems();

  /*
   * Initialize in dependency order.
   *
   * Windowing MUST come before Video because Video's
   * OpenGL device requires an active GLFW context.
   */

  AP_Subsystem initialization_order[] = {
      AP_SUBSYSTEM_WINDOWING, AP_SUBSYSTEM_VIDEO, AP_SUBSYSTEM_AUDIO};

  size_t count = sizeof(initialization_order) / sizeof(initialization_order[0]);

  for (size_t i = 0; i < count; ++i) {

    AP_Subsystem subsystem = initialization_order[i];

    AP_InitFlags flag = ((AP_InitFlags)1u << subsystem);

    if (!(flags & flag)) {
      continue;
    }

    /*
     * Don't fail AP_Init because a subsystem hasn't
     * been implemented/registered yet.
     */

    if (!g_subsystems[subsystem].registered) {
      AP_INFO("Skipping unregistered subsystem: %s",
              subsystem == AP_SUBSYSTEM_AUDIO ? "Audio" : "Unknown");

      continue;
    }

    if (!AP_InitSubsystem(subsystem)) {

      AP_ERROR("AP2 initialization failed; rolling back");

      AP_Quit();

      return false;
    }
  }

  AP_INFO("AP2 initialized successfully");

  return true;
}

/* ---------------------------------------------------------
 * AP2 shutdown
 * --------------------------------------------------------- */

void AP_Quit(void) {
  /*
   * Reverse dependency order.
   *
   * Video must close before Windowing.
   */

  for (int i = AP_SUBSYSTEM_COUNT - 1; i >= 0; --i) {

    AP_CloseSubsystem((AP_Subsystem)i);
  }

  AP_INFO("AP2 shutdown complete");
}
