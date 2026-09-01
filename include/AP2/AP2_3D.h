/*
 * AP2 — Application Primitives
 * Copyright (c) 2024-2026 Jack Waechter
 *
 * Licensed under the MIT License.
 * See LICENSE in the project root for full terms.
 */

#ifndef AP2_3D_H
#define AP2_3D_H

#include "AP2/AP2_Camera.h"
#include "AP2/AP2_Material.h"
#include "AP2/AP2_Math.h"
#include "AP2/AP2_Types.h"

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * AP2 3D
 *
 * Optional immediate 3D pass on top of the 2D renderer. World
 * space is right-handed and Y-up. Call AP_Begin3D() after
 * AP_Clear(), draw, then AP_End3D() before 2D UI. The camera
 * type lives in AP2_Camera.h (2D and 3D).
 *
 *     AP_Camera cam = AP_CameraPerspective(
 *         AP_V3(0.0f, 4.0f, 8.0f), AP_V3(0.0f, 0.0f, 0.0f), 50.0f);
 *
 *     AP_Begin3D(&cam);
 *     AP_AddLight(AP_LightPoint(AP_V3(2.0f, 3.0f, 1.0f),
 *                               AP_C4(1.0f, 0.8f, 0.5f, 1.0f), 2.0f, 12.0f));
 *     AP_DrawCube(AP_V3(0.0f, 0.5f, 0.0f), AP_V3(1.0f, 1.0f, 1.0f),
 *                 AP_C4(0.9f, 0.4f, 0.2f, 1.0f));
 *     AP_End3D();
 *
 * Exclude with AP2_NO_3D. Requires math, renderer, and shader.
 *
 * There is no scene graph. Draw it this frame or it isn't there.
 *
 * Transforms
 * ----------
 * Immediate pass:
 *   AP_Set3DModel / AP_Set3DTransform / AP_Translate3D / ...
 *   AP_DrawMesh / AP_DrawMeshEx / AP_DrawMeshTRS
 *
 * Multi-mesh models:
 *   AP_LoadModel / AP_ModelSetTRS / AP_DrawModel / AP_DrawModelTRS
 */

typedef struct AP_Mesh AP_Mesh;
typedef struct AP_Model AP_Model;
typedef struct AP_Material AP_Material;
typedef struct AP_Texture AP_Texture;

typedef struct AP_Vertex3 {
  AP_Vec3 position;
  AP_Vec3 normal;
  AP_Vec2 uv;
  AP_Color color;
} AP_Vertex3;

/* =========================================================
 * Mesh
 * ========================================================= */

AP_Mesh *AP_CreateMesh(const AP_Vertex3 *vertices, int vertex_count,
                       const AP_U32 *indices, int index_count);

AP_Mesh *AP_LoadMesh(const char *path);

AP_Mesh *AP_CreateMeshCube(AP_F32 size);

AP_Mesh *AP_CreateMeshPlane(AP_F32 width, AP_F32 depth);

AP_Mesh *AP_CreateMeshSphere(AP_F32 radius, int slices, int stacks);

void AP_DestroyMesh(AP_Mesh *mesh);

bool AP_MeshIsValid(const AP_Mesh *mesh);

int AP_MeshVertexCount(const AP_Mesh *mesh);

int AP_MeshIndexCount(const AP_Mesh *mesh);

/* Material access (ownership remains with mesh/model) */
AP_Material *AP_MeshGetMaterial(const AP_Mesh *mesh);

bool AP_MeshSetMaterial(AP_Mesh *mesh, AP_Material *material);

/* =========================================================
 * Model (multi-mesh + root / per-mesh transforms)
 * ========================================================= */

AP_Model *AP_CreateModel(void);

AP_Model *AP_LoadModel(const char *path);

void AP_DestroyModel(AP_Model *model);

bool AP_ModelIsValid(const AP_Model *model);

int AP_ModelMeshCount(const AP_Model *model);

AP_Mesh *AP_ModelGetMesh(const AP_Model *model, int index);

/* Material and texture access (ownership remains with model) */
AP_Material *AP_ModelGetMaterial(const AP_Model *model, int index);

int AP_ModelGetMaterialCount(const AP_Model *model);

AP_Texture *AP_ModelGetTexture(const AP_Model *model, int index);

int AP_ModelGetTextureCount(const AP_Model *model);

/* Root transform (model → world) */
bool AP_ModelSetTransform(AP_Model *model, const AP_Mat4 *transform);

AP_Mat4 AP_ModelGetTransform(const AP_Model *model);

bool AP_ModelSetTRS(AP_Model *model, AP_Vec3 position, AP_Quat rotation,
                    AP_Vec3 scale);

bool AP_ModelSetPosition(AP_Model *model, AP_Vec3 position);

bool AP_ModelTranslate(AP_Model *model, AP_Vec3 delta);

bool AP_ModelRotate(AP_Model *model, AP_Vec3 axis, AP_F32 degrees);

bool AP_ModelScale(AP_Model *model, AP_Vec3 scale);

bool AP_ModelResetTransform(AP_Model *model);

/* Per-mesh local transforms (mesh → model) */
bool AP_ModelSetMeshTransform(AP_Model *model, int mesh_index,
                              const AP_Mat4 *local);

AP_Mat4 AP_ModelGetMeshTransform(const AP_Model *model, int mesh_index);

bool AP_ModelSetMeshTRS(AP_Model *model, int mesh_index, AP_Vec3 position,
                        AP_Quat rotation, AP_Vec3 scale);

/* =========================================================
 * Lights
 *
 * Up to AP_3D_LIGHT_MAX lights of any mix of types. Add returns
 * a stable index. Disabled slots stay reserved until removed.
 *
 * Directional: direction points toward the scene (light travels
 *              along -direction).
 * Point:       position + range falloff.
 * Spot:        position, aim direction, range, and cone angles
 *              in degrees (inner / outer).
 * Ambient:     adds color * intensity with no direction.
 * ========================================================= */

#define AP_3D_LIGHT_MAX 16

typedef enum AP_LightType {
  AP_LIGHT_DISABLED = 0,
  AP_LIGHT_DIRECTIONAL,
  AP_LIGHT_POINT,
  AP_LIGHT_SPOT,
  AP_LIGHT_AMBIENT
} AP_LightType;

typedef struct AP_Light {
  AP_LightType type;
  bool enabled;
  AP_Vec3 position;
  AP_Vec3 direction;
  AP_Color color;
  AP_F32 intensity;
  AP_F32 range;
  AP_F32 inner_cone;
  AP_F32 outer_cone;
} AP_Light;

AP_Light AP_LightDirectional(AP_Vec3 direction, AP_Color color,
                             AP_F32 intensity);

AP_Light AP_LightPoint(AP_Vec3 position, AP_Color color, AP_F32 intensity,
                       AP_F32 range);

AP_Light AP_LightSpot(AP_Vec3 position, AP_Vec3 direction, AP_Color color,
                      AP_F32 intensity, AP_F32 range, AP_F32 inner_cone,
                      AP_F32 outer_cone);

AP_Light AP_LightAmbient(AP_Color color, AP_F32 intensity);

/*
 * Adds a light. Returns 0 .. AP_3D_LIGHT_MAX-1, or -1 if full.
 */
int AP_AddLight(AP_Light light);

bool AP_SetLight(int index, AP_Light light);

bool AP_GetLight(int index, AP_Light *out);

bool AP_EnableLight(int index, bool enabled);

bool AP_RemoveLight(int index);

void AP_ClearLights(void);

int AP_GetLightCount(void);

bool AP_SetAmbientLight(AP_Color color);

AP_Color AP_GetAmbientLight(void);

/*
 * Convenience: one directional light plus scene ambient.
 * Replaces the first directional light, or adds one.
 */
bool AP_Set3DLight(AP_Vec3 direction, AP_Color color, AP_F32 ambient);

/* =========================================================
 * Immediate 3D pass
 * ========================================================= */

bool AP_Begin3D(const AP_Camera *camera);

void AP_End3D(void);

bool AP_Is3D(void);

bool AP_Set3DModel(const AP_Mat4 *model);

AP_Mat4 AP_Get3DModel(void);

void AP_Reset3DModel(void);

/* TRS / incremental helpers for the current immediate model matrix */
bool AP_Set3DTransform(AP_Vec3 position, AP_Quat rotation, AP_Vec3 scale);

bool AP_Set3DPosition(AP_Vec3 position);

bool AP_Set3DRotation(AP_Quat rotation);

bool AP_Set3DScale(AP_Vec3 scale);

bool AP_Translate3D(AP_Vec3 delta);

bool AP_Rotate3D(AP_Vec3 axis, AP_F32 degrees);

bool AP_Scale3D(AP_Vec3 scale);

bool AP_Set3DTexture(AP_Texture *texture);

bool AP_Set3DTint(AP_Color tint);

bool AP_Set3DShininess(AP_F32 shininess);

bool AP_Set3DSpecular(AP_F32 strength);

bool AP_Set3DDepthTest(bool enabled);

bool AP_Set3DCullFace(bool enabled);

/* =========================================================
 * Draw
 * ========================================================= */

bool AP_DrawMesh(const AP_Mesh *mesh);

bool AP_DrawMeshEx(const AP_Mesh *mesh, const AP_Mat4 *model, AP_Color tint);

bool AP_DrawMeshTRS(const AP_Mesh *mesh, AP_Vec3 position, AP_Quat rotation,
                    AP_Vec3 scale, AP_Color tint);

bool AP_DrawModel(const AP_Model *model);

bool AP_DrawModelEx(const AP_Model *model, const AP_Mat4 *world_override,
                    AP_Color tint);

bool AP_DrawModelTRS(const AP_Model *model, AP_Vec3 position, AP_Quat rotation,
                     AP_Vec3 scale, AP_Color tint);

bool AP_DrawCube(AP_Vec3 center, AP_Vec3 size, AP_Color color);

bool AP_DrawPlane(AP_Vec3 center, AP_F32 width, AP_F32 depth, AP_Color color);

bool AP_DrawSphere(AP_Vec3 center, AP_F32 radius, AP_Color color);

bool AP_DrawLine3D(AP_Vec3 a, AP_Vec3 b, AP_Color color);

bool AP_DrawGrid3D(AP_F32 size, int divisions, AP_Color color);

/* Internal shutdown (called by AP_Quit). */
void AP_3DShutdown(void);

#ifdef __cplusplus
}
#endif

#endif /* AP2_3D_H */
