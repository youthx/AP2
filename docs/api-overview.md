# API overview

Copyright (c) 2024-2026 Jack Waechter. MIT licensed.

Include `<AP2/AP2.h>`. Draw with short names (`AP_Clear`, `AP_FillRect`, `AP_Present`).

| Header | Role |
|---|---|
| `AP2_Types.h` | `AP_Vec2`, `AP_FRect`, `AP_Color`, handles |
| `AP2_Init.h` | `AP_Init`, `AP_Quit`, subsystem flags |
| `AP2_Error.h` | `AP_GetErrorMessage`, `AP_SET_ERROR` |
| `AP2_Logger.h` | `AP_INFO`, `AP_WARN`, `AP_ERROR` |
| `AP2_Platform.h` | OS queries, paths, time helpers |
| `AP2_Math.h` | Vectors, matrices, quaternions (`AP_V3`, `AP_Mat4Mul`) |
| `AP2_Camera.h` | 2D / 3D camera, tracking, `AP_Begin2D` |
| `AP2_Window.h` | Create / size / fullscreen / `AP_PumpEvents` / `AP_Tick` / `AP_GetDeltaTime` |
| `AP2_Video.h` | Graphics API selection |
| `AP2_Device.h` | GPU device info |
| `AP2_Input.h` | Keys, mouse, gamepad, clipboard, file drop |
| `AP2_Renderer.h` | Immediate 2D; short macros live here |
| `AP2_Image.h` | CPU RGBA bitmaps: load, blit, filter, save |
| `AP2_Texture.h` | GPU upload / `AP_DrawTexture` / image conversion |
| `AP2_Sprite.h` | Atlas frames, animation, `AP_DrawSprite` |
| `AP2_Tilemap.h` | Tilesets, tile grids, CSV, `AP_DrawTilemap` |
| `AP2_Font.h` | TTF and `AP_DrawText` |
| `AP2_Shader.h` | GLSL, `AP_UseShader`, `AP_SetUniformF` |
| `AP2_Gui.h` | Immediate windows, menus, widgets |
| `AP2_3D.h` | Lights, meshes, `AP_DrawCube` |
| `AP2_Audio.h` | Sounds, voices, buses, listener |
| `AP2_Post.h` | Vignette, bloom, grade, grain |
| `AP2_List.h` / `AP2_String.h` | Small containers |
| `AP2_Opengl.h` | Opt-in native GL (`AP2_INCLUDE_OPENGL`) |

## Loop helpers

```c
AP_Init(AP_INIT_VIDEO | AP_INIT_AUDIO);
AP_CreateWindow("Title", 1280, 720, AP_WINDOW_RESIZABLE);
while (AP_IsRunning()) {
  AP_PumpEvents();
  AP_SetDrawColor(0.1f, 0.1f, 0.1f, 1.0f);
  AP_Clear();
  /* draw */
  AP_Present();
}
AP_DestroyWindow(NULL);
AP_Quit();
```

## Color

`AP_SetDrawColor(r, g, b, a)` takes floats in 0–1. `AP_C4(r, g, b, a)` builds an `AP_Color` for 3D and sprites.

## Next

Pick a tutorial from the [index](README.md).
