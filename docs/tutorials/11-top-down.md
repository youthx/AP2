# Tutorial 11 — Top-down walker

Copyright (c) 2024-2026 Jack Waechter. MIT licensed.

A tiny exploration sketch: WASD / stick to move, a camera that follows, footsteps that spatialize around the listener.

2D draw space is still top-left Y-down. Spatial audio 2D helpers use the same XY so a crate on the right of the player pans right.

## State

```c
#include <AP2/AP2.h>
#include <math.h>
#include <stdio.h>

typedef struct Entity {
  float x;
  float y;
  float r;
} Entity;
```

## Movement and camera

```c
int main(void) {
  AP_Init(AP_INIT_VIDEO | AP_INIT_AUDIO);
  AP_CreateWindow("Walker", 1280, 720, AP_WINDOW_RESIZABLE | AP_WINDOW_VSYNC);

  AP_Sound *step = AP_CreateSoundWave(AP_WAVEFORM_NOISE, 180.0f, 0.05f, 0.25f);
  AP_Sound *ping = AP_CreateSoundWave(AP_WAVEFORM_SINE, 990.0f, 0.10f, 0.4f);

  Entity player = {640.0f, 2000.0f, 14.0f};
  Entity beacon = {900.0f, 1800.0f, 10.0f};
  float cam_x = player.x;
  float cam_y = player.y;
  float step_cd = 0.0f;
  double last = AP_GetTime();
  int win_w = 1280;
  int win_h = 720;

  while (AP_IsRunning()) {
    AP_PumpEvents();
    AP_GetWindowSize(&win_w, &win_h);
    if (AP_IsKeyPressed(AP_KEY_ESCAPE)) {
      AP_RequestClose();
    }

    double now = AP_GetTime();
    float dt = (float)(now - last);
    last = now;
    if (dt > 0.05f) {
      dt = 0.05f;
    }

    float ax = 0.0f;
    float ay = 0.0f;
    if (AP_IsKeyDown(AP_KEY_A) || AP_IsKeyDown(AP_KEY_LEFT)) {
      ax -= 1.0f;
    }
    if (AP_IsKeyDown(AP_KEY_D) || AP_IsKeyDown(AP_KEY_RIGHT)) {
      ax += 1.0f;
    }
    if (AP_IsKeyDown(AP_KEY_W) || AP_IsKeyDown(AP_KEY_UP)) {
      ay -= 1.0f;
    }
    if (AP_IsKeyDown(AP_KEY_S) || AP_IsKeyDown(AP_KEY_DOWN)) {
      ay += 1.0f;
    }

    int pad = AP_GetFirstGamepad();
    if (pad >= 0) {
      AP_Vec2 st = AP_GetGamepadStick(pad, AP_GAMEPAD_STICK_LEFT);
      ax += st.x;
      ay += st.y; /* stick up is negative, matches Y-down walk */
    }

    float len = sqrtf(ax * ax + ay * ay);
    if (len > 1.0f) {
      ax /= len;
      ay /= len;
    }

    float speed = 220.0f;
    player.x += ax * speed * dt;
    player.y += ay * speed * dt;

    cam_x = AP_Lerpf(cam_x, player.x, 1.0f - powf(0.001f, dt));
    cam_y = AP_Lerpf(cam_y, player.y, 1.0f - powf(0.001f, dt));

    AP_SetListenerPosition2D(player.x, player.y);

    step_cd -= dt;
    if (len > 0.2f && step_cd <= 0.0f) {
      AP_PlaySoundDesc d = AP_PlaySoundDescDefault();
      d.spatial = true;
      d.volume = 0.5f;
      d.position = AP_V3(player.x, player.y, 0.0f);
      d.min_distance = 24.0f;
      d.max_distance = 400.0f;
      AP_PlayOneShotEx(step, &d);
      step_cd = 0.28f;
    }

    if (AP_IsKeyPressed(AP_KEY_SPACE) ||
        (pad >= 0 && AP_IsGamepadButtonPressed(pad, AP_GAMEPAD_A))) {
      AP_PlaySoundDesc d = AP_PlaySoundDescDefault();
      d.spatial = true;
      d.position = AP_V3(beacon.x, beacon.y, 0.0f);
      d.min_distance = 40.0f;
      d.max_distance = 800.0f;
      AP_PlayOneShotEx(ping, &d);
    }

    AP_SetDrawColor(0.09f, 0.12f, 0.10f, 1.0f);
    AP_Clear();

    AP_PushTransform();
    AP_Translate((float)win_w * 0.5f - cam_x, (float)win_h * 0.5f - cam_y);

    AP_SetDrawColor(1.0f, 1.0f, 1.0f, 0.06f);
    AP_DrawGrid(&(AP_FRect){0.0f, 0.0f, 2000.0f, 2000.0f}, 20, 20);

    AP_SetDrawColor(0.95f, 0.75f, 0.25f, 1.0f);
    AP_FillCircleF(beacon.x, beacon.y, beacon.r);

    AP_SetDrawColor(0.35f, 0.80f, 0.95f, 1.0f);
    AP_FillCircleF(player.x, player.y, player.r);

    AP_PopTransform();

    AP_SetDrawColor(1.0f, 1.0f, 1.0f, 1.0f);
    AP_DrawText(16.0f, 16.0f, "WASD / stick  |  Space: ping the beacon");

    AP_Present();
  }

  AP_DestroySound(step);
  AP_DestroySound(ping);
  AP_DestroyWindow(NULL);
  AP_Quit();
  return 0;
}
```

Walk toward the beacon and tap Space: the ping should pan and get quieter with distance.

Swap `AP_FillCircleF` for `AP_DrawSprite` once you have a texture ([tutorial 03](03-sprites-and-textures.md)), or drop in a tilemap for the ground ([tutorial 13](13-tilemaps.md)).