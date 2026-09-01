# AP2 documentation

**AP2** (**Application Primitives**) is a C17 kit for games and tools.

Copyright (c) 2024-2026 Jack Waechter. MIT licensed. See [LICENSE](../LICENSE).

Draw with the short names (`AP_Clear`, `AP_FillRect`, `AP_Present`, `AP_DrawTexture`). They are macros over `AP_Render*`.

## Guides

- [Getting started](getting-started.md) — install, build, first window
- [Architecture](architecture.md) — subsystems, modules, coordinate spaces
- [API overview](api-overview.md) — map of every public header
- [Materials and textures](14-materials-textures.md) — PBR, specular-glossiness, unlit, custom
- [glTF models](15-gltf-models.md) — loading `.glb` / `.gltf` (single-file and folder-based)
- [Advanced PBR](16-pbr-advanced.md) — Cook-Torrance BRDF, normal/metallic-roughness/emissive maps
- [Best practices](17-best-practices.md) — code organization, memory, performance, debugging
- [Material & 3D API reference](18-api-reference.md) — full signatures with examples

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
13. [Tilemaps](tutorials/13-tilemaps.md)

### Apps and games

10. [Breakout](tutorials/10-breakout.md) — bricks, paddle, collision, score
11. [Top-down walker](tutorials/11-top-down.md) — WASD/gamepad, camera, spatial SFX
12. [Desktop tool](tutorials/12-desktop-tool.md) — inspector-style GUI app

### Advanced, step-by-step

14. [3D: a PBR product scene](tutorials/14-advanced-3d-scene.md) — glTF/GLB loading (incl. folder-based models), studio lighting, orbit camera, per-mesh material overrides, post-processing
15. [Retained-mode GUI](tutorials/15-retained-gui.md) — `AP2_GuiAdvanced`: windows, layouts, theming, signals, CSS-like styling, embedded canvas widgets
16. [Camera rigs and cinematic post](tutorials/16-camera-rigs-and-post.md) — first-person/orbit rigs, screen-space picking, the full post-processing effect catalog
17. [Capstone: a complete application](tutorials/17-capstone-app.md) — 3D + tilemap HUD + retained GUI + spatial audio + post, wired into one app

Headers under `include/AP2/` win if a tutorial and a signature disagree.

The [GitHub Pages site](https://youthx.github.io/AP2/) and the [wiki](https://github.com/youthx/AP2/wiki) are published from this folder on each push.
