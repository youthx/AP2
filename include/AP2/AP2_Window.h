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
 * Window configuration
 * --------------------------------------------------------- */

typedef struct AP_WindowConfig {
  const char *title;

  uint32_t width;
  uint32_t height;

  bool resizable;
  bool decorated;
  bool maximized;
  bool fullscreen;

} AP_WindowConfig;

/* ---------------------------------------------------------
 * Windowing subsystem
 * --------------------------------------------------------- */

bool AP_WindowingInit(void);

void AP_WindowingClose(void);

/* ---------------------------------------------------------
 * Window lifecycle
 * --------------------------------------------------------- */

AP_Window *AP_CreateWindow(const AP_WindowConfig *config);

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

void AP_WindowSetShouldClose(AP_Window *window, bool should_close);

/* ---------------------------------------------------------
 * Window events
 * --------------------------------------------------------- */

void AP_WindowPollEvents(void);

/* ---------------------------------------------------------
 * Buffer
 * --------------------------------------------------------- */

void AP_WindowSwapBuffers(AP_Window *window);

/* ---------------------------------------------------------
 * Window information
 * --------------------------------------------------------- */

void AP_WindowGetSize(const AP_Window *window, uint32_t *out_width,
                      uint32_t *out_height);

void AP_WindowGetFramebufferSize(const AP_Window *window, uint32_t *out_width,
                                 uint32_t *out_height);

const char *AP_WindowGetTitle(const AP_Window *window);

/* ---------------------------------------------------------
 * Subsystem metadata
 * --------------------------------------------------------- */

extern const AP_SubsystemMetadata AP_WindowingSubsystem;

#ifdef __cplusplus
}
#endif

#endif /* AP2_WINDOW_H */
