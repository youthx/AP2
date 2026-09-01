# AP2 documentation

**AP2** (**Application Primitives**) is a C17 kit for games and tools.

Copyright (c) 2024-2026 Jack Waechter. MIT licensed. See [LICENSE](https://github.com/youthx/AP2/blob/HEAD/LICENSE).

Draw with the short names (`AP_Clear`, `AP_FillRect`, `AP_Present`, `AP_DrawTexture`). They are macros over `AP_Render*`.

## Guides

- [Getting started](getting-started) — install, build, first window
- [Architecture](architecture) — subsystems, modules, coordinate spaces
- [API overview](api-overview) — map of every public header

## Tutorials

### Subjects

1. [Hello window](01-hello-window)
2. [Drawing in 2D](02-drawing-2d)
3. [Textures and sprites](03-sprites-and-textures)
4. [Input](04-input)
5. [Audio](05-audio)
6. [Text and fonts](06-text-and-fonts)
7. [Immediate GUI](07-immediate-gui)
8. [3D](08-3d)
9. [Shaders and post-processing](09-shaders-and-post)

### Apps and games

10. [Breakout](10-breakout) — bricks, paddle, collision, score
11. [Top-down walker](11-top-down) — WASD/gamepad, camera, spatial SFX
12. [Desktop tool](12-desktop-tool) — inspector-style GUI app

Headers under `include/AP2/` win if a tutorial and a signature disagree.

The [GitHub Pages site](https://youthx.github.io/AP2/) and the [wiki](https://github.com/youthx/AP2/wiki) are published from this folder on each push.
