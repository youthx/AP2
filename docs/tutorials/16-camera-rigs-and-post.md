# Tutorial 16 — Camera Rigs and Cinematic Post-Processing

Copyright (c) 2024-2026 Jack Waechter. MIT licensed.

This tutorial covers the rest of `AP2_Camera.h` beyond the basic orbit sketch in [3D](08-3d.md), plus the full effect catalog in `AP2_Post.h` / `AP2_Post_extra.h`.

## 1. First-person camera

Yaw/pitch plus local-space movement is the standard FPS/walkthrough rig:

```c
static AP_Camera cam;
static bool cam_ready = false;
float dt = (float)AP_GetDeltaTime();

if (!cam_ready) {
  cam = AP_CameraPerspective(AP_V3(0, 1.7f, 5.0f), AP_V3(0, 1.7f, 0), 65.0f);
  cam_ready = true;
}

if (AP_IsMouseDown(AP_MOUSE_RIGHT)) {
  float dx = (float)AP_GetMouseDeltaX();
  float dy = (float)AP_GetMouseDeltaY();
  AP_CameraRotateYawPitch(&cam, dx * 0.15f, -dy * 0.15f);
}

float forward = 0.0f, right = 0.0f, up = 0.0f;
if (AP_IsKeyDown(AP_KEY_W)) forward += 1.0f;
if (AP_IsKeyDown(AP_KEY_S)) forward -= 1.0f;
if (AP_IsKeyDown(AP_KEY_D)) right += 1.0f;
if (AP_IsKeyDown(AP_KEY_A)) right -= 1.0f;
if (AP_IsKeyDown(AP_KEY_E)) up += 1.0f;
if (AP_IsKeyDown(AP_KEY_Q)) up -= 1.0f;

AP_CameraMoveLocal(&cam, forward * 5.0f * dt, right * 5.0f * dt, up * 5.0f * dt);

AP_Begin3D(&cam);
```

`AP_CameraRotateYawPitch` updates `target` from the camera's current forward vector, so `position` and orientation stay independent — `AP_CameraMoveLocal` then walks `position` along that same forward/right/up basis.

To read the current facing back out (e.g. to show a compass or serialize a saved viewpoint):

```c
AP_Vec2 yp = AP_CameraYawPitch(&cam); /* x = yaw, y = pitch, degrees */
```

## 2. Orbit camera, revisited

The sketch in [3D](08-3d.md) manually computes `eye` with trig. `AP_CameraSetYawPitch` does the same job when you'd rather store yaw/pitch as your source of truth and derive `target`:

```c
static float orbit_yaw = 0.0f, orbit_pitch = 20.0f, orbit_dist = 6.0f;

orbit_yaw += (float)AP_GetMouseDeltaX() * 0.3f;
orbit_pitch = AP_Clampf(orbit_pitch - (float)AP_GetMouseDeltaY() * 0.3f, -80.0f, 80.0f);

AP_Camera orbit = AP_CameraPerspective(AP_V3(0, 0, orbit_dist), AP_V3(0, 0, 0), 50.0f);
AP_CameraSetYawPitch(&orbit, orbit_yaw, orbit_pitch);
AP_CameraTranslate(&orbit, AP_V3(0, 0, 0)); /* no-op placeholder for clarity */
```

## 3. Screen-space picking

Turn a mouse click into a world-space ray to select objects, place markers, or implement drag-and-drop onto a 3D scene:

```c
double mx, my;
AP_GetMousePosition(&mx, &my);
float aspect = (float)AP_GetWindowWidth() / (float)AP_GetWindowHeight();
AP_Vec2 screen01 = AP_V2((float)mx / (float)AP_GetWindowWidth(),
                        (float)my / (float)AP_GetWindowHeight());

AP_Ray ray = AP_CameraScreenRay(&cam, aspect, screen01);
/* intersect ray against your scene's bounding volumes / meshes */
```

## 4. Post-processing basics

Post-processing captures the frame offscreen at `AP_Clear()` and runs effects at `AP_Present()`. Turn it on once, then tune parameters per-frame or per-scene:

```c
AP_SetPostEnabled(true);
AP_SetGuiLayer(AP_GUI_LAYER_OVERLAY); /* UI stays crisp, unprocessed */
```

The base set in `AP2_Post.h`:

```c
AP_SetPostVignette(0.35f);
AP_SetPostBloom(1.1f, 0.4f);                 /* threshold, intensity */
AP_SetPostColorGrade(1.05f, 1.02f, 0.0f);    /* saturation, contrast, brightness */
AP_SetPostChromatic(0.02f);
AP_SetPostGrain(0.05f);
AP_SetPostSharpen(0.15f);
```

Or set everything at once via the config struct, useful for save/load of a "look" preset:

```c
AP_PostConfig look = AP_PostDefaultConfig();
look.enabled = true;
look.flags = AP_POST_VIGNETTE | AP_POST_BLOOM | AP_POST_COLOR_GRADE;
look.vignette = 0.35f;
look.bloom_threshold = 1.1f;
look.bloom_intensity = 0.4f;
look.saturation = 1.05f;
AP_SetPostConfig(&look);
```

## 5. Cinematic extras (`AP2_Post_extra.h`)

Exposure, tonemapping, and depth of field push a scene from "game" toward "rendered":

```c
AP_SetPostExposure(1.15f);
AP_SetPostFilmic(0.6f);          /* filmic tonemap blend */
AP_SetPostGamma(2.2f);

AP_SetPostDepthOfField(0.5f);
AP_SetPostDOFFocus(6.0f);        /* focal distance in world units */
AP_SetPostDOFAperture(0.15f);

AP_SetPostBloomRadius(1.4f);
AP_SetPostBloomSoftness(0.5f);
AP_SetPostLensDirt(0.2f);

AP_SetPostFog(0.3f);
AP_SetPostFogDensity(0.05f);
AP_SetPostFogHeight(2.0f);

AP_SetPostMotionBlur(0.25f);
```

Color science knobs live alongside the basic color grade:

```c
AP_SetPostTemperature(0.1f);  /* warm (+) / cool (-) white balance */
AP_SetPostTint(-0.05f);       /* green (-) / magenta (+) */
AP_SetPostClarity(0.2f);      /* local contrast */
AP_SetPostDetail(0.15f);      /* fine detail enhancement */
```

## 6. Stylized looks

The same pipeline can go the other direction — retro, glitchy, or cel-shaded — which is handy for menu screens, death/pause states, or a distinct art style:

```c
/* CRT / retro TV */
AP_SetPostScanlines(0.4f);
AP_SetPostCRT(0.5f);
AP_SetPostVHS(0.3f);
AP_SetPostRGBSplit(0.15f);

/* Toon */
AP_SetPostCelShade(0.7f);
AP_SetPostOutline(0.5f);
AP_SetPostPosterize(6.0f);

/* Damage / glitch state */
AP_SetPostGlitch(0.6f);
AP_SetPostDisplacement(0.3f);
AP_SetPostNoise(0.2f);

/* Distortion */
AP_SetPostWave(0.2f);
AP_SetPostFisheye(0.15f);
```

Combine sparingly — most of these read best at low amounts layered on top of the cinematic set, not as a replacement for it.

## 7. Master intensity and runtime toggling

`AP_SetPostIntensity` scales the *entire* stack, which is the easiest way to expose a single "Effects strength" slider in a settings screen, or to fade effects in/out over a cutscene:

```c
float t = AP_Clampf(elapsed / fade_duration, 0.0f, 1.0f);
AP_SetPostIntensity(t);
```

Toggle the whole system off for a screenshot mode or performance profiling pass:

```c
AP_SetPostEnabled(false);
```

## Next

[Advanced 3D: a PBR product scene](14-advanced-3d-scene.md) · [Capstone: a complete application](17-capstone-app.md)
