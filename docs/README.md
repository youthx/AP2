# AP2 documentation

**AP2** (**Application Primitives**) is a C17 kit for games and tools.

Copyright (c) 2024-2026 Jack Waechter. MIT licensed. See [LICENSE](../LICENSE).

Draw with the short names (`AP_Clear`, `AP_FillRect`, `AP_Present`, `AP_DrawTexture`). They are macros over `AP_Render*`.

## Guides

- [Getting started](getting-started.md) — install, build, first window
- [Architecture](architecture.md) — subsystems, modules, coordinate spaces
- [API overview](api-overview.md) — map of every public header

## Tutorials

### Subjects

1. [Hello window](tutorials/01-hello-window.md)
2. [Drawing in 2D](tutorials/02-drawing-2d.md)
3. [Textures and sprites](tutorials/03-sprites-and-textures.md)
4. [Input](tutorials/04-input.md)
5. [Audio](tutorials/05-audio.md)
6. [Text and fonts](tutorials/06-text-and-fonts.md)
7. [Immediate GUI](tutorials/07-immediate-gui.md)
8. [3D](tutorials/08-3d.md)
9. [Shaders and post-processing](tutorials/09-shaders-and-post.md)

### Apps and games

10. [Breakout](tutorials/10-breakout.md) — bricks, paddle, collision, score
11. [Top-down walker](tutorials/11-top-down.md) — WASD/gamepad, camera, spatial SFX
12. [Desktop tool](tutorials/12-desktop-tool.md) — inspector-style GUI app

Headers under `include/AP2/` win if a tutorial and a signature disagree.
