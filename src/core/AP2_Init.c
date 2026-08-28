#ifndef AP2_INIT_H
#define AP2_INIT_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum AP_Subsystem {
  AP_SUBSYSTEM_VIDEO = 0,
  AP_SUBSYSTEM_WINDOWING,
  AP_SUBSYSTEM_AUDIO,

  AP_SUBSYSTEM_COUNT
} AP_Subsystem;

/* ---------------------------------------------------------
 * Initialization flags
 * --------------------------------------------------------- */

typedef uint32_t AP_InitFlags;

#define AP_INIT_NONE ((AP_InitFlags)0)

#define AP_INIT_VIDEO ((AP_InitFlags)(1u << AP_SUBSYSTEM_VIDEO))

#define AP_INIT_WINDOWING ((AP_InitFlags)(1u << AP_SUBSYSTEM_WINDOWING))

#define AP_INIT_AUDIO ((AP_InitFlags)(1u << AP_SUBSYSTEM_AUDIO))

#define AP_INIT_ALL (AP_INIT_VIDEO | AP_INIT_WINDOWING | AP_INIT_AUDIO)

/* ---------------------------------------------------------
 * Subsystem metadata
 * --------------------------------------------------------- */

typedef struct AP_SubsystemMetadata {
  bool (*init)(void);
  void (*close)(void);
} AP_SubsystemMetadata;

/* ---------------------------------------------------------
 * Subsystems
 * --------------------------------------------------------- */

bool AP_RegisterSubsystem(AP_Subsystem subsystem, const char *name,
                          AP_SubsystemMetadata metadata);

bool AP_InitSubsystem(AP_Subsystem subsystem);

bool AP_GetSubsystemInitialized(AP_Subsystem subsystem);

bool AP_CloseSubsystem(AP_Subsystem subsystem);

/* ---------------------------------------------------------
 * AP2 lifecycle
 * --------------------------------------------------------- */

bool AP_Init(AP_InitFlags flags);
void AP_Quit(void);

#ifdef __cplusplus
}
#endif

#endif /* AP2_INIT_H */
