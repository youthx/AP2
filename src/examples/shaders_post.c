/*
 * Example: Shaders & Post-Processing
 *
 * Cycles through the built-in post stack (AP2_Post.h) plus the extended
 * creative effects (AP2_Post_extra.h) over a spinning cube.
 */

#include <AP2/AP2.h>
#include <math.h>

#include "shaders_post.h"

int Example_ShadersPost(void)
{
  AP_Init(AP_INIT_VIDEO);
  AP_CreateWindow("AP2 - Shaders & Post", 1280, 720,
                  AP_WINDOW_CENTERED | AP_WINDOW_RESIZABLE);

  AP_Mesh *cube = AP_CreateMeshCube(1.5f);
  AP_Camera camera = AP_CameraPerspective(AP_V3(0.0f, 2.0f, 5.0f),
                                          AP_V3(0.0f, 0.0f, 0.0f), 50.0f);

  AP_SetPostEnabled(true);
  AP_SetPostVignette(0.4f);
  AP_SetPostBloom(0.7f, 0.35f);
  AP_SetPostColorGrade(1.05f, 1.08f, 0.02f);
  AP_SetPostGrain(0.05f);
  AP_SetPostMSAA(4); /* antialias the cube + GUI while post is capturing */

  /* Extended creative effects, all off until the sliders below ramp them up */
  AP_SetPostCRT(0.0f);
  AP_SetPostScanlines(0.0f);
  AP_SetPostWave(0.0f);
  AP_SetPostChromatic(0.002f);

  while (AP_IsRunning())
  {
    AP_PumpEvents();

    if (AP_IsKeyPressed(AP_KEY_ESCAPE))
    {
      AP_RequestClose();
    }

    float t = (float)AP_GetTime();

    /* Slowly cycle a couple of the extended stylized effects */
    AP_SetPostCRT(0.5f + 0.5f * (float)sin(t * 0.5));
    AP_SetPostScanlines(0.3f);
    AP_SetPostWave(0.15f);

    AP_SetDrawColor(0.05f, 0.05f, 0.07f, 1.0f);
    AP_Clear();

    AP_ClearLights();
    AP_Begin3D(&camera);
    AP_AddLight(AP_LightDirectional(AP_V3(0.3f, -1.0f, 0.4f),
                                    AP_C4(1.0f, 1.0f, 1.0f, 1.0f), 1.0f));
    AP_SetAmbientLight(AP_C4(0.1f, 0.1f, 0.12f, 1.0f));

    AP_Reset3DModel();
    AP_Rotate3D(AP_V3(0.0f, 1.0f, 0.0f), t * 30.0f);
    AP_DrawMeshEx(cube, NULL, AP_C4(0.9f, 0.4f, 0.2f, 1.0f));

    AP_End3D();
    AP_Present();
  }

  AP_DestroyMesh(cube);
  AP_DestroyWindow(NULL);
  AP_Quit();
  return 0;
}
