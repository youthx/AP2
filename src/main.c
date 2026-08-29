/*
 * AP2 — Application Primitives
 * Copyright (c) 2024-2026 Jack Waechter
 *
 * Licensed under the MIT License.
 * See LICENSE in the project root for full terms.
 *
 * Sample: immediate 3D scene with an orbit camera.
 *   Left drag  — orbit
 *   Scroll     — zoom
 *   Escape     — quit
 */

#include <AP2/AP2.h>

#include <math.h>
#include <stdio.h>

#define AP_SAMPLE_PI 3.14159265358979323846f
#define AP_SAMPLE_FOV 50.0f
#define AP_SAMPLE_YAW_SENS 0.35f
#define AP_SAMPLE_PITCH_SENS 0.35f
#define AP_SAMPLE_ZOOM_SENS 1.15f
#define AP_SAMPLE_PITCH_MIN -80.0f
#define AP_SAMPLE_PITCH_MAX 80.0f
#define AP_SAMPLE_DIST_MIN 3.0f
#define AP_SAMPLE_DIST_MAX 28.0f

typedef struct AP_SampleCamera {
  float yaw_deg;
  float pitch_deg;
  float distance;
  AP_Vec3 target;
} AP_SampleCamera;

static AP_Vec3 AP_SampleOrbitEye(const AP_SampleCamera *orbit) {
  float yaw = orbit->yaw_deg * (AP_SAMPLE_PI / 180.0f);
  float pitch = orbit->pitch_deg * (AP_SAMPLE_PI / 180.0f);
  float cos_pitch = cosf(pitch);

  return AP_V3(orbit->target.x + sinf(yaw) * cos_pitch * orbit->distance,
               orbit->target.y + sinf(pitch) * orbit->distance,
               orbit->target.z + cosf(yaw) * cos_pitch * orbit->distance);
}

static void AP_SampleCameraUpdate(AP_SampleCamera *orbit) {
  if (AP_IsMouseDown(AP_MOUSE_LEFT)) {
    orbit->yaw_deg += (float)AP_GetMouseDeltaX() * AP_SAMPLE_YAW_SENS;
    orbit->pitch_deg += (float)AP_GetMouseDeltaY() * AP_SAMPLE_PITCH_SENS;
    orbit->pitch_deg =
        AP_Clampf(orbit->pitch_deg, AP_SAMPLE_PITCH_MIN, AP_SAMPLE_PITCH_MAX);
  }

  orbit->distance -= (float)AP_GetMouseWheelY() * AP_SAMPLE_ZOOM_SENS;
  orbit->distance =
      AP_Clampf(orbit->distance, AP_SAMPLE_DIST_MIN, AP_SAMPLE_DIST_MAX);
}

static AP_Mat4 AP_SampleModel(AP_Vec3 position, float yaw_deg, float size) {
  return AP_Mat4Mul(AP_Mat4Translate(position),
                    AP_Mat4Mul(AP_Mat4RotateY(yaw_deg),
                               AP_Mat4Scale(AP_V3(size, size, size))));
}

static void AP_SampleDrawScene(AP_Mesh *cube, float time) {
  AP_Mat4 model;
  AP_Vec3 lamp;

  lamp = AP_V3(cosf(time * 0.7f) * 4.0f, 3.2f, sinf(time * 0.7f) * 4.0f);

  AP_ClearLights();
  AP_SetAmbientLight(AP_C4(0.10f, 0.11f, 0.14f, 1.0f));
  AP_AddLight(AP_LightDirectional(AP_V3(0.35f, 1.0f, 0.25f),
                                  AP_C4(0.75f, 0.82f, 0.95f, 1.0f), 0.45f));
  AP_AddLight(AP_LightPoint(lamp, AP_C4(1.0f, 0.72f, 0.38f, 1.0f), 2.4f, 14.0f));

  AP_Set3DShininess(48.0f);
  AP_Set3DSpecular(0.35f);

  AP_DrawGrid3D(12.0f, 12, AP_C4(0.28f, 0.32f, 0.38f, 1.0f));
  AP_DrawPlane(AP_V3(0.0f, 0.0f, 0.0f), 12.0f, 12.0f,
               AP_C4(0.16f, 0.18f, 0.22f, 1.0f));

  AP_DrawCube(AP_V3(0.0f, 0.5f, 0.0f), AP_V3(1.0f, 1.0f, 1.0f),
              AP_C4(0.90f, 0.42f, 0.28f, 1.0f));

  model = AP_SampleModel(AP_V3(-2.4f, 0.6f, 1.6f), time * 55.0f, 1.2f);
  AP_DrawMeshEx(cube, &model, AP_C4(0.28f, 0.62f, 0.95f, 1.0f));

  model = AP_SampleModel(AP_V3(2.6f, 0.45f, -1.8f), -time * 35.0f, 0.9f);
  AP_DrawMeshEx(cube, &model, AP_C4(0.35f, 0.82f, 0.55f, 1.0f));

  AP_DrawSphere(AP_V3(-0.4f, 0.55f, 2.6f), 0.55f,
                AP_C4(0.95f, 0.82f, 0.28f, 1.0f));

  AP_DrawSphere(lamp, 0.12f, AP_C4(1.0f, 0.85f, 0.45f, 1.0f));
  AP_DrawLine3D(lamp, AP_V3(lamp.x, 0.0f, lamp.z),
                AP_C4(1.0f, 0.70f, 0.30f, 0.45f));
}

static void AP_SampleDrawHud(const AP_SampleCamera *orbit, float time) {
  char line[96];

  AP_SetDrawColor(0.0f, 0.0f, 0.0f, 0.45f);
  AP_FillRect(&(AP_FRect){16.0f, 16.0f, 420.0f, 72.0f});

  AP_SetDrawColor(1.0f, 1.0f, 1.0f, 1.0f);
  AP_DrawText(28.0f, 28.0f, "AP2 3D sample");
  AP_DrawText(28.0f, 44.0f, "Left drag: orbit   Scroll: zoom   Esc: quit");

  snprintf(line, sizeof line, "yaw %.0f  pitch %.0f  dist %.1f  t %.1fs",
           orbit->yaw_deg, orbit->pitch_deg, orbit->distance, time);
  AP_DrawText(28.0f, 60.0f, line);
}

int main(void) {
  AP_SampleCamera orbit;
  AP_Camera camera;
  AP_Mesh *cube;

  orbit.yaw_deg = 35.0f;
  orbit.pitch_deg = 22.0f;
  orbit.distance = 10.0f;
  orbit.target = AP_V3(0.0f, 0.4f, 0.0f);

  if (!AP_Init(AP_INIT_VIDEO)) {
    AP_ERROR("init: %s", AP_GetErrorMessage());
    return 1;
  }

  if (!AP_CreateWindow("AP2 3D", 1280, 720,
                       AP_WINDOW_RESIZABLE | AP_WINDOW_HIGH_PIXEL_DENSITY |
                           AP_WINDOW_MSAA | AP_WINDOW_VSYNC)) {
    AP_ERROR("window: %s", AP_GetErrorMessage());
    AP_Quit();
    return 1;
  }

  cube = AP_CreateMeshCube(1.0f);

  while (AP_IsRunning()) {
    float time;

    AP_PumpEvents();
    if (AP_IsKeyPressed(AP_KEY_ESCAPE)) {
      AP_RequestClose();
    }

    time = (float)AP_GetTime();
    AP_SampleCameraUpdate(&orbit);
    camera = AP_CameraPerspective(AP_SampleOrbitEye(&orbit), orbit.target,
                                  AP_SAMPLE_FOV);

    AP_SetDrawColor(0.07f, 0.08f, 0.11f, 1.0f);
    AP_Clear();

    AP_Begin3D(&camera);
    AP_SampleDrawScene(cube, time);
    AP_End3D();

    AP_SampleDrawHud(&orbit, time);
    AP_Present();
  }

  AP_DestroyMesh(cube);
  AP_DestroyWindow(NULL);
  AP_Quit();
  return 0;
}
