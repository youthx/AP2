# Tutorial 09 — Shaders and post-processing

Copyright (c) 2024-2026 Jack Waechter. MIT licensed.

Two related paths:

1. **Custom GLSL** on the immediate 2D pass (`AP_UseShader` then `AP_FillRect`).
2. **Post stack** that runs at `AP_Present` when enabled (`AP_SetPostEnabled`).

## Custom fragment on a quad

Pass `NULL` for the vertex stage to keep the builtin (window-pixel positions, `u_resolution`).

Immediate layout:

- `location 0` `vec2 a_position`
- `location 1` `vec4 a_color`
- `location 2` `vec2 a_uv`
- `uniform vec2 u_resolution`
- `uniform sampler2D u_texture` (white 1×1 when untextured)

```c
static const char *k_wave_fs =
    "#version 330 core\n"
    "in vec4 v_color;\n"
    "in vec2 v_uv;\n"
    "uniform sampler2D u_texture;\n"
    "uniform float u_time;\n"
    "out vec4 frag_color;\n"
    "void main() {\n"
    "  vec2 uv = v_uv;\n"
    "  uv.x += 0.04 * sin(uv.y * 24.0 + u_time * 3.0);\n"
    "  frag_color = v_color * texture(u_texture, uv);\n"
    "}\n";

AP_Shader *wave = AP_CreateShader(NULL, k_wave_fs);

/* in the loop, after AP_Clear: */
AP_UseShader(wave);
AP_SetUniformF("u_time", (float)AP_GetTime());
AP_SetDrawColor(1.0f, 1.0f, 1.0f, 1.0f);
AP_FillRect(&(AP_FRect){0.0f, 0.0f, 1280.0f, 720.0f});
AP_UseShader(NULL);
```

`AP_DestroyShader(wave)` at shutdown. `AP_SetUniformF2` / `F3` / `F4` cover vectors.

Load from disk with `AP_CreateShaderFromFile(vs_path, fs_path)`.

## Post stack

When post is on, `AP_Clear` captures offscreen. Effects run inside `AP_Present`.

```c
AP_SetPostEnabled(true);
AP_SetPostVignette(0.45f);
AP_SetPostBloom(0.7f, 0.35f);
AP_SetPostColorGrade(1.05f, 1.08f, 0.02f);
AP_SetPostGrain(0.08f);
AP_SetGuiLayer(AP_GUI_LAYER_OVERLAY); /* UI not processed */
```

The setters enable their flags when the amount is > 0. You can also:

```c
AP_EnablePostFlag(AP_POST_CHROMATIC);
AP_SetPostChromatic(0.004f);
```

Flags: vignette, bloom, color grade, chromatic, grain, sharpen, custom shader.

## Custom post shader

```c
AP_SetPostShader(my_fullscreen_shader);
AP_EnablePostFlag(AP_POST_CUSTOM);
```

GUI overlay vs scene:

```c
AP_SetPostIncludeGui(false); /* default: overlay after post */
```

## Order reminder

```
AP_PumpEvents
AP_Clear                 /* starts post capture if enabled */
  AP_Begin3D … AP_End3D
  2D scene (AP_FillRect, AP_DrawTexture, …)
  AP_Gui*                /* overlay unless you change the layer */
AP_Present               /* post, then swap */
```

## Next

Build something: [Breakout](10-breakout), [Top-down walker](11-top-down), or [Desktop tool](12-desktop-tool).
