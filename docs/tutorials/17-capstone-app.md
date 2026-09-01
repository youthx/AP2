# Tutorial 17 — Capstone: A Complete Application

Copyright (c) 2024-2026 Jack Waechter. MIT licensed.

This tutorial doesn't introduce new API — it wires together everything from the rest of the tutorial series into one small but complete desktop application: a 3D model viewer with a 2D minimap, a retained-mode inspector panel, spatial audio feedback, and a cinematic post pass, all driven from a single frame loop.

Read first: [3D](08-3d.md), [Tilemaps](13-tilemaps.md), [Audio](05-audio.md), [Advanced 3D](14-advanced-3d-scene.md), [Retained-mode GUI](15-retained-gui.md), [Camera rigs and post](16-camera-rigs-and-post.md).

## 1. What we're building

- A textured glTF model in the center of the screen, orbit-controlled.
- A small top-down tilemap in the corner acting as a "level map" HUD, panned independently of the 3D camera.
- A retained-mode side panel with sliders for the model's material and the post-processing look.
- A click sound and a spatial "ping" when you click on the model.
- Bloom/vignette/color-grade tying the look together.

## 2. Application state

Keep everything the frame loop touches in one struct — it's what gets passed as `user_data` to every GUI callback:

```c
typedef struct App {
  AP_Model *model;
  AP_Material *model_material; /* first material, patched by the GUI */

  AP_Tileset *minimap_tileset;
  AP_Tilemap *minimap;

  AP_Sound *click_sound;

  AP_Camera cam;
  float orbit_yaw, orbit_pitch, orbit_dist;

  AP_GuiWidget *window;
  AP_GuiWidget *metallic_slider;
  AP_GuiWidget *roughness_slider;
  AP_GuiWidget *bloom_slider;
} App;
```

## 3. Setup

```c
App app = {0};

AP_Init(AP_INIT_VIDEO | AP_INIT_AUDIO);
AP_CreateWindow("Model Viewer", 1280, 800, 0);

/* 3D content */
app.model = AP_LoadModel("assets/microphone.glb");
if (AP_ModelIsValid(app.model) && AP_ModelGetMaterialCount(app.model) > 0) {
  app.model_material = AP_ModelGetMaterial(app.model, 0);
}
app.orbit_yaw = 0.6f;
app.orbit_pitch = 0.3f;
app.orbit_dist = 6.0f;

/* Minimap */
AP_Texture *atlas = AP_LoadTexture("assets/tiles.png");
app.minimap_tileset = AP_CreateTileset(atlas, 16, 16);
static const int layout[] = {
    1, 1, 1, 1, 1, 1,
    1, 0, 0, 0, 0, 1,
    1, 0, 2, 0, 0, 1,
    1, 1, 1, 1, 1, 1,
};
app.minimap = AP_CreateTilemapFrom(app.minimap_tileset, 6, 4, layout);

/* Audio */
app.click_sound = AP_CreateSoundWave(AP_WAVEFORM_SINE, 660.0f, 0.08f, 0.3f);

/* Post-processing look */
AP_SetPostEnabled(true);
AP_SetPostVignette(0.3f);
AP_SetPostBloom(1.1f, 0.35f);
AP_SetGuiLayer(AP_GUI_LAYER_OVERLAY);
```

## 4. The inspector panel

```c
static bool on_metallic_changed(AP_GuiEvent *event) {
  App *app = (App *)event->user_data;
  if (app->model_material != NULL) {
    app->model_material->metallic = event->value;
  }
  return true;
}

static bool on_roughness_changed(AP_GuiEvent *event) {
  App *app = (App *)event->user_data;
  if (app->model_material != NULL) {
    app->model_material->roughness = event->value;
  }
  return true;
}

static bool on_bloom_changed(AP_GuiEvent *event) {
  App *app = (App *)event->user_data;
  (void)app;
  AP_SetPostBloom(1.1f, event->value);
  return true;
}

app.window = AP_GuiWindowNew("Inspector", 300.0f, 260.0f);
AP_GuiWidgetSetPos(app.window, 960.0f, 20.0f);

AP_GuiWidget *root = AP_GuiVBoxNew();
AP_GuiWidgetAddChild(app.window, root);
AP_GuiLayoutSetSpacing(AP_GuiWidgetLayout(root), 8.0f);
AP_GuiLayoutSetMargins(AP_GuiWidgetLayout(root),
                       (AP_GuiMargins){12.0f, 12.0f, 12.0f, 12.0f});

AP_GuiWidgetAddChild(root, AP_GuiLabelNew("Material"));

app.metallic_slider = AP_GuiSliderNew(0.0f, 1.0f, 0.01f);
AP_GuiWidgetAddChild(root, app.metallic_slider);
AP_GuiWidgetConnect(app.metallic_slider, "valueChanged", on_metallic_changed, &app);

app.roughness_slider = AP_GuiSliderNew(0.0f, 1.0f, 0.01f);
AP_GuiWidgetAddChild(root, app.roughness_slider);
AP_GuiWidgetConnect(app.roughness_slider, "valueChanged", on_roughness_changed, &app);

AP_GuiWidgetAddChild(root, AP_GuiSeparatorNew(false));
AP_GuiWidgetAddChild(root, AP_GuiLabelNew("Bloom"));

app.bloom_slider = AP_GuiSliderNew(0.0f, 1.0f, 0.01f);
AP_GuiWidgetSetValue(app.bloom_slider, 0.35f);
AP_GuiWidgetAddChild(root, app.bloom_slider);
AP_GuiWidgetConnect(app.bloom_slider, "valueChanged", on_bloom_changed, &app);
```

## 5. Picking the model with a click, and a spatial ping

```c
if (!AP_GuiWantCaptureMouse() && AP_IsMousePressed(AP_MOUSE_LEFT)) {
  double mx, my;
  AP_GetMousePosition(&mx, &my);
  float aspect = (float)AP_GetWindowWidth() / (float)AP_GetWindowHeight();
  AP_Vec2 screen01 = AP_V2((float)mx / (float)AP_GetWindowWidth(),
                          (float)my / (float)AP_GetWindowHeight());
  AP_Ray ray = AP_CameraScreenRay(&app.cam, aspect, screen01);

  /* Simplified hit test: sphere around the model's origin */
  if (AP_ModelIsValid(app.model) /* && ray intersects bounding sphere */) {
    AP_PlaySoundDesc desc = AP_PlaySoundDescDefault();
    desc.spatial = true;
    desc.position = AP_V3(0.0f, 0.5f, 0.0f);
    desc.min_distance = 2.0f;
    desc.max_distance = 20.0f;
    AP_PlayOneShotEx(app.click_sound, &desc);
  }
  (void)ray;
}
```

## 6. The frame loop

```c
double last_time = AP_GetTime();

while (AP_IsRunning()) {
  double now = AP_GetTime();
  float dt = (float)(now - last_time);
  last_time = now;

  AP_PumpEvents();
  AP_GuiProcessEvents();

  /* Orbit camera, ignored while dragging over the GUI */
  if (!AP_GuiWantCaptureMouse() && AP_IsMouseDown(AP_MOUSE_RIGHT)) {
    app.orbit_yaw += (float)AP_GetMouseDeltaX() * 0.01f;
    app.orbit_pitch = AP_Clampf(
        app.orbit_pitch - (float)AP_GetMouseDeltaY() * 0.01f, -1.4f, 1.4f);
  }
  app.orbit_dist = AP_Clampf(
      app.orbit_dist - (float)AP_GetMouseWheelY() * 0.5f, 1.5f, 20.0f);

  AP_Vec3 eye = AP_V3(
      sinf(app.orbit_yaw) * cosf(app.orbit_pitch) * app.orbit_dist,
      sinf(app.orbit_pitch) * app.orbit_dist + 1.0f,
      cosf(app.orbit_yaw) * cosf(app.orbit_pitch) * app.orbit_dist);
  app.cam = AP_CameraPerspective(eye, AP_V3(0.0f, 0.5f, 0.0f), 50.0f);

  /* Listener follows the 3D camera so the click ping pans correctly */
  AP_SetListenerPosition2D(eye.x, eye.z);

  AP_SetDrawColor(0.05f, 0.05f, 0.06f, 1.0f);
  AP_Clear();

  /* --- 3D pass --- */
  AP_Begin3D(&app.cam);
  AP_ClearLights();
  AP_AddLight(AP_LightDirectional(AP_V3(-0.7f, -0.95f, -0.3f),
                                  AP_C4(1.0f, 1.0f, 0.95f, 1.0f), 1.8f));
  AP_AddLight(AP_LightPoint(AP_V3(-4.0f, 1.5f, 1.0f),
                            AP_C4(0.5f, 0.5f, 0.6f, 1.0f), 0.7f, 12.0f));
  AP_SetAmbientLight(AP_C4(0.1f, 0.1f, 0.12f, 1.0f));

  if (AP_ModelIsValid(app.model)) {
    AP_DrawModel(app.model);
  }
  AP_DrawGrid3D(10.0f, 10, AP_C4(0.3f, 0.3f, 0.3f, 1.0f));
  AP_End3D();

  /* --- 2D minimap HUD, top-left corner, fixed --- */
  AP_ResetTransform();
  AP_DrawTilemap(app.minimap, 20.0f, 20.0f);

  /* --- Retained GUI panel --- */
  AP_GuiWidgetUpdate(app.window, dt);
  AP_GuiWidgetRender(app.window);

  AP_Present();
}
```

## 7. Shutdown

Destroy in the reverse order of ownership: retained widgets first (they don't own materials or the model), then the tilemap/tileset/texture chain, then the model and sounds:

```c
AP_GuiWidgetDestroy(app.window);

AP_DestroyTilemap(app.minimap);
AP_DestroyTileset(app.minimap_tileset);

if (AP_ModelIsValid(app.model)) {
  AP_DestroyModel(app.model);
}
AP_DestroySound(app.click_sound);

AP_DestroyWindow(NULL); /* pass the window handle from AP_CreateWindow */
AP_Quit();
```

## Where to go from here

- Swap the orbit rig for the first-person rig in [Camera rigs and cinematic post](16-camera-rigs-and-post.md) and turn this into a walkable scene.
- Replace the sine-wave click sound with a loaded `.wav` and add a looping ambient bed on the Music bus, per [Audio](05-audio.md).
- Promote the minimap to a full level using `AP_LoadTilemapCSV` and a Tiled export, per [Tilemaps](13-tilemaps.md).
- Add a `AP_GuiTabWidgetNew` to the inspector with a second tab for light settings, using the same signal pattern as the material sliders.
- Save/load the current post-processing look as an `AP_PostConfig` preset file.

## Next

You've now touched every major system in AP2. From here, [Best practices](../17-best-practices.md) and the [API reference](../18-api-reference.md) are the places to go for depth on any one piece.
