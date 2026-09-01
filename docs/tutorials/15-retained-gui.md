# Tutorial 15 — Advanced GUI: A Retained-Mode Desktop App

Copyright (c) 2024-2026 Jack Waechter. MIT licensed.

[Immediate GUI](07-immediate-gui.md) is great for debug panels and in-game HUDs — you submit widgets every frame and there's no tree to manage. `AP2_GuiAdvanced.h` is the other end of the spectrum: a **retained-mode** widget hierarchy (think Qt) for building actual desktop tools, asset browsers, level editors, settings dialogs, all with persistent layout, theming, and a signal/event system.

Exclude both with `AP2_NO_GUI_ADVANCED`.

## 1. A window and a layout

Every retained widget is an `AP_GuiWidget *`. A window is the root; children are added with `AP_GuiWidgetAddChild`:

```c
AP_GuiWidget *window = AP_GuiWindowNew("Asset Inspector", 420.0f, 560.0f);

AP_GuiWidget *root = AP_GuiVBoxNew();
AP_GuiWidgetAddChild(window, root);
AP_GuiLayoutSetSpacing(AP_GuiWidgetLayout(root), 8.0f);
AP_GuiLayoutSetMargins(AP_GuiWidgetLayout(root),
                       (AP_GuiMargins){12.0f, 12.0f, 12.0f, 12.0f});
```

`AP_GuiVBoxNew()` / `AP_GuiHBoxNew()` stack children vertically or horizontally. `AP_GuiGridNew(rows, cols)` and `AP_GuiFlexBoxNew()` cover table and flex layouts respectively.

## 2. Widgets

```c
AP_GuiWidget *title = AP_GuiLabelNew("Selected mesh");
AP_GuiWidgetAddChild(root, title);

AP_GuiWidget *metallic_slider = AP_GuiSliderNew(0.0f, 1.0f, 0.01f);
AP_GuiWidgetAddChild(root, metallic_slider);

AP_GuiWidget *roughness_slider = AP_GuiSliderNew(0.0f, 1.0f, 0.01f);
AP_GuiWidgetAddChild(root, roughness_slider);

AP_GuiWidget *wireframe = AP_GuiCheckboxNew("Wireframe overlay");
AP_GuiWidgetAddChild(root, wireframe);

AP_GuiWidget *quality = AP_GuiComboBoxNew();
AP_GuiWidgetAddChild(root, quality);

AP_GuiWidget *apply = AP_GuiButtonNew("Apply to material");
AP_GuiWidgetAddChild(root, apply);
```

`AP_GuiButtonNew`, `AP_GuiTextEditNew`, `AP_GuiRadioNew`, `AP_GuiSpinnerNew`, `AP_GuiProgressBarNew`, `AP_GuiListBoxNew`, `AP_GuiTreeWidgetNew`, `AP_GuiTabWidgetNew`, and `AP_GuiImageNew(texture)` round out the widget set. `AP_GuiSpacerNew` / `AP_GuiSeparatorNew` control whitespace between groups.

## 3. Sizing and geometry

Widgets follow a size-policy model similar to Qt's:

```c
AP_GuiWidgetSetSizePolicy(metallic_slider, AP_GUI_SIZE_EXPANDING,
                         AP_GUI_SIZE_FIXED);
AP_GuiWidgetSetMinSize(apply, 0.0f, 32.0f);
```

`AP_GUI_SIZE_EXPANDING` widgets grow to fill available space; `AP_GUI_SIZE_FIXED` respects the widget's natural or explicit size (`AP_GuiWidgetSetSize`).

## 4. Theming

```c
AP_GuiTheme *dark = AP_GuiThemeDarkNew();
dark->colors.accent = (AP_FColor){0.98f, 0.42f, 0.15f, 1.0f};
dark->rounding = 6.0f;
AP_GuiSetGlobalTheme(dark);
```

Per-widget overrides win over the theme without cloning it:

```c
AP_GuiWidgetSetBackgroundColor(apply, (AP_FColor){0.98f, 0.42f, 0.15f, 1.0f});
AP_GuiWidgetSetCornerRadius(apply, 6.0f);
```

Use `AP_GuiThemeClone` when a whole sub-tree needs a variant theme instead of one-off property overrides.

## 5. Events and signals

Widgets emit named signals (`"clicked"`, `"valueChanged"`, `"textChanged"`, `"stateChanged"`, ...). Connect a callback once at setup time:

```c
typedef struct AppState {
  AP_Material *target;
} AppState;

static bool on_apply_clicked(AP_GuiEvent *event) {
  AppState *state = (AppState *)event->user_data;
  if (state->target != NULL) {
    /* pull the latest slider values and push them onto the material */
    state->target->metallic = AP_GuiWidgetValue(event->target /* not used here, illustrative */);
  }
  return true; /* accept the event */
}

AppState app = {0};
AP_GuiWidgetConnect(apply, "clicked", on_apply_clicked, &app);
```

Sliders and checkboxes report changes the same way:

```c
static bool on_metallic_changed(AP_GuiEvent *event) {
  AppState *state = (AppState *)event->user_data;
  if (state->target != NULL) {
    state->target->metallic = event->value;
  }
  return true;
}
AP_GuiWidgetConnect(metallic_slider, "valueChanged", on_metallic_changed, &app);
```

Widgets can also emit events programmatically, useful for driving one control from another:

```c
AP_GuiWidgetEmitWithValue(metallic_slider, "valueChanged", 0.8f);
```

## 6. CSS-like styling

Beyond theme colors, widgets support a Tailwind-flavored utility API for layout-heavy UIs:

```c
AP_GuiWidgetSetDisplay(root, AP_GUI_DISPLAY_FLEX);
AP_GuiWidgetSetFlexDirection(root, AP_GUI_FLEX_COLUMN);
AP_GuiWidgetSetFlexGap(root, 8.0f);
AP_GuiWidgetSetJustifyContent(root, AP_GUI_JUSTIFY_START);
AP_GuiWidgetSetAlignItems(root, AP_GUI_ALIGN_ITEMS_STRETCH);

AP_GuiWidgetSetPaddingAll(apply, 10.0f);
AP_GuiWidgetSetBorderRadius(apply, 6.0f);

AP_GuiWidgetAddClass(apply, "primary-action");
```

`AP_GuiApplyStyleString(widget, "p-4 rounded-lg bg-accent")` applies a whole batch of utility classes from a single string when you'd rather describe a widget's look in one line than call ten setters.

## 7. A canvas widget for custom drawing

`AP_GuiCanvasNew` embeds custom 2D or 3D rendering inside the retained tree — handy for a material preview thumbnail sitting next to the sliders above:

```c
AP_GuiWidget *preview = AP_GuiCanvasNew(180.0f, 180.0f);
AP_GuiWidgetAddChild(root, preview);
AP_GuiCanvasSetRenderMode(preview, AP_GUI_CANVAS_3D);

static void render_preview(AP_GuiWidget *canvas, AP_FRect bounds, void *user_data) {
  (void)bounds;
  AppState *state = (AppState *)user_data;

  AP_GuiCanvasSetup3D(canvas);
  AP_Camera cam = AP_CameraPerspective(AP_V3(0, 1.5f, 3.0f), AP_V3(0, 0, 0), 45.0f);
  AP_Begin3D(&cam);
  AP_ClearLights();
  AP_AddLight(AP_LightDirectional(AP_V3(-0.5f, -1.0f, -0.3f),
                                  AP_C4(1, 1, 1, 1), 1.5f));
  AP_DrawSphere(AP_V3(0, 0, 0), 1.0f, AP_C4(0.8f, 0.8f, 0.85f, 1.0f));
  AP_End3D();
  AP_GuiCanvasEnd3D(canvas);
  (void)state;
}

AP_GuiCanvasSetRenderCallback(preview, render_preview, &app);
```

2D canvases skip `AP_GuiCanvasSetup3D`/`End3D` and instead use `AP_GuiCanvasDrawRect`, `AP_GuiCanvasDrawCircle`, `AP_GuiCanvasDrawLine`, `AP_GuiCanvasDrawText`, and `AP_GuiCanvasDrawImage` directly inside the callback.

## 8. The frame loop

Retained widgets update and render themselves once per frame, driven from the root:

```c
while (AP_IsRunning()) {
  float dt = (float)AP_GetDeltaTime();

  AP_PumpEvents();
  AP_GuiProcessEvents(); /* dispatch queued signals from this frame's input */

  AP_SetDrawColor(0.08f, 0.08f, 0.09f, 1.0f);
  AP_Clear();

  AP_GuiWidgetUpdate(window, dt);   /* recompute layout, animations, focus */
  AP_GuiWidgetRender(window);       /* draw the whole tree */

  AP_Present();
}

AP_GuiWidgetDestroy(window); /* recursively destroys all children */
```

## 9. Finding widgets and debugging layout

```c
AP_GuiWidget *found = AP_GuiFindWidget(window, "apply-button");
AP_GuiWidgetPrintHierarchy(window, 0); /* dumps the tree to the log */
```

Name widgets with `AP_GuiWidgetSetName` right after creation if you plan to look them up later instead of holding onto every pointer.

## Troubleshooting

- **Layout doesn't update after adding a child at runtime**: call `AP_GuiWidgetLayout(window)` to force an immediate recalculation instead of waiting for the next `AP_GuiWidgetUpdate`.
- **Signal callback never fires**: double check the signal name string (`"clicked"`, `"valueChanged"`, ...) — there is no compile-time checking, a typo silently no-ops.
- **A widget outlives its `user_data` pointer**: `AP_GuiWidgetConnect` stores the pointer as-is; if `user_data` is stack-allocated and its scope ends before the widget is destroyed, disconnect first with `AP_GuiWidgetDisconnect` or make the state heap/`static`-allocated.

## Next

[Advanced 3D: a PBR product scene](14-advanced-3d-scene.md) · [Capstone: a complete application](17-capstone-app.md)
