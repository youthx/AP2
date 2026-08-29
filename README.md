# AP2 — Application Primitives

**A small C17 library of window, draw, input, audio, and GUI primitives for games and tools.**

Copyright (c) 2024-2026 [Jack Waechter](LICENSE). MIT licensed.

**AP2** stands for **Application Primitives**. Work started in 2024 under a previous name and was rebranded here. The kit is a set of primitives you assemble into games and desktop tools — window, immediate draw, input, mixer, text, GUI, plus an optional 3D pass.

```c
#include <AP2/AP2.h>

int main(void) {
  AP_Init(AP_INIT_VIDEO);
  AP_CreateWindow("Hello AP2", 1280, 720, AP_WINDOW_RESIZABLE);

  while (AP_IsRunning()) {
    AP_PumpEvents();
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

Prefer the short names (`AP_Clear`, `AP_FillRect`, `AP_Present`, `AP_DrawTexture`) in application code. They map onto the `AP_Render*` functions.

One include, opaque handles, no renderer pointer on every call. GLFW and OpenGL stay behind the public API.

## Features

| Area | What you get |
|---|---|
| Windowing | Resizable, HiDPI, MSAA, vsync, fullscreen |
| 2D | Rects, polygons, curves, gradients, textures, sprites |
| 3D | Cameras, lights, cubes / planes / spheres, custom meshes |
| Audio | WAV / FLAC / MP3, streams, buses, 3D spatialization, waveforms |
| Input | Keyboard, mouse, gamepads; down / pressed / released |
| GUI | Immediate windows, menus, sliders, text fields |
| Text | Built-in 8×8 font plus TrueType |
| Shaders & post | Custom GLSL, vignette, bloom, color grade, grain |
| Language | C17, `extern "C"` on every public header |

## Build

Needs CMake 3.15+, a C17 compiler, and Python once (to generate GLAD).

```bash
cforge build
cforge run
```

Or CMake:

```bash
cmake -B build -G "Ninja Multi-Config"
cmake --build build --config Debug
./build/bin/Debug/ap2
```

On Windows with MSYS2 UCRT, `cforge.toml` already points at `gcc` / `g++`.

| Dependency | Role |
|---|---|
| [GLFW](https://www.glfw.org/) 3.5 | Window and input |
| [GLAD](https://github.com/Dav1dde/glad) 2 | OpenGL 4.6 loader |
| [stb](https://github.com/nothings/stb) | Images and TrueType |
| [miniaudio](https://miniaud.io/) | Device, decode, mix |

## Documentation

Start with the [docs index](docs/README.md).

1. [Getting started](docs/getting-started.md)
2. [Architecture](docs/architecture.md)
3. [API overview](docs/api-overview.md)

| Tutorial | Subject |
|---|---|
| [01 Hello window](docs/tutorials/01-hello-window.md) | Init, loop, clear, present |
| [02 Drawing 2D](docs/tutorials/02-drawing-2d.md) | Color, shapes, transforms |
| [03 Sprites and textures](docs/tutorials/03-sprites-and-textures.md) | Images, atlases, animation |
| [04 Input](docs/tutorials/04-input.md) | Keys, mouse, gamepad |
| [05 Audio](docs/tutorials/05-audio.md) | Waves, buses, spatial SFX |
| [06 Text and fonts](docs/tutorials/06-text-and-fonts.md) | Built-in font and TTF |
| [07 Immediate GUI](docs/tutorials/07-immediate-gui.md) | Panels and widgets |
| [08 3D](docs/tutorials/08-3d.md) | Camera, lights, meshes |
| [09 Shaders and post](docs/tutorials/09-shaders-and-post.md) | GLSL and the post stack |
| [10 Breakout](docs/tutorials/10-breakout.md) | Game: bricks, paddle, score |
| [11 Top-down walker](docs/tutorials/11-top-down.md) | Game: WASD, camera, spatial audio |
| [12 Desktop tool](docs/tutorials/12-desktop-tool.md) | App: inspector-style GUI |

`src/main.c` is a living demo of renderer, GUI, 3D, audio, and post.

## Layout

```
include/AP2/     Public headers. Applications include <AP2/AP2.h>
src/core/        Library implementation
src/main.c       Demo executable
third_party/     stb, miniaudio (vendored)
docs/            Guides and tutorials
```

`AP2_Internal.h` is private. Do not include it from application code.

## License

MIT. See [LICENSE](LICENSE).

Copyright (c) 2024-2026 Jack Waechter.
