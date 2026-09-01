# 3D Models and glTF Loading

## Overview

AP2 supports loading and manipulating 3D models in glTF 2.0 format (both `.glTF` and `.glb` files). This guide covers model loading, material extraction, and advanced manipulation techniques.

## Table of Contents

1. [glTF Basics](#gltf-basics)
2. [Loading Models](#loading-models)
3. [Accessing Model Data](#accessing-model-data)
4. [Material Extraction](#material-extraction)
5. [Transform Control](#transform-control)
6. [Advanced Techniques](#advanced-techniques)
7. [Performance Considerations](#performance-considerations)

## glTF Basics

### File Formats

**glTF (.gltf)**: Text-based format with separate binary and texture files
```
model.gltf           (JSON structure)
model.bin            (Binary mesh and animation data)
textures/
  diffuse.png
  normal.png
```

**glB (.glb)**: Binary format with everything embedded (recommended)
```
model.glb            (Everything in one file)
```

**Benefits of glB:**
- Single file distribution
- Textures embedded or efficiently referenced
- Faster loading
- Better for asset pipelines

### Supported Features

AP2's glTF loader supports:
- ✅ Multiple meshes per model
- ✅ Multiple materials per mesh
- ✅ PBR Metallic-Roughness workflow
- ✅ PBR Specular-Glossiness workflow
- ✅ Embedded and external textures
- ✅ Normal maps, metallic-roughness, occlusion, emissive
- ✅ Per-mesh local transforms
- ✅ Base64-encoded embedded images
- ✅ Multiple texture coordinates and vertex colors

**Not Yet Supported:**
- Skeletal animation and rigging
- Morph targets
- KHR extensions (in progress)

## Loading Models

### Basic Model Loading

```c
#include <AP2/AP2.h>

int main(void) {
    AP_Init(AP_INIT_ALL);

    // Load a glB model
    AP_Model *model = AP_LoadModel("assets/models/character.glb");

    // Check if loading succeeded
    if (!AP_ModelIsValid(model)) {
        AP_WARN("Failed to load model. Check file path and format.");
        return 1;
    }

    // Use the model...

    // Clean up
    AP_DestroyModel(model);
    AP_Quit();
    return 0;
}
```

### Error Handling

```c
AP_Model *model = AP_LoadModel("path/to/model.glb");

if (!AP_ModelIsValid(model)) {
    AP_ERROR("Failed to load model");
    // Create a fallback mesh
    AP_Mesh *fallback = AP_CreateMeshCube(2.0f);
    // ... use fallback
    AP_DestroyMesh(fallback);
    return;
}

int mesh_count = AP_ModelMeshCount(model);
if (mesh_count == 0) {
    AP_WARN("Model loaded but contains no meshes");
}
```

### Loading Meshes Directly

For single meshes, you can load without a model:

```c
// Load a mesh file (first mesh in the file)
AP_Mesh *mesh = AP_LoadMesh("assets/models/sphere.glb");

if (AP_MeshIsValid(mesh)) {
    printf("Mesh: %d vertices, %d indices\n",
        AP_MeshVertexCount(mesh),
        AP_MeshIndexCount(mesh));
}

AP_DrawMesh(mesh);
AP_DestroyMesh(mesh);
```

## Accessing Model Data

### Model Information

```c
void PrintModelInfo(AP_Model *model) {
    if (!AP_ModelIsValid(model)) {
        printf("Invalid model\n");
        return;
    }

    int mesh_count = AP_ModelMeshCount(model);
    int material_count = AP_ModelGetMaterialCount(model);
    int texture_count = AP_ModelGetTextureCount(model);

    printf("Model Information:\n");
    printf("  Meshes: %d\n", mesh_count);
    printf("  Materials: %d\n", material_count);
    printf("  Textures: %d\n", texture_count);

    // Print mesh details
    for (int i = 0; i < mesh_count; ++i) {
        AP_Mesh *mesh = AP_ModelGetMesh(model, i);
        if (mesh && AP_MeshIsValid(mesh)) {
            printf("\n  Mesh %d:\n", i);
            printf("    Vertices: %d\n", AP_MeshVertexCount(mesh));
            printf("    Indices: %d\n", AP_MeshIndexCount(mesh));

            AP_Material *mat = AP_MeshGetMaterial(mesh);
            if (mat) {
                printf("    Material: %s\n", mat->name ? mat->name : "Unnamed");
            }
        }
    }
}
```

### Iterating Meshes

```c
AP_Model *model = AP_LoadModel("model.glb");

for (int i = 0; i < AP_ModelMeshCount(model); ++i) {
    AP_Mesh *mesh = AP_ModelGetMesh(model, i);

    if (mesh && AP_MeshIsValid(mesh)) {
        // Get mesh properties
        int vert_count = AP_MeshVertexCount(mesh);
        int idx_count = AP_MeshIndexCount(mesh);

        // Get mesh material
        AP_Material *mat = AP_MeshGetMaterial(mesh);

        // Draw the mesh
        AP_DrawMesh(mesh);
    }
}
```

## Material Extraction

### Getting All Materials

```c
void ListMaterials(AP_Model *model) {
    int count = AP_ModelGetMaterialCount(model);

    printf("Materials (%d):\n", count);

    for (int i = 0; i < count; ++i) {
        AP_Material *mat = AP_ModelGetMaterial(model, i);
        if (mat) {
            printf("\n  [%d] %s\n", i, mat->name ? mat->name : "Unnamed");
            printf("      Type: %d\n", mat->type);
            printf("      Base Color: (%.2f, %.2f, %.2f, %.2f)\n",
                mat->base_color.r, mat->base_color.g,
                mat->base_color.b, mat->base_color.a);

            if (mat->type == AP_MATERIAL_TYPE_PBR_METALLIC_ROUGHNESS) {
                printf("      Metallic: %.2f\n", mat->metallic);
                printf("      Roughness: %.2f\n", mat->roughness);
                printf("      Has Base Texture: %s\n",
                    mat->has_base_color_texture ? "Yes" : "No");
                printf("      Has Normal Map: %s\n",
                    mat->has_normal_texture ? "Yes" : "No");
            }
        }
    }
}
```

### Modifying Materials from Loaded Models

```c
// IMPORTANT: Materials are owned by the model and shouldn't be destroyed
// Only modify if you're keeping the model alive

AP_Model *model = AP_LoadModel("model.glb");

// Get and modify a material
AP_Material *mat = AP_ModelGetMaterial(model, 0);
if (mat) {
    // Modify material properties
    // Note: Don't modify name or destroy the material
    mat->metallic = 0.8f;  // Increase metallic
    mat->roughness = 0.2f; // Decrease roughness

    // This material will be used when rendering
}

AP_DrawModel(model);

// Clean up
AP_DestroyModel(model);  // Also destroys materials
```

### Creating a Material Copy

```c
// To safely modify a material, create a copy
AP_Material *original = AP_ModelGetMaterial(model, 0);

// Copy the material
AP_Material *modified = AP_CreateMaterial("Modified", original->type);
if (modified) {
    *modified = *original;  // Copy all properties

    // Now safely modify
    modified->metallic = 0.5f;
    modified->roughness = 0.3f;

    // Use on a mesh
    AP_Mesh *mesh = AP_ModelGetMesh(model, 0);
    if (mesh) {
        AP_MeshSetMaterial(mesh, modified);
    }

    // Cleanup
    AP_DestroyMaterial(modified);
}
```

## Transform Control

### Model Transforms

```c
// Load model
AP_Model *model = AP_LoadModel("model.glb");

// Set position
AP_ModelSetPosition(model, AP_V3(0.0f, 1.0f, 0.0f));

// Set full transform (position, rotation, scale)
AP_ModelSetTRS(model,
    AP_V3(0.0f, 0.0f, 0.0f),           // Position
    AP_QuatFromEuler(0.0f, 45.0f, 0.0f), // Rotation (45 degrees around Y)
    AP_V3(1.0f, 1.0f, 1.0f)            // Scale
);

// Incremental transforms
AP_ModelTranslate(model, AP_V3(1.0f, 0.0f, 0.0f));  // Move by offset
AP_ModelRotate(model, AP_V3(0.0f, 1.0f, 0.0f), 45.0f);  // Rotate 45° around Y
AP_ModelScale(model, AP_V3(2.0f, 2.0f, 2.0f));  // Scale 2x

// Get current transform
AP_Mat4 transform = AP_ModelGetTransform(model);

// Reset to identity
AP_ModelResetTransform(model);
```

### Per-Mesh Transforms

Individual meshes within a model can have local transforms:

```c
for (int i = 0; i < AP_ModelMeshCount(model); ++i) {
    // Get local transform
    AP_Mat4 local = AP_ModelGetMeshTransform(model, i);

    // Set local transform
    AP_Mat4 new_local = AP_Mat4TRS(
        AP_V3(0.1f, 0.0f, 0.0f),  // Local position offset
        AP_QuatIdentity(),         // No local rotation
        AP_V3(1.0f, 1.0f, 1.0f)    // No local scale
    );
    AP_ModelSetMeshTransform(model, i, &new_local);
}
```

## Advanced Techniques

### Rendering Individual Meshes

```c
AP_Model *model = AP_LoadModel("model.glb");

// Render only specific meshes
for (int i = 0; i < AP_ModelMeshCount(model); ++i) {
    AP_Mesh *mesh = AP_ModelGetMesh(model, i);

    // Skip certain meshes
    if (i == 0) continue;  // Skip first mesh

    AP_Set3DPosition(AP_V3(i * 2.0f, 0.0f, 0.0f));
    AP_DrawMesh(mesh);
}
```

### Drawing with Custom Transforms

```c
// Draw model with override transform
AP_Mat4 custom_transform = AP_Mat4Scale(AP_V3(2.0f, 2.0f, 2.0f));
AP_DrawModelEx(model, &custom_transform, AP_C4(1.0f, 1.0f, 1.0f, 1.0f));

// Or use TRS convenience function
AP_DrawModelTRS(model,
    AP_V3(0.0f, 0.0f, 0.0f),
    AP_QuatFromEuler(0.0f, 45.0f, 0.0f),
    AP_V3(2.0f, 2.0f, 2.0f),
    AP_C4(1.0f, 1.0f, 1.0f, 1.0f)
);
```

### Tinting Meshes

```c
// Draw with a color tint
AP_DrawMesh(mesh, NULL, AP_C4(1.0f, 0.5f, 0.0f, 1.0f));  // Orange tint

// Draw with transform and tint
AP_DrawMeshTRS(mesh,
    AP_V3(0.0f, 0.0f, 0.0f),
    AP_QuatIdentity(),
    AP_V3(1.0f, 1.0f, 1.0f),
    AP_C4(1.0f, 0.5f, 0.0f, 1.0f)
);
```

### Accessing Textures

```c
void PrintTextureInfo(AP_Model *model) {
    int tex_count = AP_ModelGetTextureCount(model);

    printf("Textures (%d):\n", tex_count);

    for (int i = 0; i < tex_count; ++i) {
        AP_Texture *tex = AP_ModelGetTexture(model, i);
        if (tex) {
            printf("  [%d] %dx%d pixels, %d channels\n",
                i, tex->width, tex->height, tex->channels);
        }
    }
}
```

## Performance Considerations

### 1. File Format Selection

```c
// ✅ Prefer glB format for better performance
AP_Model *model = AP_LoadModel("model.glb");

// ⚠️  glTF with external files is slower
AP_Model *model = AP_LoadModel("model.gltf");
```

### 2. Batching Multiple Models

```c
// Load once, draw multiple times with different transforms
AP_Model *model = AP_LoadModel("character.glb");

for (int i = 0; i < 10; ++i) {
    AP_ModelSetPosition(model, AP_V3(i * 2.0f, 0.0f, 0.0f));
    AP_DrawModel(model);
}

AP_DestroyModel(model);
```

### 3. LOD (Level of Detail)

```c
// Load different models based on distance
typedef struct {
    AP_Model *high_detail;
    AP_Model *low_detail;
    AP_Vec3 position;
} Character;

void DrawCharacter(Character *ch, AP_Vec3 camera_pos) {
    float dist = AP_Vec3Distance(ch->position, camera_pos);

    if (dist < 10.0f) {
        AP_DrawModel(ch->high_detail);
    } else {
        AP_DrawModel(ch->low_detail);
    }
}
```

### 4. Memory Management

```c
// Load only needed models
AP_Model *visible_models[MAX_MODELS];
int visible_count = 0;

// Load visible models
for (int i = 0; i < scene_model_count; ++i) {
    if (IsInFrustum(scene_models[i].bounds)) {
        visible_models[visible_count++] =
            AP_LoadModel(scene_models[i].path);
    }
}

// Render
for (int i = 0; i < visible_count; ++i) {
    AP_DrawModel(visible_models[i]);
}

// Clean up
for (int i = 0; i < visible_count; ++i) {
    AP_DestroyModel(visible_models[i]);
}
```

## Complete Example: Model Viewer

```c
#include <AP2/AP2.h>

typedef struct {
    AP_Model *model;
    float rotation_y;
    AP_Vec3 position;
    float scale;
} ModelViewer;

int main(void) {
    AP_Init(AP_INIT_ALL);
    AP_Window *window = AP_CreateWindow("Model Viewer", 1280, 720, AP_WINDOW_RESIZABLE);
    AP_SetActiveWindow(window);

    AP_Camera camera = AP_CameraPerspective(
        AP_V3(0.0f, 2.0f, 5.0f),
        AP_V3(0.0f, 1.0f, 0.0f),
        60.0f
    );

    ModelViewer viewer = {0};
    viewer.model = AP_LoadModel("assets/models/model.glb");
    viewer.scale = 1.0f;
    viewer.position = AP_V3(0.0f, 0.0f, 0.0f);
    viewer.rotation_y = 0.0f;

    if (!AP_ModelIsValid(viewer.model)) {
        AP_ERROR("Failed to load model");
        return 1;
    }

    // Print model info
    printf("Model loaded:\n");
    printf("  Meshes: %d\n", AP_ModelMeshCount(viewer.model));
    printf("  Materials: %d\n", AP_ModelGetMaterialCount(viewer.model));
    printf("  Textures: %d\n", AP_ModelGetTextureCount(viewer.model));

    while (AP_IsRunning()) {
        AP_ClearLights();
        AP_Fill(0.1f, 0.1f, 0.1f, 1.0f);
        AP_PumpEvents();

        // Rotate model
        viewer.rotation_y += 0.5f;
        if (viewer.rotation_y > 360.0f) viewer.rotation_y -= 360.0f;

        // Update model transform
        AP_ModelSetTRS(viewer.model,
            viewer.position,
            AP_QuatFromEuler(0.0f, viewer.rotation_y, 0.0f),
            AP_V3(viewer.scale, viewer.scale, viewer.scale)
        );

        AP_Begin3D(&camera);

        // Setup lighting
        AP_AddLight(AP_LightDirectional(
            AP_V3(-1.0f, -1.0f, -1.0f),
            AP_C4(1.0f, 1.0f, 1.0f, 1.0f),
            1.2f
        ));
        AP_AddLight(AP_LightPoint(
            AP_V3(3.0f, 2.0f, 2.0f),
            AP_C4(1.0f, 0.8f, 0.6f, 1.0f),
            1.5f,
            15.0f
        ));
        AP_SetAmbientLight(AP_C4(0.3f, 0.3f, 0.3f, 1.0f));

        // Draw model
        AP_DrawModel(viewer.model);

        // Draw ground reference
        AP_Reset3DModel();
        AP_DrawPlane(AP_V3(0.0f, -0.1f, 0.0f), 20.0f, 20.0f,
                    AP_C4(0.5f, 0.5f, 0.5f, 1.0f));

        AP_End3D();
        AP_Present();
    }

    // Cleanup
    AP_DestroyModel(viewer.model);
    AP_DestroyWindow(window);
    AP_Quit();

    return 0;
}
```

## Troubleshooting

### Model Won't Load

```c
AP_Model *model = AP_LoadModel("model.glb");
if (!AP_ModelIsValid(model)) {
    // Check:
    // 1. File path is correct (relative to executable)
    // 2. File format is supported (glTF 2.0)
    // 3. File is not corrupted
    AP_ERROR("Model load failed");
}
```

### Materials Look Wrong

```c
// Ensure lighting is set up
AP_AddLight(AP_LightDirectional(...));
AP_SetAmbientLight(...);

// Check material properties
AP_Material *mat = AP_ModelGetMaterial(model, 0);
printf("Metallic: %.2f, Roughness: %.2f\n", mat->metallic, mat->roughness);
```

### Performance Issues

- Use `.glb` format instead of `.gltf`
- Reduce texture resolution
- Use LOD models for distant objects
- Batch multiple instances of the same model

## See Also

- [Materials and Textures](14-materials-textures.md)
- [Advanced PBR Workflows](16-pbr-advanced.md)
- [3D Rendering Guide](08-3d.md)
