/*
 * AP2 — Application Primitives
 * Copyright (c) 2026 Jack Waechter
 *
 * Licensed under the MIT License.
 * See LICENSE in the project root for full terms.
 */

#ifndef AP2_INIT_H
#define AP2_INIT_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* =========================================================
 * AP2 Subsystems
 * ========================================================= */

typedef enum AP_Subsystem {
  /*
   * Windowing must initialize before Video.
   */
  AP_SUBSYSTEM_WINDOWING = 0,

  /*
   * Video owns the graphics device/backend.
   */
  AP_SUBSYSTEM_VIDEO,

  /*
   * Audio is independent of the graphics stack.
   */
  AP_SUBSYSTEM_AUDIO,

  AP_SUBSYSTEM_COUNT
} AP_Subsystem;

/* =========================================================
 * Initialization Flags
 * ========================================================= */

typedef uint32_t AP_InitFlags;

/*
 * No subsystems.
 */
#define AP_INIT_NONE ((AP_InitFlags)0)

/*
 * Windowing subsystem.
 */
#define AP_INIT_WINDOWING ((AP_InitFlags)(1u << AP_SUBSYSTEM_WINDOWING))

/*
 * Video subsystem.
 */
#define AP_INIT_VIDEO ((AP_InitFlags)(1u << AP_SUBSYSTEM_VIDEO))

/*
 * Audio subsystem.
 */
#define AP_INIT_AUDIO ((AP_InitFlags)(1u << AP_SUBSYSTEM_AUDIO))

/*
 * All AP2 subsystems.
 */
#define AP_INIT_ALL (AP_INIT_WINDOWING | AP_INIT_VIDEO | AP_INIT_AUDIO)

/* =========================================================
 * Subsystem Metadata
 * ========================================================= */

typedef struct AP_SubsystemMetadata {
  /*
   * Called when the subsystem is initialized.
   *
   * Return true on success.
   */
  bool (*init)(void);

  /*
   * Called when the subsystem is shut down.
   */
  void (*close)(void);

} AP_SubsystemMetadata;

/* =========================================================
 * Subsystem Registration
 * ========================================================= */

/*
 * Registers a subsystem with AP2.
 *
 * This is primarily intended for AP2's internal subsystem
 * registration system.
 */
bool AP_RegisterSubsystem(AP_Subsystem subsystem, const char *name,
                          AP_SubsystemMetadata metadata);

/* =========================================================
 * Subsystem Lifecycle
 * ========================================================= */

/*
 * Initializes one registered subsystem.
 */
bool AP_InitSubsystem(AP_Subsystem subsystem);

/*
 * Returns whether a subsystem has been successfully
 * initialized.
 */
bool AP_GetSubsystemInitialized(AP_Subsystem subsystem);

/*
 * Shuts down one initialized subsystem.
 */
bool AP_CloseSubsystem(AP_Subsystem subsystem);

/* =========================================================
 * AP2 Lifecycle
 * ========================================================= */

/*
 * Initializes AP2.
 *
 * Multiple subsystem flags may be combined:
 *
 *     AP_Init(AP_INIT_WINDOWING | AP_INIT_VIDEO);
 *
 * AP_INIT_VIDEO also enables windowing. AP_INIT_ALL starts every
 * registered subsystem.
 */
bool AP_Init(AP_InitFlags flags);

/*
 * Shuts down AP2 and all initialized subsystems.
 */
void AP_Quit(void);

/*
 * Returns whether AP2 itself is currently initialized.
 */
bool AP_IsInitialized(void);

#ifdef __cplusplus
}
#endif

#endif /* AP2_INIT_H */
