# Getting started

Copyright (c) 2026 Jack Waechter. MIT licensed.

AP2 (**Application Primitives**) is one header and a small C17 library. This page gets a window on screen.

## What you need

- CMake 3.15 or newer
- A C17 compiler (GCC, Clang, or MSVC)
- Python 3, once, so CMake can generate the GLAD OpenGL loader
- cforge is optional; plain CMake works

On Windows, MSYS2 UCRT (`C:/msys64/ucrt64/bin/gcc.exe`) matches `cforge.toml`.

## Build and run

From the repo root:

```bash
cforge build
cforge run
```

Or:

```bash
cmake -B build -G "Ninja Multi-Config"
cmake --build build --config Debug
./build/bin/Debug/ap2
```

The demo in `src/main.c` exercises 2D, GUI, 3D, audio, and post-process.

## A program of your own

Create a `.c` file and include the umbrella header:

```c
#include <AP2/AP2.h>

int main(void) {
  if (!AP_Init(AP_INIT_VIDEO)) {
    return 1;
  }

  if (!AP_CreateWindow("AP2", 1280, 720, AP_WINDOW_RESIZABLE)) {
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

    AP_SetDrawColor(0.95f, 0.35f, 0.35f, 1.0f);
    AP_FillRect(&(AP_FRect){80.0f, 80.0f, 240.0f, 140.0f});

    AP_Present();
  }

  AP_DestroyWindow(NULL);
  AP_Quit();
  return 0;
}
```

`AP_INIT_VIDEO` also brings up windowing. Add `| AP_INIT_AUDIO` when you need the mixer.

## Short draw names

Application code should use the short macros:

| You write | Underlying call |
|---|---|
| `AP_SetDrawColor` | `AP_SetRenderDrawColorFloat` |
| `AP_Clear` | `AP_RenderClear` |
| `AP_Present` | `AP_RenderPresent` |
| `AP_FillRect` | `AP_RenderFillRect` |
| `AP_FillCircleF` | `AP_RenderFillCircle` |
| `AP_DrawLineF` | `AP_RenderLine` |
| `AP_DrawTexture` | `AP_RenderTexture` |
| `AP_DrawSprite` | `AP_RenderSprite` |
| `AP_DrawText` | `AP_RenderText` |
| `AP_PushTransform` | `AP_PushRenderTransform` |

Both names work. Tutorials in this tree use the left column.

## Coordinates

- **2D / GUI / text:** top-left origin, Y down, units are window pixels.
- **3D / math / spatial audio:** right-handed, Y up (OpenGL world).

## Next

- [Architecture](architecture.md) — subsystems and modules
- [Hello window](tutorials/01-hello-window.md) — the loop in more detail
- [Drawing in 2D](tutorials/02-drawing-2d.md) — shapes and color
