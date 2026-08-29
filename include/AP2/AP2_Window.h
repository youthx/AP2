/*
 * AP2 — Application Primitives
 * Copyright (c) 2024-2026 Jack Waechter
 *
 * Licensed under the MIT License.
 * See LICENSE in the project root for full terms.
 */

#ifndef AP2_WINDOW_H
#define AP2_WINDOW_H

#include "AP2/AP2_Init.h"
#include "AP2/AP2_Types.h"

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * AP2 Window
 *
 * The window implementation is opaque. Applications never access
 * native handles, GLFW, or window internals.
 *
 * After AP_CreateWindow(), every function below operates on the
 * active window. Typical usage:
 *
 *     AP_CreateWindow("Game", 1280, 720, AP_WINDOW_RESIZABLE);
 *
 *     AP_SetWindowTitleVisible(false);
 *     AP_SetWindowMinimizeButton(false);
 *
 *     while (AP_IsRunning()) {
 *         AP_PumpEvents();
 *         AP_SetRenderDrawColorFloat(0.1f, 0.1f, 0.1f, 1.0f);
 *         AP_RenderClear();
 *         AP_RenderPresent();
 *     }
 *
 *     AP_DestroyWindow(NULL);
 */

typedef struct AP_Window AP_Window;

/* =========================================================
 * Position sentinels
 * ========================================================= */

#define AP_WINDOW_POS_UNDEFINED 0x1FFF0000 /* "put it wherever" */
#define AP_WINDOW_POS_CENTERED 0x2FFF0000 /* not a pixel. don't add it to one. */

/* =========================================================
 * Window flags
 * ========================================================= */

typedef enum AP_WindowFlags {
  AP_WINDOW_NONE = 0,
  AP_WINDOW_RESIZABLE = 1u << 0,
  AP_WINDOW_DECORATED = 1u << 1,
  AP_WINDOW_MAXIMIZED = 1u << 2,
  AP_WINDOW_FULLSCREEN = 1u << 3,
  AP_WINDOW_HIDDEN = 1u << 4,
  AP_WINDOW_FLOATING = 1u << 5,
  AP_WINDOW_TRANSPARENT = 1u << 6,
  AP_WINDOW_VSYNC = 1u << 7,
  AP_WINDOW_CENTERED = 1u << 8,
  AP_WINDOW_MINIMIZED = 1u << 9,
  AP_WINDOW_FOCUSED = 1u << 10,
  AP_WINDOW_HIGH_DPI = 1u << 11,
  AP_WINDOW_MOUSE_PASSTHROUGH = 1u << 12,
  AP_WINDOW_FOCUS_ON_SHOW = 1u << 13,
  AP_WINDOW_SCALE_TO_MONITOR = 1u << 14,
  AP_WINDOW_MSAA = 1u << 15,
  AP_WINDOW_SRGB = 1u << 16,
  AP_WINDOW_DEBUG = 1u << 17,
  AP_WINDOW_NO_AUTO_ICONIFY = 1u << 18,
  AP_WINDOW_NO_FOCUS = 1u << 19,
  AP_WINDOW_CENTER_CURSOR = 1u << 20,
  AP_WINDOW_BORDERLESS = 1u << 21,
  AP_WINDOW_NO_TITLE = 1u << 22, /* hide caption text; bar can stay */
  AP_WINDOW_NO_MINIMIZE = 1u << 23,
  AP_WINDOW_NO_MAXIMIZE = 1u << 24,
  AP_WINDOW_NO_CLOSE = 1u << 25
} AP_WindowFlags;

#define AP_WINDOW_HIGH_PIXEL_DENSITY AP_WINDOW_HIGH_DPI
#define AP_WINDOW_ALWAYS_ON_TOP AP_WINDOW_FLOATING
#define AP_WINDOW_NOT_FOCUSABLE AP_WINDOW_NO_FOCUS
#define AP_WINDOW_INPUT_FOCUS AP_WINDOW_FOCUSED
#define AP_WINDOW_NO_BUTTONS                                                       \
  (AP_WINDOW_NO_MINIMIZE | AP_WINDOW_NO_MAXIMIZE | AP_WINDOW_NO_CLOSE)

/* =========================================================
 * Window configuration
 * ========================================================= */

typedef struct AP_WindowConfig {
  const char *title;

  int width;
  int height;

  int x;
  int y;

  uint32_t flags;

  int monitor_index;
  int swap_interval;

  /*
   * Multisample count used when AP_WINDOW_MSAA is set.
   * 0 selects a default of 4 samples.
   */
  int msaa_samples;

  /*
   * Window opacity in the range (0, 1]. 0 selects fully opaque.
   */
  float opacity;

  int min_width;
  int min_height;
  int max_width;
  int max_height;
} AP_WindowConfig;

/*
 * Returns the default window configuration:
 *   1280x720, resizable, decorated, visible, VSync, centered.
 */
AP_WindowConfig AP_WindowDefaultConfig(void);

bool AP_WindowValidateConfig(const AP_WindowConfig *config);

/* =========================================================
 * Creation
 *
 * The renderer is created automatically. Applications must not
 * create a renderer themselves.
 * ========================================================= */

AP_Window *AP_CreateWindow(const char *title, int width, int height,
                           AP_WindowFlags flags);

AP_Window *AP_CreateWindowEx(const AP_WindowConfig *config);

/*
 * Destroys a window. NULL means the active one.
 */
void AP_DestroyWindow(AP_Window *window);

bool AP_WindowIsValid(const AP_Window *window);

/* =========================================================
 * Active window
 * ========================================================= */

AP_Window *AP_GetWindow(void);

bool AP_SetActiveWindow(AP_Window *window);

/* =========================================================
 * Events
 *
 * Pumps window events and updates input state for this frame.
 * ========================================================= */

void AP_PumpEvents(void);

void AP_WaitEvents(void);

void AP_WaitEventsTimeout(double timeout);

#define AP_PollEvents AP_PumpEvents /* SDL muscle memory */

/* =========================================================
 * Close / main loop
 * ========================================================= */

/*
 * Returns true while the active window exists and has not been
 * asked to close.
 */
bool AP_IsRunning(void);

void AP_RequestClose(void);

void AP_CancelClose(void);

bool AP_WindowShouldClose(const AP_Window *window);

void AP_WindowSetShouldClose(AP_Window *window, bool should_close);

/* =========================================================
 * Title
 * ========================================================= */

bool AP_SetWindowTitle(const char *title);

const char *AP_GetWindowTitle(void);

/* =========================================================
 * Size
 * ========================================================= */

bool AP_SetWindowSize(int width, int height);

bool AP_GetWindowSize(int *w, int *h);

int AP_GetWindowWidth(void);

int AP_GetWindowHeight(void);

#define AP_GetWindowSizeEx AP_GetWindowSize

/* =========================================================
 * Framebuffer size
 * ========================================================= */

bool AP_GetWindowSizeInPixels(int *w, int *h);

int AP_GetWindowPixelWidth(void);

int AP_GetWindowPixelHeight(void);

#define AP_GetFramebufferSizeEx AP_GetWindowSizeInPixels
#define AP_GetFramebufferWidth AP_GetWindowPixelWidth
#define AP_GetFramebufferHeight AP_GetWindowPixelHeight

/* =========================================================
 * Position
 * ========================================================= */

bool AP_SetWindowPosition(int x, int y);

bool AP_GetWindowPosition(int *x, int *y);

#define AP_GetWindowPositionEx AP_GetWindowPosition

bool AP_CenterWindow(void);

/* =========================================================
 * Visibility / focus
 * ========================================================= */

void AP_ShowWindow(void);

void AP_HideWindow(void);

bool AP_IsWindowVisible(void);

void AP_RaiseWindow(void);

bool AP_IsWindowFocused(void);

void AP_FlashWindow(void);

#define AP_FocusWindow AP_RaiseWindow
#define AP_RequestWindowAttention AP_FlashWindow

/* =========================================================
 * Window state
 * ========================================================= */

void AP_MinimizeWindow(void);

void AP_MaximizeWindow(void);

void AP_RestoreWindow(void);

bool AP_IsWindowMinimized(void);

bool AP_IsWindowMaximized(void);

bool AP_IsWindowOpen(void);

/* =========================================================
 * Fullscreen
 * ========================================================= */

bool AP_SetWindowFullscreen(bool fullscreen);

bool AP_IsFullscreen(void);

bool AP_ToggleFullscreen(void);

#define AP_SetFullscreen AP_SetWindowFullscreen

/* =========================================================
 * Monitor
 * ========================================================= */

int AP_GetMonitorCount(void);

int AP_GetWindowMonitor(void);

bool AP_SetWindowMonitor(int monitor_index);

/* =========================================================
 * Window attributes
 * ========================================================= */

bool AP_SetWindowResizable(bool enabled);

bool AP_IsWindowResizable(void);

bool AP_SetWindowBordered(bool bordered);

bool AP_IsWindowBordered(void);

/*
 * Caption chrome. These apply when the window is decorated.
 * Title text can be hidden while the bar and buttons stay.
 * Min / max hide the buttons. Close disables the button (and the
 * OS close request); AP_RequestClose still works.
 */
bool AP_SetWindowTitleVisible(bool visible);

bool AP_IsWindowTitleVisible(void);

bool AP_SetWindowMinimizeButton(bool visible);

bool AP_IsWindowMinimizeButton(void);

bool AP_SetWindowMaximizeButton(bool visible);

bool AP_IsWindowMaximizeButton(void);

bool AP_SetWindowCloseButton(bool visible);

bool AP_IsWindowCloseButton(void);

bool AP_SetWindowAlwaysOnTop(bool on_top);

bool AP_IsWindowAlwaysOnTop(void);

#define AP_SetWindowDecorated AP_SetWindowBordered
#define AP_IsWindowDecorated AP_IsWindowBordered
#define AP_SetWindowFloating AP_SetWindowAlwaysOnTop
#define AP_IsWindowFloating AP_IsWindowAlwaysOnTop

bool AP_SetWindowOpacity(float opacity);

float AP_GetWindowOpacity(void);

bool AP_SetWindowMousePassthrough(bool enabled);

bool AP_IsWindowMousePassthrough(void);

bool AP_SetWindowSizeLimits(int min_width, int min_height, int max_width,
                            int max_height);

bool AP_SetWindowAspectRatio(int numerator, int denominator);

bool AP_SetWindowAutoIconify(bool enabled);

bool AP_GetWindowAutoIconify(void);

/* =========================================================
 * Presentation
 * ========================================================= */

bool AP_SetSwapInterval(int interval);

int AP_GetSwapInterval(void);

bool AP_SetVSync(bool enabled);

bool AP_GetVSync(void);

/* =========================================================
 * Cursor
 * ========================================================= */

void AP_SetCursorVisible(bool visible);

bool AP_IsCursorVisible(void);

void AP_SetCursorLocked(bool locked);

bool AP_IsCursorLocked(void);

bool AP_SetRawMouseInput(bool enabled);

bool AP_IsRawMouseInput(void);

bool AP_GetCursorPosition(double *x, double *y);

bool AP_SetCursorPosition(double x, double y);

double AP_GetCursorX(void);

double AP_GetCursorY(void);

/* =========================================================
 * Content scale / DPI
 * ========================================================= */

float AP_GetContentScaleX(void);

float AP_GetContentScaleY(void);

bool AP_GetContentScale(float *x, float *y);

/* =========================================================
 * User data
 * ========================================================= */

void AP_SetWindowUserData(void *user_data);

void *AP_GetWindowUserData(void);

/* =========================================================
 * Flags
 * ========================================================= */

uint32_t AP_GetWindowFlags(void);

bool AP_WindowHasFlag(AP_WindowFlags flag);

bool AP_SetWindowFlags(uint32_t flags);

bool AP_EnableWindowFlag(AP_WindowFlags flag);

bool AP_DisableWindowFlag(AP_WindowFlags flag);

/* =========================================================
 * Native handle
 *
 * Platform-specific window pointer (HWND, NSWindow, X11 Window).
 * Intended for advanced embedding only.
 * ========================================================= */

void *AP_GetNativeHandle(void);

void *AP_GetNativeDisplay(void);

/* =========================================================
 * Time
 * ========================================================= */

double AP_GetTime(void);

uint64_t AP_GetTicks(void);

void AP_SetTime(double time);

/* =========================================================
 * Subsystem
 * ========================================================= */

extern const AP_SubsystemMetadata AP_WindowingSubsystem;

#ifdef __cplusplus
}
#endif

#endif /* AP2_WINDOW_H */
