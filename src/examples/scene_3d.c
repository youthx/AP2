/*
 * Example: 3D Scene
 *
 * Orbit camera, mixed light types, and a few primitive meshes.
 */

#include <AP2/AP2.h>
#include <math.h>

#include "scene_3d.h"

int Example_Scene3D(void) {
  AP_Init(AP_INIT_VIDEO);
  AP_CreateWindow("AP2 - 3D Scene", 1280, 720,
                  AP_WINDOW_CENTERED | AP_WINDOW_RESIZABLE);

  AP_Mesh *sphere = AP_CreateMeshSphere(0.75f, 24, 16);
  AP_Mesh *plane = AP_CreateMeshPlane(8.0f, 8.0f);

  float yaw = 0.4f;
  float dist = 8.0f;

  while (AP_IsRunning()) {
    AP_PumpEvents();

    if (AP_IsKeyPressed(AP_KEY_ESCAPE)) {
      AP_RequestClose();
    }

    if (AP_IsMouseDown(AP_MOUSE_RIGHT)) {
      yaw += (float)AP_GetMouseDeltaX() * 0.01f;
    }
    dist = AP_Clampf(dist - (float)AP_GetMouseDeltaY() * 0.02f, 3.0f, 20.0f);

    AP_Vec3 eye = AP_V3(sinf(yaw) * dist, 4.0f, cosf(yaw) * dist);
    AP_Camera camera =
        AP_CameraPerspective(eye, AP_V3(0.0f, 0.0f, 0.0f), 50.0f);

    AP_SetDrawColor(0.08f, 0.09f, 0.12f, 1.0f);
    AP_Clear();

    AP_ClearLights();
    AP_Begin3D(&camera);

    AP_AddLight(AP_LightDirectional(AP_V3(0.4f, -1.0f, 0.3f),
                                    AP_C4(1.0f, 1.0f, 1.0f, 1.0f), 0.8f));
    AP_AddLight(AP_LightPoint(AP_V3(2.0f, 2.0f, 0.0f),
                              AP_C4(1.0f, 0.5f, 0.2f, 1.0f), 2.0f, 8.0f));
    AP_SetAmbientLight(AP_C4(0.12f, 0.13f, 0.16f, 1.0f));

    AP_DrawMesh(plane);
    AP_Set3DPosition(AP_V3(0.0f, 0.75f, 0.0f));
    AP_DrawMeshEx(sphere, NULL, AP_C4(0.2f, 0.7f, 0.9f, 1.0f));

    AP_Reset3DModel();
    AP_DrawGrid3D(10.0f, 10, AP_C4(0.35f, 0.38f, 0.42f, 1.0f));

    AP_End3D();
    AP_Present();
  }

  AP_DestroyMesh(sphere);
  AP_DestroyMesh(plane);
  AP_DestroyWindow(NULL);
  AP_Quit();
  return 0;
}
