# Architecture

Copyright (c) 2024-2026 Jack Waechter. MIT licensed.

AP2 (**Application Primitives**) is a few subsystems you turn on at `AP_Init`, then a set of headers you call from the game loop. There is no scene graph, no ECS, and no hidden renderer object.

## Subsystems

```c
AP_Init(AP_INIT_WINDOWING | AP_INIT_VIDEO | AP_INIT_AUDIO);
/* equivalent: AP_Init(AP_INIT_ALL); */
```

| Flag | Owns |
|---|---|
| `AP_INIT_WINDOWING` | GLFW, window list, event pump |
| `AP_INIT_VIDEO` | Graphics device (OpenGL). Implies windowing. |
| `AP_INIT_AUDIO` | Mixer and output device. Independent of video. |

`AP_Quit()` tears them down in reverse order. `AP_PumpEvents()` polls the OS and also ticks audio (one-shot reaping, ducking, muffling).

## Frame shape

```
AP_PumpEvents()     input + audio update
AP_Tick()           wait for target FPS, returns dt (seconds)
AP_Clear()          begin the color buffer (and post capture, if enabled)
  2D / 3D / text    scene
  AP_Gui*           widgets (overlay by default)
AP_Present()        post stack, swap
```

There is one active window. After `AP_CreateWindow`, draw and input calls target that window.

## Modules

`<AP2/AP2.h>` is an umbrella. Define exclude flags **before** the include:

```c
#define AP2_NO_3D
#define AP2_NO_AUDIO
#include <AP2/AP2.h>
```

| Flag | Drops |
|---|---|
| `AP2_ONLY_CORE` | Everything but types, error, logger, init |
| `AP2_NO_GRAPHICS` | Device, video, renderer, OpenGL, 3D |
| `AP2_NO_<MODULE>` | One header family (`AUDIO`, `GUI`, `POST`, …) |

After include, `AP2_HAS_AUDIO` (and friends) is `0` or `1`.

`AP2_Internal.h` is for `src/core` only.

## Handles

Textures, images, sounds, shaders, fonts, tilesets, tilemaps, and windows are opaque pointers. Destroy what you create. Voices from `AP_PlayOneShot` are reaped automatically.

## Backends

| Concern | Implementation |
|---|---|
| Window / input | GLFW 3.5 |
| GPU | OpenGL 4.6 via GLAD (3.3 core is the documented floor on some platforms) |
| Images / TTF | stb |
| Audio | miniaudio (WASAPI, CoreAudio, ALSA, …) |

Application code never includes GLFW or GLAD unless you opt in with `AP2_INCLUDE_OPENGL`.

## Identity

```c
AP2_NAME          /* "AP2" */
AP2_FULL_NAME     /* "Application Primitives" */
AP2_DESCRIPTION
AP2_AUTHOR        /* "Jack Waechter" */
AP2_LICENSE       /* "MIT" */
```

## Next

[API overview](api-overview.md) lists each public header.
