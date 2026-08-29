# Tutorial 12 — Desktop tool

Copyright (c) 2024-2026 Jack Waechter. MIT licensed.

AP2 is not only for games. This sketch is an inspector-style utility: a dark canvas, a live preview, and widgets that mutate it.

## Layout

- Full-window `AP_Clear` + preview shape
- A persistent `AP_GuiBeginWindow` on the left
- File menu that quits
- Sliders write into the same structs the preview reads

## Sketch

```c
#include <AP2/AP2.h>
#include <stdio.h>

int main(void) {
  AP_Init(AP_INIT_VIDEO);
  AP_CreateWindow("AP2 Inspector", 1280, 720,
                  AP_WINDOW_RESIZABLE | AP_WINDOW_VSYNC);

  AP_GuiStyle style = AP_GuiDarkStyle();
  style.accent = AP_C4(0.95f, 0.40f, 0.35f, 1.0f);
  AP_GuiSetStyle(&style);

  bool inspector_open = true;
  bool show_grid = true;
  bool post = false;
  float radius = 72.0f;
  int sides = 6;
  AP_FColor fill = {0.95f, 0.35f, 0.40f, 1.0f};
  AP_FColor bg = {0.10f, 0.11f, 0.13f, 1.0f};
  char label[64] = "Preview";
  float vignette = 0.35f;

  AP_GuiSetNextWindowPos(16.0f, 16.0f);
  AP_GuiSetNextWindowSize(300.0f, 520.0f);

  while (AP_IsRunning()) {
    AP_PumpEvents();
    if (!AP_GuiWantCaptureKeyboard() && AP_IsKeyPressed(AP_KEY_ESCAPE)) {
      AP_RequestClose();
    }

    AP_SetPostEnabled(post);
    if (post) {
      AP_SetPostVignette(vignette);
    }

    AP_SetDrawColor(bg.r, bg.g, bg.b, 1.0f);
    AP_Clear();

    int w = 1280;
    int h = 720;
    AP_GetWindowSize(&w, &h);

    if (show_grid) {
      AP_SetDrawColor(1.0f, 1.0f, 1.0f, 0.06f);
      AP_DrawGrid(&(AP_FRect){0.0f, 0.0f, (float)w, (float)h}, 16, 9);
    }

    AP_SetDrawColor(fill.r, fill.g, fill.b, fill.a);
    AP_FillNGon((float)w * 0.5f + 80.0f, (float)h * 0.5f, radius, sides);

    AP_SetDrawColor(1.0f, 1.0f, 1.0f, 0.9f);
    AP_DrawText((float)w * 0.5f, 48.0f, label);

    if (AP_GuiBeginWindow("Inspector", &inspector_open,
                          AP_GUI_WINDOW_MENU_BAR)) {
      if (AP_GuiBeginMenuBar()) {
        if (AP_GuiBeginMenu("File")) {
          if (AP_GuiMenuItem("Quit", "Esc")) {
            AP_RequestClose();
          }
          AP_GuiEndMenu();
        }
        AP_GuiEndMenuBar();
      }

      AP_GuiLabelF("%s  %dx%d", AP2_FULL_NAME, w, h);
      AP_GuiSeparator();

      AP_GuiCheckbox("Grid", &show_grid);
      AP_GuiCheckbox("Post vignette", &post);
      AP_GuiSliderF("Vignette", &vignette, 0.0f, 1.0f);
      AP_GuiSliderF("Radius", &radius, 16.0f, 240.0f);
      AP_GuiSliderI("Sides", &sides, 3, 12);
      AP_GuiColorEdit("Fill", &fill);
      AP_GuiColorEdit("Background", &bg);
      AP_GuiInputText("Label", label, (int)sizeof label);

      if (AP_GuiButton("Reset")) {
        radius = 72.0f;
        sides = 6;
        fill = AP_C4(0.95f, 0.35f, 0.40f, 1.0f);
      }

      AP_GuiEndWindow();
    }

    AP_Present();
  }

  AP_DestroyWindow(NULL);
  AP_Quit();
  return 0;
}
```

## Why this pattern works

The preview is ordinary 2D (`AP_Clear`, `AP_FillNGon`, `AP_DrawText`). The GUI is immediate, so every slider write is visible on the same frame. Post stays off the UI because the default layer is `AP_GUI_LAYER_OVERLAY`.

From here: load a texture with `AP_LoadTexture` and preview it with `AP_DrawTexture`, or add `AP_GuiBeginTabBar` for multiple tools. `src/main.c` in the repo is a larger version of the same idea.

## Index

Back to the [docs index](../README.md).
