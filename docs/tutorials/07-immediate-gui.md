# Tutorial 07 — Immediate GUI

Copyright (c) 2024-2026 Jack Waechter. MIT licensed.

Widgets are submitted every frame between `AP_Clear` and `AP_Present`. There is no retained tree. Layout is vertical unless you call `AP_GuiSameLine()`.

## Panel vs window

`AP_GuiBeginPanel` is a fixed rectangle. `AP_GuiBeginWindow` is movable/resizable like a desktop tool.

```c
AP_SetDrawColor(0.10f, 0.11f, 0.13f, 1.0f);
AP_Clear();

if (AP_GuiBeginPanel("Debug", 24.0f, 24.0f, 280.0f, 360.0f)) {
  AP_GuiLabel("AP2");
  AP_GuiSeparator();
  if (AP_GuiButton("Ping")) {
    /* clicked this frame */
  }
  AP_GuiEndPanel();
}

AP_Present();
```

## Inspector window

```c
static bool open = true;
static float volume = 0.8f;
static bool mute = false;
static int quality = 1;
const char *quality_items[] = {"Low", "Medium", "High"};

AP_GuiSetNextWindowPos(40.0f, 40.0f);
AP_GuiSetNextWindowSize(320.0f, 420.0f);

if (AP_GuiBeginWindow("Inspector", &open, AP_GUI_WINDOW_MENU_BAR)) {
  if (AP_GuiBeginMenuBar()) {
    if (AP_GuiBeginMenu("File")) {
      if (AP_GuiMenuItem("Quit", "Esc")) {
        AP_RequestClose();
      }
      AP_GuiEndMenu();
    }
    AP_GuiEndMenuBar();
  }

  AP_GuiCheckbox("Mute", &mute);
  AP_GuiSliderF("Volume", &volume, 0.0f, 1.0f);
  AP_GuiCombo("Quality", &quality, quality_items, 3);

  if (AP_GuiCollapsingHeader("Color", true)) {
    static AP_FColor tint = {1.0f, 1.0f, 1.0f, 1.0f};
    AP_GuiColorEdit("Tint", &tint);
  }

  AP_GuiEndWindow();
}
```

If `open` becomes false, skip drawing the window next frame (or set it true again from a menu).

## Capture

The GUI eats input when a widget is hovered or a text field is focused:

```c
if (!AP_GuiWantCaptureKeyboard() && AP_IsKeyPressed(AP_KEY_SPACE)) {
  /* game action */
}

if (!AP_GuiWantCaptureMouse() && AP_IsMousePressed(AP_MOUSE_LEFT)) {
  /* world click */
}
```

## Layer vs post

Default is overlay: UI is composited **after** post-process.

```c
AP_SetGuiLayer(AP_GUI_LAYER_OVERLAY); /* default */
AP_SetGuiLayer(AP_GUI_LAYER_SCENE);   /* UI sits in the scene and is processed */
AP_SetGuiLayer(AP_GUI_LAYER_OFF);     /* no draw, no capture */
```

## Style

```c
AP_GuiStyle style = AP_GuiDarkStyle();
style.accent = AP_C4(0.95f, 0.35f, 0.35f, 1.0f);
AP_GuiSetStyle(&style);
```

`AP_GuiLightStyle()` is the other preset. `AP_GuiSetFont(font)` uses a TrueType face for labels.

## IDs

Two buttons named `"OK"` collide. Disambiguate:

```c
AP_GuiButton("OK##save");
AP_GuiButton("OK##quit");
```

Or `AP_GuiPushId("row0")` / `AP_GuiPopId()`.

## Next

[3D](08-3d.md) · [Desktop tool](12-desktop-tool.md)
