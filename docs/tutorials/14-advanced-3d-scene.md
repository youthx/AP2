# Tutorial 14 — Advanced 3D: A PBR Product Scene

Copyright (c) 2024-2026 Jack Waechter. MIT licensed.

This tutorial builds a complete "product photography" scene: load a textured glTF/GLB model (including multi-file models with an external `.bin` and a `textures/` folder), wire up a 3-point studio light rig, drive an orbit camera, patch an individual mesh's material at runtime, and finish with a light post-processing pass for realism.

It assumes you've read [3D](08-3d.md) and [Materials & Textures](../14-materials-textures.md).

## 1. Loading models

`AP_LoadModel` accepts three shapes of glTF content:

- A single `.glb` (binary, textures embedded as buffer views).
- A `.gltf` + `.bin` pair sitting next to each other.
- A `.gltf` whose `images[].uri` point into a subfolder (e.g. `textures/albedo.png`), which is how most asset-store exports and Sketchfab downloads are packaged.

```c
AP_Model *model = AP_LoadModel("assets/microphone.glb");
if (!AP_ModelIsValid(model)) {
  /* Falls back to a placeholder so the rest of the app can still run */
  model = NULL;
}
```

For a folder-based export, point at the `.gltf` inside the folder — the loader resolves sibling `.bin` and `textures/*` paths relative to it, normalizing `\` to `/` so exports made on Windows still load on other platforms:

```c
AP_Model *phone = AP_LoadModel("assets/telephone/scene.gltf");
```

### Inspecting what loaded

```c
int meshes = AP_ModelMeshCount(model);
int mats = AP_ModelGetMaterialCount(model);
int texs = AP_ModelGetTextureCount(model);
AP_INFO("Loaded %d meshes, %d materials, %d textures", meshes, mats, texs);

for (int i = 0; i < mats; ++i) {
  AP_Material *m = AP_ModelGetMaterial(model, i);
  AP_INFO("Material %d: %s metallic=%.2f roughness=%.2f",
          i, m->name ? m->name : "(unnamed)", m->metallic, m->roughness);
}
```

Every mesh keeps a pointer to its material (`AP_MeshGetMaterial`); the model owns all materials and textures and frees them in `AP_DestroyModel`.

## 2. Studio 3-point lighting

Flat, uniform ambient light kills depth. A classic 3-point rig — key, fill, rim — reads much better on metallic/rough PBR surfaces:

```c
AP_ClearLights();

/* Key: strong, warm, from front/upper-right */
AP_AddLight(AP_LightDirectional(AP_V3(-0.7f, -0.95f, -0.3f),
                                AP_C4(1.0f, 1.0f, 0.95f, 1.0f), 1.8f));

/* Fill: soft, cool, opposite side, so shadows aren't pure black */
AP_AddLight(AP_LightPoint(AP_V3(-4.0f, 1.5f, 1.0f),
                          AP_C4(0.5f, 0.5f, 0.6f, 1.0f), 0.7f, 12.0f));

/* Rim: from behind, separates the subject from the background */
AP_AddLight(AP_LightPoint(AP_V3(2.0f, 4.0f, -6.0f),
                          AP_C4(0.3f, 0.3f, 0.4f, 1.0f), 0.5f, 15.0f));

/* Low ambient keeps contrast; the PBR shader also adds a cheap
 * fresnel-weighted "fake IBL" term so metallic parts don't go flat. */
AP_SetAmbientLight(AP_C4(0.1f, 0.1f, 0.12f, 1.0f));
```

Directional `direction` points **toward** the subject (the light travels along `-direction`). Point lights fall off smoothly to zero at `range`.

## 3. Orbit camera

A mouse-driven orbit camera is the standard way to inspect a loaded model from every angle:

```c
static float yaw = 0.6f;
static float pitch = 0.35f;
static float distance = 6.0f;

if (!AP_GuiWantCaptureMouse() && AP_IsMouseDown(AP_MOUSE_RIGHT)) {
  yaw += (float)AP_GetMouseDeltaX() * 0.01f;
  pitch = AP_Clampf(pitch - (float)AP_GetMouseDeltaY() * 0.01f, -1.4f, 1.4f);
}
distance = AP_Clampf(distance - (float)AP_GetMouseWheelY() * 0.5f, 1.5f, 20.0f);

AP_Vec3 eye = AP_V3(
    sinf(yaw) * cosf(pitch) * distance,
    sinf(pitch) * distance + 1.0f,
    cosf(yaw) * cosf(pitch) * distance);

AP_Camera cam = AP_CameraPerspective(eye, AP_V3(0.0f, 0.5f, 0.0f), 50.0f);
```

For a first-person rig instead of orbit, see [Camera rigs and cinematic post](16-camera-rigs-and-post.md).

## 4. Draw and per-mesh overrides

```c
float dt = (float)AP_GetDeltaTime();

AP_Begin3D(&cam);
/* ... lights from step 2 ... */

if (AP_ModelIsValid(model)) {
  AP_ModelRotate(model, AP_V3(0.0f, 1.0f, 0.0f), 15.0f * dt); /* slow turntable */
  AP_DrawModel(model);
}

AP_DrawGrid3D(10.0f, 10, AP_C4(0.3f, 0.3f, 0.3f, 1.0f));
AP_End3D();
```

Sometimes you want to nudge a single mesh's look without touching the source asset — for example, making one part of a model shinier:

```c
AP_Mesh *grille = AP_ModelGetMesh(model, 15);
AP_Material *shared = AP_MeshGetMaterial(grille);

static AP_Material override;
override = *shared;             /* copy, don't mutate the shared material */
override.metallic = 0.9f;
override.roughness = 0.15f;
AP_MeshSetMaterial(grille, &override);
```

`AP_MeshSetMaterial` does **not** take ownership, so `override` must outlive the draw calls that reference it (a `static` or heap-allocated copy, not a stack temporary that goes out of scope).

## 5. A touch of post-processing

Real product shots aren't SDR-flat either. A restrained bloom + vignette + slight saturation boost goes a long way:

```c
AP_SetPostEnabled(true);
AP_SetPostVignette(0.25f);
AP_SetPostBloom(1.05f, 0.35f);       /* threshold, intensity */
AP_SetPostColorGrade(1.08f, 1.03f, 0.0f); /* saturation, contrast, brightness */
AP_SetPostExposure(1.1f);
AP_SetGuiLayer(AP_GUI_LAYER_OVERLAY); /* keep any debug UI crisp, unprocessed */
```

See [Camera rigs and cinematic post](16-camera-rigs-and-post.md) for the full effect catalog (depth of field, film grain, chromatic aberration, stylized looks).

## 6. Full frame

```c
AP_SetDrawColor(0.05f, 0.05f, 0.06f, 1.0f);
AP_Clear();

AP_Begin3D(&cam);
AP_ClearLights();
/* key / fill / rim / ambient as above */
AP_DrawModel(model);
AP_DrawGrid3D(10.0f, 10, AP_C4(0.3f, 0.3f, 0.3f, 1.0f));
AP_End3D();

AP_Present();
```

## Troubleshooting

- **Model loads but looks flat gray**: the PBR shader is likely starved of light — very low ambient plus a fully metallic/rough material has almost no diffuse term. Raise ambient slightly or reduce `metallic` on the mesh in question. See [PBR advanced](../16-pbr-advanced.md).
- **Folder-based model fails to find textures**: confirm the `.gltf`'s `images[].uri` values are relative to the `.gltf` file itself, not the working directory.
- **First run feels slow**: initial texture decode + GPU upload is one-time; subsequent frames are unaffected. Large (4K+) source textures dominate this cost.

## Next

[Camera rigs and cinematic post](16-camera-rigs-and-post.md) · [Retained-mode GUI](15-retained-gui.md)
