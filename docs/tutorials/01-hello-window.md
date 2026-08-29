# Tutorial 01 — Hello window

Copyright (c) 2024-2026 Jack Waechter. MIT licensed.

Open a window, clear it, swap. That is the whole AP2 loop.

## The loop

```c
#include <AP2/AP2.h>

int main(void) {
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
```

## What each call does

| Call | Role |
|---|---|
| `AP_Init(AP_INIT_VIDEO)` | Starts windowing and the graphics device |
| `AP_CreateWindow` | One window. Later draw/input calls use it |
| `AP_IsRunning` | False after the user hits the close button or `AP_RequestClose` |
| `AP_PumpEvents` | OS events, input, audio tick |
| `AP_SetDrawColor` | RGBA 0–1. `AP_Clear` uses this color |
| `AP_Clear` | Fill the color buffer |
| `AP_Present` | Swap (and run post-process if enabled) |
| `AP_DestroyWindow(NULL)` | Destroy the active window |
| `AP_Quit` | Shut down subsystems |

`AP_INIT_VIDEO` already includes windowing. Add `| AP_INIT_AUDIO` when you need sound.

## Window flags worth knowing

```c
AP_WINDOW_RESIZABLE | AP_WINDOW_HIGH_PIXEL_DENSITY | AP_WINDOW_MSAA | AP_WINDOW_VSYNC
```

`AP_WINDOW_FULLSCREEN` is exclusive fullscreen. Size is logical pixels; `AP_GetWindowSizeInPixels` is the framebuffer size on HiDPI displays.

## Errors

Failed init or window creation leaves a message in `AP_GetErrorMessage()`. Log it if you are wiring a real app:

```c
if (!AP_Init(AP_INIT_VIDEO)) {
  AP_ERROR("init: %s", AP_GetErrorMessage());
  return 1;
}
```

## Next

[Drawing in 2D](02-drawing-2d.md)
