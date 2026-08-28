#ifndef AP2_WINDOW_H
#define AP2_WINDOW_H

#include "AP2/AP2_Init.h"

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ---------------------------------------------------------
 * Window
 * --------------------------------------------------------- */

typedef struct AP_Window AP_Window;

/* ---------------------------------------------------------
 * Windowing subsystem
 * --------------------------------------------------------- */

bool AP_WindowingInit(void);
void AP_WindowingClose(void);

extern const AP_SubsystemMetadata AP_WindowingSubsystem;

/* ---------------------------------------------------------
 * Window lifecycle
 * --------------------------------------------------------- */

AP_Window *AP_CreateWindow(const char *title, uint32_t width, uint32_t height);

void AP_DestroyWindow(AP_Window *window);

/* ---------------------------------------------------------
 * Active window
 * --------------------------------------------------------- */

bool AP_SetActiveWindow(AP_Window *window);
AP_Window *AP_GetActiveWindow(void);

/* ---------------------------------------------------------
 * Window state
 * --------------------------------------------------------- */

bool AP_WindowShouldClose(const AP_Window *window);

void AP_WindowGetSize(const AP_Window *window, uint32_t *out_width,
                      uint32_t *out_height);

/* ---------------------------------------------------------
 * Window operations
 * --------------------------------------------------------- */

void AP_WindowSwapBuffers(AP_Window *window);
void AP_WindowPollEvents(void);

#ifdef __cplusplus
}
#endif

#endif /* AP2_WINDOW_H */
