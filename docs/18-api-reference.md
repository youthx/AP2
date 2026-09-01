# AP2 Material & 3D Graphics API Reference

## Material API

### Material Creation and Destruction

#### `AP_Material *AP_CreateMaterial(const char *name, AP_MaterialType type)`

Creates a new material with the specified type.

**Parameters:**
- `name`: Material name for identification (can be NULL)
- `type`: Material type (PBR_METALLIC_ROUGHNESS, PBR_SPECULAR_GLOSSINESS, UNLIT, CUSTOM)

**Returns:** Pointer to new material, or NULL on failure

**Example:**
```c
AP_Material *mat = AP_CreateMaterial("Steel", AP_MATERIAL_TYPE_PBR_METALLIC_ROUGHNESS);
if (!mat) AP_WARN("Failed to create material");
```

#### `void AP_DestroyMaterial(AP_Material *material)`

Frees memory associated with a material.

**Parameters:**
- `material`: Material to destroy (NULL-safe)

**Example:**
```c
AP_DestroyMaterial(material);
material = NULL;
```

### Material Initialization Helpers

#### `void AP_MaterialInitPbrMetallicRoughness(AP_Material *mat, AP_Color base_color, AP_F32 metallic, AP_F32 roughness)`

Initialize a PBR metallic-roughness material with base properties.

**Parameters:**
- `mat`: Material to initialize
- `base_color`: Base surface color (linear RGB)
- `metallic`: Metallic factor [0, 1]
- `roughness`: Roughness factor [0, 1]

**Example:**
```c
AP_MaterialInitPbrMetallicRoughness(material,
    AP_C4(0.9f, 0.1f, 0.1f, 1.0f),  // Red base
    0.0f,    // Not metallic
    0.5f);   // Medium roughness
```

#### `void AP_MaterialInitUnlit(AP_Material *mat, AP_Color color)`

Initialize an unlit material.

**Parameters:**
- `mat`: Material to initialize
- `color`: Unlit color

**Example:**
```c
AP_MaterialInitUnlit(emissive_mat, AP_C4(0.0f, 1.0f, 0.0f, 1.0f));  // Green neon
```

### Material Property Setters

#### `void AP_MaterialSetBaseColorTexture(AP_Material *mat, AP_UInt texture)`

Set the base color/albedo texture.

**Parameters:**
- `mat`: Target material
- `texture`: Texture handle (0 = none)

#### `void AP_MaterialSetMetallicRoughnessTexture(AP_Material *mat, AP_UInt texture)`

Set combined metallic-roughness texture (glTF format: R=unused, G=roughness, B=metallic).

#### `void AP_MaterialSetNormalTexture(AP_Material *mat, AP_UInt texture, AP_F32 scale)`

Set normal map with intensity scale.

**Parameters:**
- `scale`: Normal intensity [0, 2+]. 1.0 = default, < 1.0 = less detail, > 1.0 = more detail

#### `void AP_MaterialSetOcclusionTexture(AP_Material *mat, AP_UInt texture, AP_F32 strength)`

Set ambient occlusion texture.

**Parameters:**
- `strength`: AO strength [0, 1]

#### `void AP_MaterialSetEmissiveTexture(AP_Material *mat, AP_UInt texture)`

Set emissive/glow texture.

#### `void AP_MaterialSetSpecularGlossinessTexture(AP_Material *mat, AP_UInt texture)`

Set specular-glossiness texture (legacy workflow).

### Material Utility Functions

#### `bool AP_MaterialIsTransparent(const AP_Material *mat)`

Check if material has any transparency.

**Returns:** true if material has transparency (BLEND or MASK alpha mode, or base_color.a < 1.0)

#### `bool AP_MaterialNeedsAlphaBlend(const AP_Material *mat)`

Check if material requires alpha blending.

**Returns:** true if alpha_mode == AP_ALPHA_MODE_BLEND

#### `bool AP_MaterialIsDoubleSided(const AP_Material *mat)`

Check if material renders both sides.

**Returns:** true if double_sided flag is set

## Material Structure

```c
typedef struct AP_Material {
    const char *name;                      // Material name
    AP_MaterialType type;                  // Material type

    // Base/Albedo
    AP_Color base_color;                   // RGBA, linear
    AP_UInt base_color_texture;            // Texture handle
    bool has_base_color_texture;           // Has texture flag

    // Metallic-Roughness (PBR)
    AP_F32 metallic;                       // [0, 1]
    AP_F32 roughness;                      // [0, 1]
    AP_UInt metallic_roughness_texture;    // Combined texture
    bool has_metallic_roughness_texture;   // Has texture flag

    // Specular-Glossiness (legacy)
    AP_Color specular_factor;              // RGB
    AP_F32 glossiness_factor;              // [0, 1]
    AP_UInt specular_glossiness_texture;   // Texture
    bool has_specular_glossiness_texture;  // Has texture flag

    // Normal Mapping
    AP_UInt normal_texture;                // Normal map
    AP_F32 normal_scale;                   // Intensity
    bool has_normal_texture;               // Has texture flag

    // Occlusion
    AP_UInt occlusion_texture;             // AO map
    AP_F32 occlusion_strength;             // [0, 1]
    bool has_occlusion_texture;            // Has texture flag

    // Emissive
    AP_Color emissive_factor;              // RGB, can be > 1.0
    AP_UInt emissive_texture;              // Emissive map
    bool has_emissive_texture;             // Has texture flag

    // Alpha/Transparency
    AP_F32 alpha_cutoff;                   // [0, 1] for MASK mode
    int alpha_mode;                        // OPAQUE, MASK, BLEND
    bool double_sided;                     // Render both sides

    void *user_data;                       // Custom data
} AP_Material;
```

## Texture API

### Texture Loading and Creation

#### `AP_Texture *AP_LoadTexture(const char *path)`

Load texture from file (PNG, JPG, TGA, etc.).

**Parameters:**
- `path`: File path

**Returns:** Pointer to texture, or NULL on failure

**Example:**
```c
AP_Texture *diffuse = AP_LoadTexture("assets/textures/wood_diffuse.png");
if (!diffuse) AP_WARN("Failed to load texture");
```

#### `AP_Texture *AP_LoadTextureFromMemory(const void *data, int size)`

Load texture from memory buffer.

**Parameters:**
- `data`: Image data pointer
- `size`: Data size in bytes

**Returns:** Pointer to texture, or NULL on failure

#### `AP_Texture *AP_CreateTextureFromPixels(int width, int height, const void *pixels, int pitch)`

Create texture from raw pixel data.

**Parameters:**
- `width`: Texture width
- `height`: Texture height
- `pixels`: Raw RGBA pixel data
- `pitch`: Bytes per row (0 = tightly packed)

**Returns:** Pointer to texture, or NULL on failure

#### `void AP_DestroyTexture(AP_Texture *texture)`

Free texture memory.

**Parameters:**
- `texture`: Texture to destroy (NULL-safe)

### Texture Structure

```c
typedef struct AP_Texture {
    AP_U32 id;        // GPU handle
    int width;        // Texture width
    int height;       // Texture height
    int channels;     // Color channels (3 or 4)
} AP_Texture;
```

## Mesh API

### Mesh Creation and Destruction

#### `AP_Mesh *AP_CreateMesh(const AP_Vertex3 *vertices, int vertex_count, const AP_U32 *indices, int index_count)`

Create mesh from vertex and index data.

**Parameters:**
- `vertices`: Vertex array
- `vertex_count`: Number of vertices
- `indices`: Index array (NULL for non-indexed)
- `index_count`: Number of indices

**Returns:** Pointer to mesh, or NULL on failure

#### `AP_Mesh *AP_LoadMesh(const char *path)`

Load mesh from file.

**Parameters:**
- `path`: File path (glTF/glB)

**Returns:** Pointer to mesh, or NULL on failure

#### Primitive Mesh Functions

```c
AP_Mesh *AP_CreateMeshCube(AP_F32 size);
AP_Mesh *AP_CreateMeshPlane(AP_F32 width, AP_F32 depth);
AP_Mesh *AP_CreateMeshSphere(AP_F32 radius, int slices, int stacks);
```

#### `void AP_DestroyMesh(AP_Mesh *mesh)`

Free mesh memory. Cannot destroy builtin primitives.

### Mesh Queries

#### `bool AP_MeshIsValid(const AP_Mesh *mesh)`

Check if mesh is valid.

#### `int AP_MeshVertexCount(const AP_Mesh *mesh)`

Get number of vertices.

#### `int AP_MeshIndexCount(const AP_Mesh *mesh)`

Get number of indices.

### Mesh Material

#### `AP_Material *AP_MeshGetMaterial(const AP_Mesh *mesh)`

Get mesh's associated material (does not own).

**Returns:** Material pointer, or NULL

#### `bool AP_MeshSetMaterial(AP_Mesh *mesh, AP_Material *material)`

Set material for mesh (mesh does not take ownership).

**Parameters:**
- `mesh`: Target mesh
- `material`: Material to assign

**Returns:** true on success

## Model API

### Model Creation and Destruction

#### `AP_Model *AP_CreateModel(void)`

Create an empty model.

**Returns:** Pointer to model, or NULL on failure

#### `AP_Model *AP_LoadModel(const char *path)`

Load model from glTF/glB file with materials and textures.

**Parameters:**
- `path`: File path

**Returns:** Pointer to model, or NULL on failure

**Example:**
```c
AP_Model *model = AP_LoadModel("assets/models/character.glb");
if (!AP_ModelIsValid(model)) {
    AP_ERROR("Failed to load model");
    return;
}
```

#### `void AP_DestroyModel(AP_Model *model)`

Free model and all owned resources.

**Parameters:**
- `model`: Model to destroy (NULL-safe)

### Model Queries

#### `bool AP_ModelIsValid(const AP_Model *model)`

Check if model is valid.

#### `int AP_ModelMeshCount(const AP_Model *model)`

Get number of meshes.

#### `AP_Mesh *AP_ModelGetMesh(const AP_Model *model, int index)`

Get mesh by index.

**Parameters:**
- `index`: Mesh index [0, mesh_count)

**Returns:** Pointer to mesh, or NULL

#### `int AP_ModelGetMaterialCount(const AP_Model *model)`

Get number of materials.

#### `AP_Material *AP_ModelGetMaterial(const AP_Model *model, int index)`

Get material by index. Materials are owned by model.

**Parameters:**
- `index`: Material index [0, material_count)

**Returns:** Pointer to material, or NULL

#### `int AP_ModelGetTextureCount(const AP_Model *model)`

Get number of textures.

#### `AP_Texture *AP_ModelGetTexture(const AP_Model *model, int index)`

Get texture by index. Textures are owned by model.

**Parameters:**
- `index`: Texture index [0, texture_count)

**Returns:** Pointer to texture, or NULL

### Model Transforms

#### `bool AP_ModelSetTransform(AP_Model *model, const AP_Mat4 *transform)`

Set model's world transform.

#### `AP_Mat4 AP_ModelGetTransform(const AP_Model *model)`

Get model's current world transform.

#### `bool AP_ModelSetTRS(AP_Model *model, AP_Vec3 position, AP_Quat rotation, AP_Vec3 scale)`

Set transform from position, rotation, and scale.

#### `bool AP_ModelSetPosition(AP_Model *model, AP_Vec3 position)`

Set model position.

#### `bool AP_ModelTranslate(AP_Model *model, AP_Vec3 delta)`

Move model by offset.

#### `bool AP_ModelRotate(AP_Model *model, AP_Vec3 axis, AP_F32 degrees)`

Rotate model around axis.

#### `bool AP_ModelScale(AP_Model *model, AP_Vec3 scale)`

Scale model.

#### `bool AP_ModelResetTransform(AP_Model *model)`

Reset to identity transform.

### Per-Mesh Transforms

#### `bool AP_ModelSetMeshTransform(AP_Model *model, int mesh_index, const AP_Mat4 *local)`

Set mesh's local transform.

#### `AP_Mat4 AP_ModelGetMeshTransform(const AP_Model *model, int mesh_index)`

Get mesh's local transform.

#### `bool AP_ModelSetMeshTRS(AP_Model *model, int mesh_index, AP_Vec3 position, AP_Quat rotation, AP_Vec3 scale)`

Set mesh local transform from TRS.

## Drawing API

### Basic Drawing

#### `bool AP_DrawMesh(const AP_Mesh *mesh)`

Draw mesh with current material and transform.

#### `bool AP_DrawMeshEx(const AP_Mesh *mesh, const AP_Mat4 *model, AP_Color tint)`

Draw mesh with override transform and tint.

#### `bool AP_DrawMeshTRS(const AP_Mesh *mesh, AP_Vec3 position, AP_Quat rotation, AP_Vec3 scale, AP_Color tint)`

Draw mesh with TRS parameters.

#### `bool AP_DrawModel(const AP_Model *model)`

Draw entire model with all meshes and materials.

#### `bool AP_DrawModelEx(const AP_Model *model, const AP_Mat4 *world_override, AP_Color tint)`

Draw model with override transform.

#### `bool AP_DrawModelTRS(const AP_Model *model, AP_Vec3 position, AP_Quat rotation, AP_Vec3 scale, AP_Color tint)`

Draw model with TRS parameters.

## 3D State API

### 3D Pass Control

#### `bool AP_Begin3D(const AP_Camera *camera)`

Begin 3D rendering pass.

#### `void AP_End3D(void)`

End 3D rendering pass.

#### `bool AP_Is3D(void)`

Check if currently in 3D pass.

### 3D Transform

#### `bool AP_Set3DModel(const AP_Mat4 *model)`

Set current model matrix.

#### `AP_Mat4 AP_Get3DModel(void)`

Get current model matrix.

#### `void AP_Reset3DModel(void)`

Reset to identity.

#### `bool AP_Set3DTransform(AP_Vec3 position, AP_Quat rotation, AP_Vec3 scale)`

Set model matrix from TRS.

#### `bool AP_Set3DPosition(AP_Vec3 position)`

Set position only.

#### `bool AP_Set3DRotation(AP_Quat rotation)`

Set rotation only.

#### `bool AP_Set3DScale(AP_Vec3 scale)`

Set scale only.

#### `bool AP_Translate3D(AP_Vec3 delta)`

Translate by offset.

#### `bool AP_Rotate3D(AP_Vec3 axis, AP_F32 degrees)`

Rotate around axis.

#### `bool AP_Scale3D(AP_Vec3 scale)`

Scale by factor.

### 3D State

#### `bool AP_Set3DTexture(AP_Texture *texture)`

Set texture for next draw.

#### `bool AP_Set3DTint(AP_Color tint)`

Set color tint.

#### `bool AP_Set3DShininess(AP_F32 shininess)`

Set specular shininess.

#### `bool AP_Set3DSpecular(AP_F32 strength)`

Set specular strength.

#### `bool AP_Set3DDepthTest(bool enabled)`

Enable/disable depth testing.

#### `bool AP_Set3DCullFace(bool enabled)`

Enable/disable back-face culling.

## Constants

### Material Types

```c
#define AP_MATERIAL_TYPE_PBR_METALLIC_ROUGHNESS 0
#define AP_MATERIAL_TYPE_PBR_SPECULAR_GLOSSINESS 1
#define AP_MATERIAL_TYPE_UNLIT 2
#define AP_MATERIAL_TYPE_CUSTOM 3
```

### Alpha Modes

```c
#define AP_ALPHA_MODE_OPAQUE 0
#define AP_ALPHA_MODE_MASK 1
#define AP_ALPHA_MODE_BLEND 2
```

## Complete Example

```c
#include <AP2/AP2.h>

int main(void) {
    AP_Init(AP_INIT_ALL);
    AP_Window *w = AP_CreateWindow("API Demo", 1280, 720, AP_WINDOW_RESIZABLE);
    AP_SetActiveWindow(w);

    AP_Camera cam = AP_CameraPerspective(
        AP_V3(0, 2, 8), AP_V3(0, 0, 0), 60);

    // Load model with embedded materials and textures
    AP_Model *model = AP_LoadModel("model.glb");
    if (!AP_ModelIsValid(model)) {
        AP_WARN("Failed to load model");
        return 1;
    }

    // Create custom material
    AP_Material *mat = AP_CreateMaterial("Custom", AP_MATERIAL_TYPE_PBR_METALLIC_ROUGHNESS);
    AP_MaterialInitPbrMetallicRoughness(mat,
        AP_C4(1, 0, 0, 1), 0.5f, 0.4f);

    AP_Mesh *sphere = AP_CreateMeshSphere(1, 32, 32);
    AP_MeshSetMaterial(sphere, mat);

    while (AP_IsRunning()) {
        AP_ClearLights();
        AP_Fill(0.1f, 0.1f, 0.1f, 1);
        AP_PumpEvents();

        AP_Begin3D(&cam);

        AP_AddLight(AP_LightDirectional(
            AP_V3(-1, -1, -1), AP_C4(1, 1, 1, 1), 1.2f));
        AP_SetAmbientLight(AP_C4(0.3f, 0.3f, 0.3f, 1));

        AP_DrawModel(model);

        AP_Set3DPosition(AP_V3(4, 0, 0));
        AP_DrawMesh(sphere);

        AP_End3D();
        AP_Present();
    }

    AP_DestroyMesh(sphere);
    AP_DestroyMaterial(mat);
    AP_DestroyModel(model);
    AP_DestroyWindow(w);
    AP_Quit();

    return 0;
}
```

## See Also

- [Materials and Textures Tutorial](14-materials-textures.md)
- [3D Models and glTF Loading](15-gltf-models.md)
- [Advanced PBR Workflows](16-pbr-advanced.md)
- [Best Practices Guide](17-best-practices.md)
