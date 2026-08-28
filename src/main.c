#include "AP2/AP2.h"

int main() {
  if (!AP_Init(AP_INIT_ALL)) {
    AP_ERROR("Couldn't initialize AP2");
    return 1;
  }

  AP_WindowConfig config = {.title = "Blank Window",
                            .width = 800,
                            .height = 600,

                            .resizable = true,
                            .decorated = true,
                            .maximized = false,
                            .fullscreen = false};

  AP_Window *window = AP_CreateWindow(&config);
  if (!window) {
    AP_ERROR(AP_GetErrorMessage());
    AP_Quit();
    return 1;
  }

  AP_SetActiveWindow(window);

  while (!AP_WindowShouldClose(window)) {
    AP_WindowPollEvents();

    AP_WindowSwapBuffers(window);
  }

  AP_DestroyWindow(window);
  AP_Quit();

  return 0;
}
