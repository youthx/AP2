#include "AP2/AP2.h"
#include <stdio.h>

int main() {
  AP_Init(AP_INIT_ALL);

  AP_Window *window = AP_CreateWindow("Blank Window", 800, 600);

  AP_INFO("Successfully created window!");

  AP_SetActiveWindow(window);

  while (!AP_WindowShouldClose(window)) {
    AP_WindowPollEvents();

    AP_WindowSwapBuffers(window);
  }

  AP_DestroyWindow(window);
  return 0;
}
