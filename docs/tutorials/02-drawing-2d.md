# Tutorial 02 — Drawing in 2D

Copyright (c) 2026 Jack Waechter. MIT licensed.

2D is immediate: set color, issue a shape, next frame start over. Origin is the **top-left**. Y grows downward. Rotation is degrees, **clockwise**.

Prefer the short names (`AP_FillRect`, `AP_FillCircleF`, `AP_DrawLineF`).

## Color and clear

```c
AP_SetDrawColor(0.08f, 0.09f, 0.11f, 1.0f);
AP_Clear();
```

`AP_SetDrawColor8(r, g, b, a)` is the 0–255 variant.

## Rects, circles, lines

```c
AP_SetDrawColor(0.95f, 0.35f, 0.35f, 1.0f);
AP_FillRect(&(AP_FRect){80.0f, 80.0f, 240.0f, 140.0f});

AP_SetDrawColor(0.35f, 0.75f, 0.95f, 1.0f);
AP_FillRoundedRect(&(AP_FRect){360.0f, 80.0f, 200.0f, 140.0f}, 18.0f);

AP_SetDrawColor(0.95f, 0.85f, 0.35f, 1.0f);
AP_FillCircleF(200.0f, 360.0f, 48.0f);

AP_SetDrawColor(1.0f, 1.0f, 1.0f, 0.8f);
AP_SetLineWidth(2.0f);
AP_DrawLineF(80.0f, 480.0f, 560.0f, 520.0f);
```

`AP_FRect` is `{x, y, w, h}` in window pixels.

## Polygons and grids

```c
AP_SetDrawColor(0.55f, 0.85f, 0.45f, 1.0f);
AP_FillNGon(720.0f, 200.0f, 64.0f, 6);

AP_SetDrawColor(1.0f, 1.0f, 1.0f, 0.12f);
AP_DrawGrid(&(AP_FRect){0.0f, 0.0f, 1280.0f, 720.0f}, 16, 9);
```

Also useful: `AP_FillStar`, `AP_DrawCross`, `AP_DrawRing`, `AP_FillPieF`, `AP_FillTriangleF`.

## Gradients

```c
AP_FillRectGradient(
    &(AP_FRect){80.0f, 560.0f, 320.0f, 80.0f},
    AP_C4(0.2f, 0.3f, 0.8f, 1.0f),
    AP_C4(0.8f, 0.3f, 0.5f, 1.0f),
    AP_C4(0.9f, 0.6f, 0.2f, 1.0f),
    AP_C4(0.1f, 0.5f, 0.4f, 1.0f));
```

Corners are top-left, top-right, bottom-right, bottom-left.

## Transforms

Push a matrix, translate/rotate/scale, pop. Rotation is about the current origin unless you set `AP_SetRotationOrigin`.

```c
float t = (float)AP_GetTime();

AP_PushTransform();
AP_Translate(640.0f, 360.0f);
AP_SetRotation(t * 40.0f);
AP_SetDrawColor(0.9f, 0.4f, 0.2f, 1.0f);
AP_FillRect(&(AP_FRect){-40.0f, -40.0f, 80.0f, 80.0f});
AP_PopTransform();
```

`AP_ResetTransform()` clears the stack to identity.

## Clip and viewport

```c
AP_Rect clip = {100, 100, 400, 300};
AP_SetClipRect(&clip);
/* draws are clipped */
AP_SetClipRect(NULL); /* disable, if the implementation treats NULL as off */
```

`AP_SetViewport` limits where the 2D pass lands. Most games leave the default (full window).

## Blend

```c
AP_SetBlendMode(AP_BLEND_ADD);
AP_SetDrawColor(1.0f, 0.4f, 0.1f, 0.35f);
AP_FillCircleF(mx, my, 80.0f);
AP_SetBlendMode(AP_BLEND_ALPHA);
```

## A full sketch

```c
#include <AP2/AP2.h>
#include <math.h>

int main(void) {
  AP_Init(AP_INIT_VIDEO);
  AP_CreateWindow("2D", 1280, 720, AP_WINDOW_RESIZABLE);

  while (AP_IsRunning()) {
    AP_PumpEvents();
    if (AP_IsKeyPressed(AP_KEY_ESCAPE)) {
      AP_RequestClose();
    }

    float t = (float)AP_GetTime();
    AP_SetDrawColor(0.07f, 0.08f, 0.10f, 1.0f);
    AP_Clear();

    AP_SetDrawColor(1.0f, 1.0f, 1.0f, 0.08f);
    AP_DrawGrid(&(AP_FRect){0.0f, 0.0f, 1280.0f, 720.0f}, 20, 12);

    AP_SetDrawColor(0.95f, 0.35f, 0.40f, 1.0f);
    AP_FillRect(&(AP_FRect){48.0f, 48.0f, 180.0f, 110.0f});

    AP_SetDrawColor(0.30f, 0.75f, 0.90f, 1.0f);
    AP_FillCircleF(400.0f, 120.0f, 40.0f + 8.0f * sinf(t * 3.0f));

    AP_Present();
  }

  AP_DestroyWindow(NULL);
  AP_Quit();
  return 0;
}
```

## Next

[Textures and sprites](03-sprites-and-textures.md)
