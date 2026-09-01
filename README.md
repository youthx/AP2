# AP2

AP2 is a work‑in‑progress graphics engine written in C17. I develop it in my spare time, and the goal is to build a fully supported engine with a clear, consistent API and a straightforward codebase. It’s still early in development, but the core systems are mainly functional and the project is actively growing.

---

## Overview

AP2 currently includes:

- Window creation and event handling
- 2D rendering (rectangles, lines, textures, sprites)
- 3D rendering (camera, meshes, lights, custom shaders)
- Keyboard, mouse, and gamepad input
- Audio/Mixer
- Text rendering (bitmap font + TTF)
- Immediate‑mode UI
- (in-dev) Further advanced UI support with customizable styling for desktop applications
- A post-processing pipeline

The engine is designed to stay readable and approachable. Most of the API uses simple structs and `AP_*` functions, and the internal layout is meant to be easy to follow for contributors.

---

## Guides

- [Getting started](docs/getting-started.md) — install, build, first window
- [Architecture](docs/architecture.md) — subsystems, modules, coordinate spaces
- [API overview](docs/api-overview.md) — map of every public header
- [Materials and textures](docs/14-materials-textures.md) — PBR, specular-glossiness, unlit, custom
- [glTF models](docs/15-gltf-models.md) — loading `.glb` / `.gltf` (single-file and folder-based)
- [Advanced PBR](docs/16-pbr-advanced.md) — Cook-Torrance BRDF, normal/metallic-roughness/emissive maps
- [Best practices](docs/17-best-practices.md) — code organization, memory, performance, debugging
- [Material & 3D API reference](docs/18-api-reference.md) — full signatures with examples

## Tutorials

### Subjects

1. [Hello window](docs/tutorials/01-hello-window.md)
2. [Drawing in 2D](docs/tutorials/02-drawing-2d.md)
3. [Textures and sprites](docs/tutorials/03-sprites-and-textures.md)
4. [Input](docs/tutorials/04-input.md)
5. [Audio](docs/tutorials/05-audio.md)
6. [Text and fonts](docs/tutorials/06-text-and-fonts.md)
7. [Immediate GUI](docs/tutorials/07-immediate-gui.md)
8. [3D](docs/tutorials/08-3d.md)
9. [Shaders and post-processing](docs/tutorials/09-shaders-and-post.md)
13. [Tilemaps](docs/tutorials/13-tilemaps.md)

### Apps and games

10. [Breakout](docs/tutorials/10-breakout.md) — bricks, paddle, collision, score
11. [Top-down walker](docs/tutorials/11-top-down.md) — WASD/gamepad, camera, spatial SFX
12. [Desktop tool](docs/tutorials/12-desktop-tool.md) — inspector-style GUI app

### Advanced, step-by-step

14. [3D: a PBR product scene](docs/tutorials/14-advanced-3d-scene.md) — glTF/GLB loading (incl. folder-based models), studio lighting, orbit camera, per-mesh material overrides, post-processing
15. [Retained-mode GUI](docs/tutorials/15-retained-gui.md) — `AP2_GuiAdvanced`: windows, layouts, theming, signals, CSS-like styling, embedded canvas widgets
16. [Camera rigs and cinematic post](docs/tutorials/16-camera-rigs-and-post.md) — first-person/orbit rigs, screen-space picking, the full post-processing effect catalog
17. [Capstone: a complete application](docs/tutorials/17-capstone-app.md) — 3D + tilemap HUD + retained GUI + spatial audio + post, wired into one app

Headers under `include/AP2/` win if a tutorial and a signature disagree.

The [GitHub Pages site](https://youthx.github.io/AP2/) and the [wiki](https://github.com/youthx/AP2/wiki) are published from this folder on each push.

---

## Building

AP2 supports both CMake and cforge.

### CMake

cmake -B build cmake --build build ./build/bin/ap2


### cforge

cforge build cforge run


Dependencies are included in the repository and handled automatically.

---

## Current State

AP2 is usable for small projects, experiments, and learning. The engine is still under active development, and systems will continue to evolve. Expect breaking changes as things improve.

---

## Planned Improvements

Some of the work planned for future versions:

- More complete text rendering
- Additional post‑processing effects
- Improved material/shader system
- More examples and documentation
- Expansion of 3D features
- General engine tooling and workflow improvements

If you have ideas or find issues, feel free to open them.

---

## Contributing

Contributions are welcome. The project is still growing, so even small improvements help — bug fixes, cleanup, documentation, examples, or focused feature work.

If you’re interested in contributing:

- Open an issue to discuss changes
- Submit a PR when ready
- Keep things simple and consistent with the existing style

I review and merge changes when I have time.

---

## License

MIT License.
