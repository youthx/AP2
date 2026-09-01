# Best Practices for AP2 Materials & 3D Graphics

## Overview

This guide consolidates best practices for working with materials, textures, and 3D graphics in AP2 to achieve optimal performance, visual quality, and maintainability.

## Table of Contents

1. [Code Organization](#code-organization)
2. [Memory Management](#memory-management)
3. [Performance Optimization](#performance-optimization)
4. [Debugging](#debugging)
5. [Common Patterns](#common-patterns)
6. [Troubleshooting](#troubleshooting)

## Code Organization

### Project Structure

```
project/
├── src/
│   ├── main.c
│   ├── graphics/
│   │   ├── materials.c
│   │   ├── models.c
│   │   └── lighting.c
│   └── scenes/
│       ├── scene_main.c
│       └── scene_loading.c
├── assets/
│   ├── models/
│   │   ├── character/
│   │   │   ├── character.glb
│   │   │   └── character_hd.glb
│   │   ├── environment/
│   │   └── props/
│   ├── textures/
│   │   ├── materials/
│   │   ├── ui/
│   │   └── generated/
│   └── shaders/
└── include/
    └── graphics.h
```

### Material Definition Header

```c
// include/materials.h
#ifndef MATERIALS_H
#define MATERIALS_H

#include <AP2/AP2.h>

typedef enum {
    MATERIAL_METAL_STEEL,
    MATERIAL_METAL_GOLD,
    MATERIAL_PLASTIC_RED,
    MATERIAL_WOOD_OAK,
    MATERIAL_SKIN_FAIR,
    MATERIAL_COUNT
} MaterialID;

typedef struct {
    AP_Material *materials[MATERIAL_COUNT];
} MaterialLibrary;

MaterialLibrary *MaterialLibrary_Create(void);
void MaterialLibrary_Destroy(MaterialLibrary *lib);
AP_Material *MaterialLibrary_Get(MaterialLibrary *lib, MaterialID id);

#endif
```

### Material Implementation

```c
// src/graphics/materials.c
#include "materials.h"
#include "AP2/AP2.h"

static AP_Material *CreateSteelMaterial(void) {
    AP_Material *mat = AP_CreateMaterial("Steel", AP_MATERIAL_TYPE_PBR_METALLIC_ROUGHNESS);
    if (!mat) return NULL;

    mat->base_color = AP_C4(0.5f, 0.5f, 0.5f, 1.0f);
    mat->metallic = 1.0f;
    mat->roughness = 0.2f;

    return mat;
}

static AP_Material *CreateGoldMaterial(void) {
    AP_Material *mat = AP_CreateMaterial("Gold", AP_MATERIAL_TYPE_PBR_METALLIC_ROUGHNESS);
    if (!mat) return NULL;

    mat->base_color = AP_C4(1.0f, 0.84f, 0.0f, 1.0f);
    mat->metallic = 1.0f;
    mat->roughness = 0.04f;

    return mat;
}

MaterialLibrary *MaterialLibrary_Create(void) {
    MaterialLibrary *lib = (MaterialLibrary *)malloc(sizeof(MaterialLibrary));
    if (!lib) return NULL;

    lib->materials[MATERIAL_METAL_STEEL] = CreateSteelMaterial();
    lib->materials[MATERIAL_METAL_GOLD] = CreateGoldMaterial();
    // ... more materials

    // Verify all materials created
    for (int i = 0; i < MATERIAL_COUNT; ++i) {
        if (!lib->materials[i]) {
            AP_WARN("Failed to create material %d", i);
            MaterialLibrary_Destroy(lib);
            return NULL;
        }
    }

    return lib;
}

void MaterialLibrary_Destroy(MaterialLibrary *lib) {
    if (!lib) return;

    for (int i = 0; i < MATERIAL_COUNT; ++i) {
        if (lib->materials[i]) {
            AP_DestroyMaterial(lib->materials[i]);
            lib->materials[i] = NULL;
        }
    }

    free(lib);
}

AP_Material *MaterialLibrary_Get(MaterialLibrary *lib, MaterialID id) {
    if (!lib || id < 0 || id >= MATERIAL_COUNT) {
        return NULL;
    }
    return lib->materials[id];
}
```

## Memory Management

### Ownership Model

```c
// Clear ownership patterns:

// 1. Model OWNS materials and textures
AP_Model *model = AP_LoadModel("model.glb");
// Don't destroy model->materials or model->textures manually
AP_DestroyModel(model);  // Cleans everything up

// 2. Mesh DOESN'T own materials (usually)
AP_Mesh *mesh = AP_CreateMeshCube(1.0f);
AP_Material *mat = AP_CreateMaterial("Custom", AP_MATERIAL_TYPE_PBR_METALLIC_ROUGHNESS);
AP_MeshSetMaterial(mesh, mat);  // Material still owned by caller
AP_DestroyMesh(mesh);
AP_DestroyMaterial(mat);  // Caller must destroy

// 3. Exception: Mesh CAN own materials if set via special function
// (but currently this is managed as described in case 2)
```

### Proper Cleanup Pattern

```c
typedef struct {
    AP_Model *models[10];
    AP_Material *custom_materials[5];
    AP_Texture *textures[20];
    int model_count;
    int material_count;
    int texture_count;
} ResourceCache;

void ResourceCache_Cleanup(ResourceCache *cache) {
    // Destroy models FIRST (they own their materials/textures)
    for (int i = 0; i < cache->model_count; ++i) {
        if (cache->models[i]) {
            AP_DestroyModel(cache->models[i]);
            cache->models[i] = NULL;
        }
    }

    // Then destroy standalone materials
    for (int i = 0; i < cache->material_count; ++i) {
        if (cache->custom_materials[i]) {
            AP_DestroyMaterial(cache->custom_materials[i]);
            cache->custom_materials[i] = NULL;
        }
    }

    // Finally destroy textures (not owned by models)
    for (int i = 0; i < cache->texture_count; ++i) {
        if (cache->textures[i]) {
            AP_DestroyTexture(cache->textures[i]);
            cache->textures[i] = NULL;
        }
    }
}
```

### Error-Safe Resource Allocation

```c
AP_Model *LoadModelWithFallback(const char *path) {
    AP_Model *model = AP_LoadModel(path);

    if (!AP_ModelIsValid(model)) {
        AP_WARN("Failed to load %s, using fallback", path);

        // Create fallback cube
        AP_Mesh *cube = AP_CreateMeshCube(2.0f);
        if (!cube) {
            return NULL;  // Critical failure
        }

        AP_Material *mat = AP_CreateMaterial("Fallback", AP_MATERIAL_TYPE_PBR_METALLIC_ROUGHNESS);
        if (!mat) {
            AP_DestroyMesh(cube);
            return NULL;
        }

        AP_MaterialInitPbrMetallicRoughness(mat,
            AP_C4(1.0f, 0.0f, 0.0f, 1.0f),  // Red error indicator
            0.0f, 0.5f);

        AP_MeshSetMaterial(cube, mat);

        // Note: This is a workaround - ideally create a model from mesh
        // For now, cube is owned and must be destroyed by caller
    }

    return model;
}
```

## Performance Optimization

### Draw Call Batching

```c
// ❌ Bad: Many draw calls
for (int i = 0; i < 100; ++i) {
    AP_DrawModel(model_instances[i]);
}

// ✅ Good: Batch by material
typedef struct {
    AP_Model **models;
    int count;
    AP_Material *material;
} Batch;

void DrawBatch(Batch *batch) {
    for (int i = 0; i < batch->count; ++i) {
        AP_DrawModel(batch->models[i]);
    }
}
```

### Texture Memory Management

```c
// ❌ Bad: Loading textures every frame
for (int frame = 0; frame < 1000; ++frame) {
    AP_Texture *tex = AP_LoadTexture("data/texture.png");
    // use tex
    AP_DestroyTexture(tex);
}

// ✅ Good: Load once, reuse
AP_Texture *texture = AP_LoadTexture("data/texture.png");
for (int frame = 0; frame < 1000; ++frame) {
    // use texture
}
AP_DestroyTexture(texture);
```

### Mesh Reuse

```c
// Create primitive meshes once
typedef struct {
    AP_Mesh *cube;
    AP_Mesh *sphere;
    AP_Mesh *plane;
    AP_Mesh *pyramid;
} PrimitiveMeshes;

PrimitiveMeshes g_primitives = {0};

void InitPrimitives(void) {
    g_primitives.cube = AP_CreateMeshCube(1.0f);
    g_primitives.sphere = AP_CreateMeshSphere(1.0f, 32, 32);
    g_primitives.plane = AP_CreateMeshPlane(1.0f, 1.0f);
    // ...
}

void CleanupPrimitives(void) {
    if (g_primitives.cube) AP_DestroyMesh(g_primitives.cube);
    if (g_primitives.sphere) AP_DestroyMesh(g_primitives.sphere);
    // ...
}

// Usage: Transform and draw repeatedly
AP_Set3DPosition(AP_V3(0.0f, 0.0f, 0.0f));
AP_DrawMesh(g_primitives.cube);

AP_Set3DPosition(AP_V3(5.0f, 0.0f, 0.0f));
AP_DrawMesh(g_primitives.sphere);
```

### LOD (Level of Detail)

```c
typedef struct {
    AP_Model *high_quality;   // 50k triangles
    AP_Model *medium_quality; // 10k triangles
    AP_Model *low_quality;    // 1k triangles
    AP_Vec3 position;
} LODModel;

void DrawLODModel(LODModel *model, AP_Vec3 camera_pos) {
    float distance = AP_Vec3Distance(model->position, camera_pos);

    if (distance < 5.0f) {
        AP_DrawModel(model->high_quality);
    } else if (distance < 20.0f) {
        AP_DrawModel(model->medium_quality);
    } else {
        AP_DrawModel(model->low_quality);
    }
}
```

### Material Pooling

```c
#define MAX_MATERIALS 256

typedef struct {
    AP_Material *materials[MAX_MATERIALS];
    int count;
    bool in_use[MAX_MATERIALS];
} MaterialPool;

MaterialPool pool = {0};

AP_Material *MaterialPool_Acquire(const char *name, AP_MaterialType type) {
    // Find free slot
    for (int i = 0; i < MAX_MATERIALS; ++i) {
        if (!pool.in_use[i]) {
            if (!pool.materials[i]) {
                pool.materials[i] = AP_CreateMaterial(name, type);
            }
            pool.in_use[i] = true;
            return pool.materials[i];
        }
    }

    AP_WARN("Material pool exhausted");
    return NULL;
}

void MaterialPool_Release(AP_Material *material) {
    for (int i = 0; i < MAX_MATERIALS; ++i) {
        if (pool.materials[i] == material) {
            pool.in_use[i] = false;
            return;
        }
    }
}

void MaterialPool_Cleanup(void) {
    for (int i = 0; i < MAX_MATERIALS; ++i) {
        if (pool.materials[i]) {
            AP_DestroyMaterial(pool.materials[i]);
            pool.materials[i] = NULL;
        }
    }
}
```

## Debugging

### Material Inspection Utility

```c
void DebugPrintMaterial(AP_Material *mat) {
    if (!mat) {
        printf("Material is NULL\n");
        return;
    }

    printf("=== Material Debug Info ===\n");
    printf("Name: %s\n", mat->name ? mat->name : "Unnamed");
    printf("Type: %d\n", mat->type);
    printf("Base Color: (%.3f, %.3f, %.3f, %.3f)\n",
        mat->base_color.r, mat->base_color.g,
        mat->base_color.b, mat->base_color.a);
    printf("Metallic: %.3f\n", mat->metallic);
    printf("Roughness: %.3f\n", mat->roughness);
    printf("Alpha Mode: %d (0=opaque, 1=mask, 2=blend)\n", mat->alpha_mode);
    printf("Alpha Cutoff: %.3f\n", mat->alpha_cutoff);
    printf("Double Sided: %s\n", mat->double_sided ? "Yes" : "No");
    printf("Emissive: (%.3f, %.3f, %.3f)\n",
        mat->emissive_factor.r, mat->emissive_factor.g, mat->emissive_factor.b);
    printf("Has Base Texture: %s\n", mat->has_base_color_texture ? "Yes" : "No");
    printf("Has Normal: %s\n", mat->has_normal_texture ? "Yes" : "No");
    printf("Has Metallic/Roughness: %s\n", mat->has_metallic_roughness_texture ? "Yes" : "No");
    printf("Has Occlusion: %s\n", mat->has_occlusion_texture ? "Yes" : "No");
    printf("Has Emissive Texture: %s\n", mat->has_emissive_texture ? "Yes" : "No");
    printf("===========================\n");
}

void DebugPrintModel(AP_Model *model) {
    if (!AP_ModelIsValid(model)) {
        printf("Model is invalid\n");
        return;
    }

    printf("=== Model Debug Info ===\n");
    printf("Valid: Yes\n");
    printf("Mesh Count: %d\n", AP_ModelMeshCount(model));
    printf("Material Count: %d\n", AP_ModelGetMaterialCount(model));
    printf("Texture Count: %d\n", AP_ModelGetTextureCount(model));

    AP_Mat4 transform = AP_ModelGetTransform(model);
    printf("Transform Matrix:\n");
    for (int i = 0; i < 4; ++i) {
        printf("  [%.3f, %.3f, %.3f, %.3f]\n",
            transform.m[i*4+0], transform.m[i*4+1],
            transform.m[i*4+2], transform.m[i*4+3]);
    }

    printf("\nMeshes:\n");
    for (int i = 0; i < AP_ModelMeshCount(model); ++i) {
        AP_Mesh *mesh = AP_ModelGetMesh(model, i);
        if (mesh && AP_MeshIsValid(mesh)) {
            printf("  [%d] Vertices: %d, Indices: %d\n",
                i, AP_MeshVertexCount(mesh), AP_MeshIndexCount(mesh));

            AP_Material *mat = AP_MeshGetMaterial(mesh);
            if (mat) {
                printf("       Material: %s\n", mat->name ? mat->name : "Unnamed");
            }
        }
    }
    printf("========================\n");
}
```

### Visual Debugging

```c
void DebugVisualizeNormals(AP_Mesh *mesh) {
    // Draw normal lines for debugging
    // (requires access to vertex data - use for debugging only)

    // This would require custom shaders to visualize
    // For now, use color to verify materials are applied:
    AP_Set3DTint(AP_C4(1.0f, 0.0f, 0.0f, 1.0f));  // Red tint
    AP_DrawMesh(mesh);
    AP_Set3DTint(AP_C4(1.0f, 1.0f, 1.0f, 1.0f));  // Reset
}

void DebugVisualizeMaterials(AP_Model *model) {
    // Render with solid colors to verify material assignment
    for (int i = 0; i < AP_ModelMeshCount(model); ++i) {
        AP_Mesh *mesh = AP_ModelGetMesh(model, i);

        // Different color per mesh to verify separation
        AP_Color colors[] = {
            AP_C4(1.0f, 0.0f, 0.0f, 1.0f),  // Red
            AP_C4(0.0f, 1.0f, 0.0f, 1.0f),  // Green
            AP_C4(0.0f, 0.0f, 1.0f, 1.0f),  // Blue
        };

        AP_Set3DTint(colors[i % 3]);
        AP_DrawMesh(mesh);
    }

    AP_Set3DTint(AP_C4(1.0f, 1.0f, 1.0f, 1.0f));  // Reset
}
```

## Common Patterns

### Material Variant System

```c
typedef struct {
    AP_Material *base;
    float metallic_override;
    float roughness_override;
} MaterialVariant;

MaterialVariant *MaterialVariant_Create(AP_Material *base) {
    MaterialVariant *var = (MaterialVariant *)malloc(sizeof(MaterialVariant));
    if (!var) return NULL;

    var->base = base;
    var->metallic_override = -1.0f;  // -1 means use base value
    var->roughness_override = -1.0f;

    return var;
}

void MaterialVariant_Apply(MaterialVariant *variant, AP_Material *target) {
    if (variant->metallic_override >= 0.0f) {
        target->metallic = variant->metallic_override;
    }
    if (variant->roughness_override >= 0.0f) {
        target->roughness = variant->roughness_override;
    }
}
```

### Mesh-Material Binding

```c
typedef struct {
    AP_Mesh *mesh;
    AP_Material *material;
} MeshMaterialPair;

void DrawMeshMaterialPair(MeshMaterialPair *pair) {
    AP_MeshSetMaterial(pair->mesh, pair->material);
    AP_DrawMesh(pair->mesh);
}
```

## Troubleshooting

### Textures Not Appearing

```c
void DebugMissingTexture(AP_Material *mat) {
    if (mat->has_base_color_texture && mat->base_color_texture == 0) {
        AP_WARN("Material has texture flag but texture ID is 0");
        // Texture failed to load
    }

    if (!mat->has_base_color_texture) {
        // Use base color instead
        printf("Base Color: (%.2f, %.2f, %.2f)\n",
            mat->base_color.r, mat->base_color.g, mat->base_color.b);
    }
}
```

### Models Not Rendering

```c
bool DebugValidateModel(AP_Model *model) {
    if (!AP_ModelIsValid(model)) {
        AP_ERROR("Model is invalid");
        return false;
    }

    int mesh_count = AP_ModelMeshCount(model);
    if (mesh_count == 0) {
        AP_ERROR("Model has no meshes");
        return false;
    }

    for (int i = 0; i < mesh_count; ++i) {
        AP_Mesh *mesh = AP_ModelGetMesh(model, i);
        if (!mesh || !AP_MeshIsValid(mesh)) {
            AP_ERROR("Mesh %d is invalid", i);
            return false;
        }
    }

    return true;
}
```

### Material Changes Not Visible

```c
// Remember: Some materials are owned by models
// Changes to model materials are reflected, but
// changes to mesh materials may require redraw

void UpdateMaterialCorrectly(AP_Model *model, int mesh_index) {
    // ✅ Correct: Get material from model
    AP_Material *mat = AP_ModelGetMaterial(model, mesh_index);
    if (mat) {
        mat->roughness = 0.5f;  // Change is reflected
    }

    // ⚠️  Be careful: Mesh material ownership
    AP_Mesh *mesh = AP_ModelGetMesh(model, mesh_index);
    AP_Material *mesh_mat = AP_MeshGetMaterial(mesh);
    if (mesh_mat && mesh_mat == mat) {
        // Both point to same material - change is reflected
    }
}
```

## Summary

Key takeaways:
1. **Clear ownership** - Know who owns each resource
2. **Proper cleanup** - Destroy in reverse creation order
3. **Batch efficiently** - Reuse meshes, materials, and textures
4. **Debug thoroughly** - Use inspection utilities liberally
5. **Handle errors** - Always check return values and use fallbacks

## See Also

- [Materials and Textures](14-materials-textures.md)
- [3D Models and glTF Loading](15-gltf-models.md)
- [Advanced PBR Workflows](16-pbr-advanced.md)
