# Advanced PBR Workflows in AP2

## Overview

This guide covers advanced Physically-Based Rendering (PBR) workflows in AP2, including material creation, texture baking, and optimization techniques for professional-quality rendering.

## Table of Contents

1. [PBR Fundamentals](#pbr-fundamentals)
2. [Metallic-Roughness Workflow](#metallic-roughness-workflow)
3. [Texture Baking](#texture-baking)
4. [Material Database](#material-database)
5. [Environmental Lighting](#environmental-lighting)
6. [Optimization Techniques](#optimization-techniques)
7. [Real-World Examples](#real-world-examples)

## PBR Fundamentals

### The PBR Equation (Simplified)

```
Final Color = (Diffuse + Specular) × Lighting × Shadows
Where:
  Diffuse = Base Color × (1 - Metallic)
  Specular = F0 + (F1 - F0) × (1 - dot(V, H))^5
  F0 = Metallic ? Specular Color : 0.04
```

### Key PBR Properties

1. **Base Color (Albedo)**
   - Pure surface color without lighting
   - Always linear, never premultiplied
   - Range: [0, 1]

2. **Metallic**
   - 0.0 = Non-metal (dielectric)
   - 1.0 = Metal (conductor)
   - Most values are 0 or 1, rarely in-between
   - Range: [0, 1]

3. **Roughness**
   - 0.0 = Mirror-like (specular)
   - 1.0 = Completely diffuse
   - Affects microfacet distribution
   - Range: [0, 1]

4. **Normal Map**
   - Encodes surface detail
   - Tangent-space normal vectors
   - Does NOT change base geometry

5. **Occlusion (AO)**
   - Darkens crevices and contact shadows
   - Usually multiplied with diffuse
   - Range: [0, 1]

6. **Emissive**
   - Self-illuminated light
   - Typically multiplied with base color
   - Can use HDR values > 1.0 for glow

## Metallic-Roughness Workflow

### Material Classifications

#### 1. Metals

```c
AP_Material *copper = AP_CreateMaterial("Copper", AP_MATERIAL_TYPE_PBR_METALLIC_ROUGHNESS);

// Copper oxidation variation
copper->base_color = AP_C4(0.96f, 0.64f, 0.54f, 1.0f);  // Polished
copper->metallic = 1.0f;     // Pure metal
copper->roughness = 0.05f;   // Very smooth

// F0 (Fresnel at 0°) is automatically calculated from metallic=1.0
// Copper F0 ≈ (0.95, 0.64, 0.54)
```

**Common Metal Base Colors:**
- **Gold**: (1.0, 0.84, 0.0)
- **Silver**: (0.95, 0.93, 0.88)
- **Copper**: (0.96, 0.64, 0.54)
- **Aluminum**: (0.91, 0.92, 0.92)
- **Steel**: (0.50, 0.50, 0.50)

#### 2. Dielectrics (Non-Metals)

```c
AP_Material *plastic = AP_CreateMaterial("Plastic", AP_MATERIAL_TYPE_PBR_METALLIC_ROUGHNESS);

plastic->base_color = AP_C4(0.2f, 0.8f, 0.1f, 1.0f);  // Green plastic
plastic->metallic = 0.0f;    // Not metallic
plastic->roughness = 0.3f;   // Slightly rough

// F0 for dielectrics is always ~0.04 (no need to specify)
```

**Common Dielectric Materials:**
- **Wood**: Base (0.6-0.8, 0.4-0.6, 0.2-0.4), Roughness 0.5-0.8
- **Plastic**: Base (varies), Metallic 0, Roughness 0.1-0.4
- **Concrete**: Base (0.5-0.6, 0.5-0.6, 0.5-0.6), Roughness 0.8-1.0
- **Fabric**: Base (varies), Metallic 0, Roughness 0.6-1.0
- **Skin**: Base (0.8-1.0, 0.6-0.8, 0.5-0.7), Metallic 0, Roughness 0.2-0.4

#### 3. Hybrid Materials (Wear, Patina)

```c
AP_Material *aged_metal = AP_CreateMaterial("Aged Metal", AP_MATERIAL_TYPE_PBR_METALLIC_ROUGHNESS);

// Metal base with oxidation
aged_metal->base_color = AP_C4(0.3f, 0.35f, 0.38f, 1.0f);  // Tarnished steel
aged_metal->metallic = 0.8f;    // Mostly metallic (40% corrosion)
aged_metal->roughness = 0.6f;   // Rough oxidation layer

// F0 is interpolated: lerp(0.04, F0_metal, metallic)
```

### Roughness Interpretation

```c
// Roughness maps the microsurface detail level:
// 0.0 - 0.1: Polished (phones, mirrors, water)
// 0.1 - 0.3: Smooth (brushed metal, plastic)
// 0.3 - 0.7: Medium (most objects, weathered surfaces)
// 0.7 - 1.0: Rough (gravel, concrete, fabric)

void MaterialComparison(void) {
    AP_Material *smooth = AP_CreateMaterial("Smooth", AP_MATERIAL_TYPE_PBR_METALLIC_ROUGHNESS);
    smooth->metallic = 1.0f;
    smooth->roughness = 0.1f;   // Sharp reflections

    AP_Material *rough = AP_CreateMaterial("Rough", AP_MATERIAL_TYPE_PBR_METALLIC_ROUGHNESS);
    rough->metallic = 1.0f;
    rough->roughness = 0.8f;    // Diffuse reflections

    // Both are metal, but have very different appearances
}
```

### Complex Materials with Textures

```c
void CreateWoodMaterial(void) {
    AP_Material *wood = AP_CreateMaterial("Oak", AP_MATERIAL_TYPE_PBR_METALLIC_ROUGHNESS);

    // Base properties
    wood->base_color = AP_C4(0.8f, 0.6f, 0.3f, 1.0f);  // Wood color
    wood->metallic = 0.0f;
    wood->roughness = 0.5f;

    // Texture maps
    AP_Texture *diffuse = AP_LoadTexture("assets/wood_diffuse.png");
    AP_Texture *normal = AP_LoadTexture("assets/wood_normal.png");
    AP_Texture *roughness_map = AP_LoadTexture("assets/wood_roughness.png");
    AP_Texture *ao = AP_LoadTexture("assets/wood_ao.png");

    if (diffuse) AP_MaterialSetBaseColorTexture(wood, diffuse->id);
    if (normal) AP_MaterialSetNormalTexture(wood, normal->id, 1.0f);
    if (roughness_map) AP_MaterialSetMetallicRoughnessTexture(wood, roughness_map->id);
    if (ao) AP_MaterialSetOcclusionTexture(wood, ao->id, 1.0f);

    // The textures now override the base properties
    // Texture values multiply with the base color/properties
}
```

## Texture Baking

### Normal Map Baking

Normal maps encode surface detail in RGB:
- Red (R): X direction
- Green (G): Y direction
- Blue (B): Z direction

```c
void SetupNormalMap(AP_Material *mat, const char *normal_path) {
    AP_Texture *normal_tex = AP_LoadTexture(normal_path);
    if (normal_tex) {
        // Normal scale controls intensity (1.0 = default)
        AP_MaterialSetNormalTexture(mat, normal_tex->id, 1.0f);

        // Lower values = less detail
        // Higher values = more intense detail
    }
}
```

### Roughness Map Baking

Separate roughness maps for precise control:

```c
void SetupRoughnessMap(AP_Material *mat, const char *rough_path) {
    AP_Texture *roughness_tex = AP_LoadTexture(rough_path);
    if (roughness_tex) {
        // Roughness is stored in the green channel
        // Values: 0 (smooth) to 1 (rough)
        AP_MaterialSetMetallicRoughnessTexture(mat, roughness_tex->id);
    }
}
```

### Metallic-Roughness Texture Format

The combined metallic-roughness texture uses:
- **Red channel**: Unused (reserved)
- **Green channel**: Roughness (0 = smooth, 1 = rough)
- **Blue channel**: Metallic (0 = non-metal, 1 = metal)

```c
// Example: Standard glTF format
void LoadCombinedMetallicRoughness(AP_Material *mat, const char *path) {
    AP_Texture *tex = AP_LoadTexture(path);
    if (tex) {
        // This single texture contains both properties
        AP_MaterialSetMetallicRoughnessTexture(mat, tex->id);
    }
}
```

### Occlusion Baking

Ambient occlusion darkens crevices:

```c
void SetupOcclusionMap(AP_Material *mat, const char *ao_path) {
    AP_Texture *ao_tex = AP_LoadTexture(ao_path);
    if (ao_tex) {
        // Strength 0.0 = no effect, 1.0 = full effect
        AP_MaterialSetOcclusionTexture(mat, ao_tex->id, 1.0f);
    }
}
```

## Material Database

### Organizing Materials

```c
#define MATERIAL_COUNT 20

typedef struct {
    AP_Material *materials[MATERIAL_COUNT];
    int count;
} MaterialLibrary;

MaterialLibrary CreateMaterialLibrary(void) {
    MaterialLibrary lib = {0};

    // Metals
    lib.materials[lib.count++] = CreateMetalMaterial("Gold",
        AP_C4(1.0f, 0.84f, 0.0f, 1.0f), 0.04f);
    lib.materials[lib.count++] = CreateMetalMaterial("Silver",
        AP_C4(0.95f, 0.93f, 0.88f, 1.0f), 0.02f);
    lib.materials[lib.count++] = CreateMetalMaterial("Steel",
        AP_C4(0.50f, 0.50f, 0.50f, 1.0f), 0.1f);

    // Dielectrics
    lib.materials[lib.count++] = CreatePlasticMaterial("Red Plastic",
        AP_C4(1.0f, 0.0f, 0.0f, 1.0f), 0.3f);
    lib.materials[lib.count++] = CreatePlasticMaterial("White Plastic",
        AP_C4(0.95f, 0.95f, 0.95f, 1.0f), 0.1f);

    return lib;
}

void DestroyMaterialLibrary(MaterialLibrary *lib) {
    for (int i = 0; i < lib->count; ++i) {
        AP_DestroyMaterial(lib->materials[i]);
    }
    lib->count = 0;
}

AP_Material *CreateMetalMaterial(const char *name, AP_Color base, float roughness) {
    AP_Material *mat = AP_CreateMaterial(name, AP_MATERIAL_TYPE_PBR_METALLIC_ROUGHNESS);
    mat->base_color = base;
    mat->metallic = 1.0f;
    mat->roughness = roughness;
    return mat;
}

AP_Material *CreatePlasticMaterial(const char *name, AP_Color base, float roughness) {
    AP_Material *mat = AP_CreateMaterial(name, AP_MATERIAL_TYPE_PBR_METALLIC_ROUGHNESS);
    mat->base_color = base;
    mat->metallic = 0.0f;
    mat->roughness = roughness;
    return mat;
}
```

## Environmental Lighting

### Three-Point Lighting Setup

```c
void SetupThreePointLighting(void) {
    // Key light (main)
    AP_AddLight(AP_LightDirectional(
        AP_V3(-1.0f, -1.0f, -0.5f),      // Direction
        AP_C4(1.0f, 0.95f, 0.85f, 1.0f),  // Warm white
        1.2f                               // Intensity
    ));

    // Fill light (soften shadows)
    AP_AddLight(AP_LightPoint(
        AP_V3(2.0f, 1.5f, 1.0f),         // Position
        AP_C4(0.8f, 0.8f, 1.0f, 1.0f),   // Cool white
        0.6f,                             // Softer
        15.0f                             // Range
    ));

    // Back light (rim light)
    AP_AddLight(AP_LightPoint(
        AP_V3(-2.0f, 2.0f, -3.0f),       // Behind object
        AP_C4(0.5f, 0.5f, 0.6f, 1.0f),   // Subtle
        0.3f,
        10.0f
    ));

    // Ambient
    AP_SetAmbientLight(AP_C4(0.2f, 0.2f, 0.25f, 1.0f));
}
```

### Studio Lighting

```c
void SetupStudioLighting(void) {
    // Bright directional for product shots
    AP_AddLight(AP_LightDirectional(
        AP_V3(-0.7f, -0.7f, 0.0f),
        AP_C4(1.0f, 1.0f, 1.0f, 1.0f),
        1.5f
    ));

    // Bright fill light
    AP_AddLight(AP_LightPoint(
        AP_V3(3.0f, 2.0f, 2.0f),
        AP_C4(1.0f, 1.0f, 1.0f, 1.0f),
        1.0f,
        20.0f
    ));

    // Minimal ambient (high contrast)
    AP_SetAmbientLight(AP_C4(0.05f, 0.05f, 0.05f, 1.0f));
}
```

### Outdoor Lighting

```c
void SetupOutdoorLighting(float time_of_day) {
    // Simulate sun movement
    // time_of_day: 0.0 = sunrise, 0.5 = noon, 1.0 = sunset

    float sun_angle = time_of_day * 3.14159f;
    float sun_x = sinf(sun_angle);
    float sun_y = cosf(sun_angle) * 0.7f + 0.3f;  // Not straight down

    AP_Color sun_color;
    if (time_of_day < 0.25f) {
        // Sunrise: warm orange
        sun_color = AP_C4(1.0f, 0.7f, 0.4f, 1.0f);
    } else if (time_of_day < 0.75f) {
        // Noon: bright white
        sun_color = AP_C4(1.0f, 1.0f, 0.95f, 1.0f);
    } else {
        // Sunset: warm orange
        sun_color = AP_C4(1.0f, 0.5f, 0.2f, 1.0f);
    }

    AP_AddLight(AP_LightDirectional(
        AP_V3(-sun_x, -sun_y, 0.0f),
        sun_color,
        1.5f
    ));

    // Sky ambient
    AP_SetAmbientLight(AP_C4(0.4f, 0.5f, 0.6f, 1.0f));
}
```

## Optimization Techniques

### Material Atlasing

```c
typedef struct {
    AP_Texture *atlas;
    struct {
        int x, y, width, height;
    } regions[MATERIAL_COUNT];
} MaterialAtlas;

// Benefits:
// - Fewer texture bindings (faster)
// - Better cache locality
// - Easier streaming
```

### Dynamic Material Variants

```c
AP_Material *CreateVariant(AP_Material *base, const char *variant_name) {
    AP_Material *variant = AP_CreateMaterial(variant_name, base->type);
    *variant = *base;  // Copy properties
    return variant;
}

// Usage:
AP_Material *wet_steel = CreateVariant(steel, "Wet Steel");
wet_steel->roughness = 0.15f;  // Smoother when wet
wet_steel->metallic = 0.95f;   // Shinier
```

### Texture Resolution Strategy

```c
// Use different resolution based on distance/importance
//
// High priority (player character):
//   - 2048x2048 diffuse
//   - 2048x2048 normal
//   - 1024x1024 roughness/metallic
//
// Medium priority (interactive objects):
//   - 1024x1024 diffuse
//   - 1024x1024 normal
//   - 512x512 roughness/metallic
//
// Low priority (background):
//   - 512x512 diffuse
//   - 512x512 normal (optional)
```

## Real-World Examples

### Car Paint Material

```c
AP_Material *CreateCarPaint(void) {
    AP_Material *paint = AP_CreateMaterial("Car Paint", AP_MATERIAL_TYPE_PBR_METALLIC_ROUGHNESS);

    // Deep, rich color
    paint->base_color = AP_C4(0.15f, 0.0f, 0.03f, 1.0f);  // Deep red

    // Metallic with clear coat
    paint->metallic = 0.9f;      // Metal flakes
    paint->roughness = 0.05f;    // Glossy clear coat

    // Slight emission for night scenes
    paint->emissive_factor = AP_C4(0.05f, 0.0f, 0.0f, 1.0f);

    // Load texture map
    AP_Texture *normal = AP_LoadTexture("assets/car_paint_normal.png");
    if (normal) {
        AP_MaterialSetNormalTexture(paint, normal->id, 0.8f);
    }

    return paint;
}
```

### Human Skin

```c
AP_Material *CreateSkinMaterial(const char *tone) {
    AP_Material *skin = AP_CreateMaterial(tone, AP_MATERIAL_TYPE_PBR_METALLIC_ROUGHNESS);

    // Natural skin color (linear space)
    if (strcmp(tone, "Fair") == 0) {
        skin->base_color = AP_C4(0.96f, 0.80f, 0.70f, 1.0f);
    } else if (strcmp(tone, "Medium") == 0) {
        skin->base_color = AP_C4(0.75f, 0.60f, 0.50f, 1.0f);
    } else if (strcmp(tone, "Dark") == 0) {
        skin->base_color = AP_C4(0.35f, 0.25f, 0.15f, 1.0f);
    }

    // Skin is not metallic but has subsurface scattering
    skin->metallic = 0.0f;
    skin->roughness = 0.4f;  // Slightly rough

    // Normal map for skin pores, wrinkles
    AP_Texture *normal = AP_LoadTexture("assets/skin_normal.png");
    if (normal) {
        AP_MaterialSetNormalTexture(skin, normal->id, 1.2f);  // Slight exaggeration
    }

    return skin;
}
```

### Weathered Wood

```c
AP_Material *CreateWeatheredWood(void) {
    AP_Material *wood = AP_CreateMaterial("Weathered Wood", AP_MATERIAL_TYPE_PBR_METALLIC_ROUGHNESS);

    // Faded, grayish wood
    wood->base_color = AP_C4(0.45f, 0.40f, 0.35f, 1.0f);

    // Non-metallic
    wood->metallic = 0.0f;

    // Very rough from weathering
    wood->roughness = 0.85f;

    // Load comprehensive textures
    AP_Texture *diffuse = AP_LoadTexture("assets/weathered_wood_diffuse.png");
    AP_Texture *normal = AP_LoadTexture("assets/weathered_wood_normal.png");
    AP_Texture *roughness = AP_LoadTexture("assets/weathered_wood_roughness.png");
    AP_Texture *ao = AP_LoadTexture("assets/weathered_wood_ao.png");

    if (diffuse) AP_MaterialSetBaseColorTexture(wood, diffuse->id);
    if (normal) AP_MaterialSetNormalTexture(wood, normal->id, 1.0f);
    if (roughness) AP_MaterialSetMetallicRoughnessTexture(wood, roughness->id);
    if (ao) AP_MaterialSetOcclusionTexture(wood, ao->id, 0.8f);

    return wood;
}
```

## Summary

| Property | Range | Interpretation |
|----------|-------|-----------------|
| **Metallic** | [0, 1] | 0 = dielectric, 1 = conductor |
| **Roughness** | [0, 1] | 0 = mirror, 1 = diffuse |
| **Roughness** | 0.0-0.1 | Polished, reflective |
| **Roughness** | 0.1-0.3 | Smooth, brushed |
| **Roughness** | 0.3-0.7 | Medium, most objects |
| **Roughness** | 0.7-1.0 | Rough, matte |

## See Also

- [Materials and Textures](14-materials-textures.md)
- [3D Models and glTF Loading](15-gltf-models.md)
- [3D Rendering Guide](08-3d.md)
