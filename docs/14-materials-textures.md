# Materials and Textures in AP2

## Overview

AP2 provides a comprehensive material and texture system designed for modern PBR (Physically-Based Rendering) workflows. This guide covers everything from basic material creation to advanced techniques.

## Table of Contents

1. [Core Concepts](#core-concepts)
2. [Material Types](#material-types)
3. [Creating Materials](#creating-materials)
4. [Texture Management](#texture-management)
5. [Advanced Material Properties](#advanced-material-properties)
6. [Working with Models](#working-with-models)
7. [Best Practices](#best-practices)

## Core Concepts

### Material

A material defines how a surface looks when rendered. It contains:
- **Color properties** (base color, specular, emissive)
- **Surface properties** (metallic, roughness, alpha)
- **Texture maps** (base color, normal, metallic-roughness, etc.)
- **Material type** (PBR Metallic-Roughness, Specular-Glossiness, Unlit, Custom)

### Texture

A texture is a 2D image stored on the GPU. It can represent:
- **Diffuse/Albedo** - Base color information
- **Normal** - Surface detail and bump information
- **Metallic-Roughness** - Metal and surface roughness information
- **Occlusion** - Ambient occlusion data
- **Emissive** - Self-illumination data

## Material Types

AP2 supports four material types:

### 1. PBR Metallic-Roughness (Recommended)

The standard glTF 2.0 material workflow. Uses:
- Base color
- Metallic factor (0.0 = non-metal, 1.0 = metal)
- Roughness factor (0.0 = mirror, 1.0 = rough)

```c
AP_Material *mat = AP_CreateMaterial("Steel", AP_MATERIAL_TYPE_PBR_METALLIC_ROUGHNESS);
AP_MaterialInitPbrMetallicRoughness(mat,
    AP_C4(0.5f, 0.5f, 0.5f, 1.0f),  // base color
    0.8f,  // metallic
    0.3f   // roughness
);
```

**Best for:** Realistic materials like metals, plastics, wood, stone

### 2. PBR Specular-Glossiness (Legacy)

An older workflow using:
- Diffuse color
- Specular color
- Glossiness factor

```c
AP_Material *mat = AP_CreateMaterial("Plastic", AP_MATERIAL_TYPE_PBR_SPECULAR_GLOSSINESS);
mat->specular_factor = AP_C4(1.0f, 1.0f, 1.0f, 1.0f);
mat->glossiness_factor = 0.8f;
```

**Best for:** Legacy assets or specific artistic styles

### 3. Unlit

A simple material with no lighting calculation. Perfect for:
- UI elements
- Emissive surfaces
- Post-process effects

```c
AP_Material *mat = AP_CreateMaterial("Neon", AP_MATERIAL_TYPE_UNLIT);
AP_MaterialInitUnlit(mat, AP_C4(0.0f, 1.0f, 1.0f, 1.0f));
```

### 4. Custom

Reserved for application-specific materials. Set `user_data` pointer for custom parameters.

## Creating Materials

### Basic Material Creation

```c
// Create a material
AP_Material *material = AP_CreateMaterial("MyMaterial", AP_MATERIAL_TYPE_PBR_METALLIC_ROUGHNESS);

// Set properties
AP_MaterialInitPbrMetallicRoughness(material,
    AP_C4(0.9f, 0.1f, 0.1f, 1.0f),  // Red base color
    0.0f,   // Not metallic
    0.5f    // Medium roughness
);

// When done, destroy it
AP_DestroyMaterial(material);
```

### Material with Properties

```c
AP_Material *wood = AP_CreateMaterial("Oak Wood", AP_MATERIAL_TYPE_PBR_METALLIC_ROUGHNESS);

// Set base properties
wood->base_color = AP_C4(0.8f, 0.6f, 0.3f, 1.0f);
wood->metallic = 0.0f;
wood->roughness = 0.8f;

// Set alpha properties
wood->alpha_mode = AP_ALPHA_MODE_OPAQUE;
wood->double_sided = false;

// Set emissive for glow
wood->emissive_factor = AP_C4(0.0f, 0.0f, 0.0f, 1.0f);
```

### Checking Material Properties

```c
AP_Material *mat = AP_ModelGetMaterial(model, 0);
if (mat) {
    printf("Material: %s\n", mat->name);
    printf("Type: %d\n", mat->type);
    printf("Base Color: (%.2f, %.2f, %.2f, %.2f)\n",
        mat->base_color.r, mat->base_color.g, mat->base_color.b, mat->base_color.a);
    printf("Metallic: %.2f, Roughness: %.2f\n", mat->metallic, mat->roughness);

    // Check transparency
    if (AP_MaterialNeedsAlphaBlend(mat)) {
        printf("Material requires alpha blending\n");
    }
    if (AP_MaterialIsDoubleSided(mat)) {
        printf("Material is double-sided\n");
    }
}
```

## Texture Management

### Loading Textures

```c
// Load a texture from disk
AP_Texture *diffuse = AP_LoadTexture("assets/textures/wood_diffuse.png");
AP_Texture *normal = AP_LoadTexture("assets/textures/wood_normal.png");
AP_Texture *roughness = AP_LoadTexture("assets/textures/wood_roughness.png");

if (!diffuse) {
    AP_WARN("Failed to load texture");
}
```

### Creating Textures from Pixels

```c
// Create texture from raw pixel data
AP_U8 *pixels = /* raw RGBA pixel data */;
AP_Texture *texture = AP_CreateTextureFromPixels(pixels, width, height, 4);
```

### Attaching Textures to Materials

```c
AP_Material *mat = AP_CreateMaterial("Wood", AP_MATERIAL_TYPE_PBR_METALLIC_ROUGHNESS);

// Set base color texture
AP_MaterialSetBaseColorTexture(mat, diffuse->id);

// Set normal map
AP_MaterialSetNormalTexture(mat, normal->id, 1.0f);

// Set metallic-roughness texture
AP_MaterialSetMetallicRoughnessTexture(mat, roughness->id);

// Set occlusion
AP_MaterialSetOcclusionTexture(mat, occlusion->id, 1.0f);

// Set emissive
AP_MaterialSetEmissiveTexture(mat, emissive->id);
```

### Cleaning Up Textures

```c
AP_DestroyTexture(diffuse);
AP_DestroyTexture(normal);
AP_DestroyTexture(roughness);
```

## Advanced Material Properties

### Alpha Modes

Control how transparency is handled:

```c
// Opaque - no transparency
material->alpha_mode = AP_ALPHA_MODE_OPAQUE;

// Masked - binary transparency (on/off at threshold)
material->alpha_mode = AP_ALPHA_MODE_MASK;
material->alpha_cutoff = 0.5f;  // Transparency threshold

// Blend - smooth transparency
material->alpha_mode = AP_ALPHA_MODE_BLEND;
```

### Double-Sided Rendering

```c
// Render both sides of the surface
material->double_sided = true;
```

### Emissive Materials

Create self-illuminating surfaces:

```c
AP_Material *neon = AP_CreateMaterial("Neon Sign", AP_MATERIAL_TYPE_UNLIT);
neon->base_color = AP_C4(0.0f, 1.0f, 0.0f, 1.0f);
neon->emissive_factor = AP_C4(0.0f, 2.0f, 0.0f, 1.0f);  // HDR green glow
```

### Material Transparency Checks

```c
// Check if material needs special handling
if (AP_MaterialIsTransparent(material)) {
    printf("Material has transparency\n");
}

if (AP_MaterialNeedsAlphaBlend(material)) {
    printf("Material needs alpha blending (may render differently)\n");
}
```

## Working with Models

### Loading Models with Materials

glTF/glB files can include complete material and texture information:

```c
// Load a complete model with all materials and textures
AP_Model *model = AP_LoadModel("assets/models/character.glb");

if (AP_ModelIsValid(model)) {
    int mat_count = AP_ModelGetMaterialCount(model);
    int tex_count = AP_ModelGetTextureCount(model);
    int mesh_count = AP_ModelMeshCount(model);

    printf("Loaded model with:\n");
    printf("  %d meshes\n", mesh_count);
    printf("  %d materials\n", mat_count);
    printf("  %d textures\n", tex_count);
}
```

### Accessing Mesh Materials

```c
for (int i = 0; i < AP_ModelMeshCount(model); ++i) {
    AP_Mesh *mesh = AP_ModelGetMesh(model, i);
    AP_Material *mat = AP_MeshGetMaterial(mesh);

    if (mat) {
        printf("Mesh %d uses material: %s\n", i, mat->name);
    }
}
```

### Accessing Model Materials

```c
// Get specific material by index
AP_Material *mat = AP_ModelGetMaterial(model, 0);
if (mat) {
    printf("Material 0: %s\n", mat->name);
    printf("  Base Color: (%.2f, %.2f, %.2f)\n",
        mat->base_color.r, mat->base_color.g, mat->base_color.b);
}
```

### Accessing Model Textures

```c
// Get all textures from the model
int tex_count = AP_ModelGetTextureCount(model);
for (int i = 0; i < tex_count; ++i) {
    AP_Texture *tex = AP_ModelGetTexture(model, i);
    if (tex) {
        printf("Texture %d: %dx%d\n", i, tex->width, tex->height);
    }
}
```

### Creating Meshes with Materials

```c
// Create a mesh
AP_Mesh *cube = AP_CreateMeshCube(2.0f);

// Create and set a material
AP_Material *mat = AP_CreateMaterial("CubeMat", AP_MATERIAL_TYPE_PBR_METALLIC_ROUGHNESS);
AP_MaterialInitPbrMetallicRoughness(mat,
    AP_C4(1.0f, 0.0f, 0.0f, 1.0f),  // Red
    0.2f, 0.4f);

AP_MeshSetMaterial(cube, mat);

// Draw with the material
AP_DrawMesh(cube);
```

## Best Practices

### 1. Texture Organization

```
assets/
  models/
    character/
      character.glb
      textures/
        diffuse.png
        normal.png
        roughness.png
```

### 2. Material Reuse

```c
// Create materials once
AP_Material *plastic = AP_CreateMaterial("Plastic", AP_MATERIAL_TYPE_PBR_METALLIC_ROUGHNESS);
AP_MaterialInitPbrMetallicRoughness(plastic, AP_C4(1.0f, 1.0f, 1.0f, 1.0f), 0.0f, 0.2f);

// Reuse for multiple meshes
AP_MeshSetMaterial(mesh1, plastic);
AP_MeshSetMaterial(mesh2, plastic);

// Clean up once when done
AP_DestroyMaterial(plastic);
```

### 3. Error Checking

```c
AP_Texture *tex = AP_LoadTexture("path/to/texture.png");
if (!tex) {
    AP_WARN("Failed to load texture, using fallback");
    // Use a fallback material or texture
}

AP_Model *model = AP_LoadModel("path/to/model.glb");
if (!AP_ModelIsValid(model)) {
    AP_WARN("Failed to load model");
    return;
}
```

### 4. Resource Cleanup

```c
void CleanupResources(void) {
    // Clean up meshes
    AP_DestroyMesh(mesh);

    // Clean up materials
    AP_DestroyMaterial(material);

    // Clean up models (also destroys internal meshes and materials)
    AP_DestroyModel(model);

    // Clean up textures if not owned by model
    if (external_texture) {
        AP_DestroyTexture(external_texture);
    }
}
```

### 5. Performance Tips

- **Load models** with embedded textures (glB format) rather than separate files
- **Reuse materials** across multiple meshes when possible
- **Use appropriate alpha modes** - avoid BLEND if MASK is sufficient
- **Texture resolution** - use mipmaps for distant objects
- **Material count** - group similar materials to reduce state changes

## Complete Example

```c
#include <AP2/AP2.h>

int main(void) {
    AP_Init(AP_INIT_ALL);
    AP_Window *window = AP_CreateWindow("Material Demo", 1280, 720, AP_WINDOW_RESIZABLE);
    AP_SetActiveWindow(window);

    AP_Camera camera = AP_CameraPerspective(
        AP_V3(0.0f, 2.0f, 8.0f),
        AP_V3(0.0f, 0.0f, 0.0f),
        60.0f
    );

    // Load model with materials
    AP_Model *model = AP_LoadModel("assets/models/scene.glb");

    // Create custom material
    AP_Material *steel = AP_CreateMaterial("Steel", AP_MATERIAL_TYPE_PBR_METALLIC_ROUGHNESS);
    AP_MaterialInitPbrMetallicRoughness(steel,
        AP_C4(0.5f, 0.5f, 0.5f, 1.0f),
        0.9f,   // Very metallic
        0.1f    // Very smooth
    );

    AP_Mesh *sphere = AP_CreateMeshSphere(1.0f, 32, 32);
    AP_MeshSetMaterial(sphere, steel);

    while (AP_IsRunning()) {
        AP_ClearLights();
        AP_Fill(0.1f, 0.1f, 0.1f, 1.0f);
        AP_PumpEvents();

        AP_Begin3D(&camera);

        // Add lighting
        AP_AddLight(AP_LightDirectional(
            AP_V3(-1.0f, -1.0f, -1.0f),
            AP_C4(1.0f, 1.0f, 1.0f, 1.0f),
            1.2f
        ));
        AP_AddLight(AP_LightPoint(
            AP_V3(3.0f, 3.0f, 3.0f),
            AP_C4(1.0f, 0.8f, 0.6f, 1.0f),
            1.5f,
            20.0f
        ));

        // Draw model
        if (AP_ModelIsValid(model)) {
            AP_DrawModel(model);
        }

        // Draw steel sphere
        AP_Set3DPosition(AP_V3(4.0f, 0.0f, 0.0f));
        AP_DrawMesh(sphere);

        AP_End3D();
        AP_Present();
    }

    // Cleanup
    if (AP_ModelIsValid(model)) AP_DestroyModel(model);
    AP_DestroyMaterial(steel);
    AP_DestroyMesh(sphere);
    AP_DestroyWindow(window);
    AP_Quit();

    return 0;
}
```

## See Also

- [3D Models and GLTF Loading](15-gltf-models.md)
- [Advanced PBR Workflows](16-pbr-advanced.md)
- [3D Rendering Guide](08-3d.md)
