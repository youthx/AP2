#ifndef AP2_INIT_H
#define AP2_INIT_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum AP_Subsystem {
  AP_SUBSYSTEM_VIDEO = 0,
  AP_SUBSYSTEM_AUDIO,
  AP_SUBSYSTEM_WINDOWING,

  AP_SUBSYSTEM_COUNT
} AP_Subsystem;

/*
 * Initialization flags.
 *
 * These are bit flags, so multiple subsystems can be initialized
 * with a single call:
 *
 *     AP_Init(AP_INIT_VIDEO | AP_INIT_AUDIO);
 */
typedef uint32_t AP_InitFlags;

#define AP_INIT_NONE ((AP_InitFlags)0)
#define AP_INIT_VIDEO ((AP_InitFlags)(1u << AP_SUBSYSTEM_VIDEO))
#define AP_INIT_AUDIO ((AP_InitFlags)(1u << AP_SUBSYSTEM_AUDIO))
#define AP_INIT_WINDOWING ((AP_InitFlags)(1u << AP_SUBSYSTEM_WINDOWING))

#define AP_INIT_ALL (AP_INIT_VIDEO | AP_INIT_AUDIO | AP_INIT_WINDOWING)

typedef struct AP_SubsystemMetadata {
  bool (*init)(void);
  void (*close)(void);
} AP_SubsystemMetadata;

/*
 * Registers a subsystem and its lifecycle callbacks.
 *
 * `name` is primarily intended for debugging/logging.
 */
bool AP_RegisterSubsystem(AP_Subsystem subsystem, const char *name,
                          AP_SubsystemMetadata metadata);

/*
 * Initializes a single registered subsystem.
 */
bool AP_InitSubsystem(AP_Subsystem subsystem);

/*
 * Initializes all subsystems specified by `flags`.
 */
bool AP_Init(AP_InitFlags flags);

/*
 * Returns whether a subsystem has successfully initialized.
 */
bool AP_GetSubsystemInitialized(AP_Subsystem subsystem);

/*
 * Shuts down a single initialized subsystem.
 */
bool AP_CloseSubsystem(AP_Subsystem subsystem);

/*
 * Shuts down all initialized subsystems.
 */
void AP_Quit(void);

#ifdef __cplusplus
}
#endif

#endif /* AP2_INIT_H */
