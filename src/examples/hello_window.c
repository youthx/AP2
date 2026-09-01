/*
 * Example: Hello Window
 *
 * The smallest possible AP2 app: open a window, clear it, swap.
 */

#include <AP2/AP2.h>

#include "hello_window.h"

int Example_HelloWindow(void) {
  if (!AP_Init(AP_INIT_VIDEO)) {
    return 1;
  }

  if (!AP_CreateWindow("Hello AP2", 1280, 720,
                       AP_WINDOW_RESIZABLE | AP_WINDOW_VSYNC)) {
    AP_Quit();
    return 1;
  }

  while (AP_IsRunning()) {
    AP_PumpEvents();

    if (AP_IsKeyPressed(AP_KEY_ESCAPE)) {
      AP_RequestClose();
    }

    AP_SetDrawColor(0.08f, 0.09f, 0.11f, 1.0f);
    AP_Clear();
    AP_Present();
  }

  AP_DestroyWindow(NULL);
  AP_Quit();
  return 0;
}
