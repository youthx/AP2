/*
 * Example: 2D Drawing
 *
 * Shapes, gradients, and a rotating transform, all in immediate mode.
 */

#include <AP2/AP2.h>

#include "shapes_2d.h"

int Example_Shapes2D(void) {
  AP_Init(AP_INIT_VIDEO);
  AP_CreateWindow("AP2 - 2D Drawing", 1280, 720,
                  AP_WINDOW_CENTERED | AP_WINDOW_RESIZABLE);

  while (AP_IsRunning()) {
    AP_PumpEvents();

    if (AP_IsKeyPressed(AP_KEY_ESCAPE)) {
      AP_RequestClose();
    }

    AP_SetDrawColor(0.08f, 0.09f, 0.11f, 1.0f);
    AP_Clear();

    AP_SetDrawColor(0.95f, 0.35f, 0.35f, 1.0f);
    AP_FillRect(&(AP_FRect){80.0f, 80.0f, 240.0f, 140.0f});

    AP_SetDrawColor(0.35f, 0.75f, 0.95f, 1.0f);
    AP_FillRoundedRect(&(AP_FRect){360.0f, 80.0f, 200.0f, 140.0f}, 18.0f);

    AP_SetDrawColor(0.95f, 0.85f, 0.35f, 1.0f);
    AP_FillCircleF(200.0f, 360.0f, 48.0f);

    AP_SetDrawColor(0.55f, 0.85f, 0.45f, 1.0f);
    AP_FillNGon(720.0f, 200.0f, 64.0f, 6);

    AP_FillRectGradient(
        &(AP_FRect){80.0f, 560.0f, 320.0f, 80.0f},
        AP_C4(0.2f, 0.3f, 0.8f, 1.0f), AP_C4(0.8f, 0.3f, 0.5f, 1.0f),
        AP_C4(0.9f, 0.6f, 0.2f, 1.0f), AP_C4(0.1f, 0.5f, 0.4f, 1.0f));

    /* Spinning square about its own center */
    float t = (float)AP_GetTime();
    AP_PushTransform();
    AP_Translate(960.0f, 400.0f);
    AP_SetRotation(t * 40.0f);
    AP_SetDrawColor(0.9f, 0.4f, 0.2f, 1.0f);
    AP_FillRect(&(AP_FRect){-40.0f, -40.0f, 80.0f, 80.0f});
    AP_PopTransform();

    AP_Present();
  }

  AP_DestroyWindow(NULL);
  AP_Quit();
  return 0;
}
