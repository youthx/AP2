#include "AP2/AP2_Material.h"
#include <stdlib.h>
#include <string.h>

#define AP_MaterialCreate AP_CreateMaterial
#define AP_MaterialDestroy AP_DestroyMaterial

/* -------------------------------------------------------------------------- */
/* Internal helpers                                                           */
/* -------------------------------------------------------------------------- */

static void AP_MaterialZero(AP_Material *mat) {
  if (!mat)
    return;

  mat->name = NULL;
  mat->type = AP_MATERIAL_TYPE_PBR_METALLIC_ROUGHNESS;

  /* Base / Albedo */
  mat->base_color.r = 1.0f;
  mat->base_color.g = 1.0f;
  mat->base_color.b = 1.0f;
  mat->base_color.a = 1.0f;
  mat->base_color_texture = 0;
  mat->has_base_color_texture = false;

  /* Metallic-Roughness */
  mat->metallic = 0.0f;
  mat->roughness = 1.0f;
  mat->metallic_roughness_texture = 0;
  mat->has_metallic_roughness_texture = false;

  /* Specular-Glossiness */
  mat->specular_factor.r = 1.0f;
  mat->specular_factor.g = 1.0f;
  mat->specular_factor.b = 1.0f;
  mat->specular_factor.a = 1.0f; /* usually unused, but keep consistent */
  mat->glossiness_factor = 1.0f;
  mat->specular_glossiness_texture = 0;
  mat->has_specular_glossiness_texture = false;

  /* Normal */
  mat->normal_texture = 0;
  mat->normal_scale = 1.0f;
  mat->has_normal_texture = false;

  /* Occlusion */
  mat->occlusion_texture = 0;
  mat->occlusion_strength = 1.0f;
  mat->has_occlusion_texture = false;

  /* Emissive */
  mat->emissive_factor.r = 0.0f;
  mat->emissive_factor.g = 0.0f;
  mat->emissive_factor.b = 0.0f;
  mat->emissive_factor.a = 1.0f;
  mat->emissive_texture = 0;
  mat->has_emissive_texture = false;

  /* Alpha */
  mat->alpha_cutoff = 0.5f;
  mat->alpha_mode = AP_ALPHA_MODE_OPAQUE;
  mat->double_sided = false;

  mat->user_data = NULL;
}

/* -------------------------------------------------------------------------- */
/* Creation / Destruction                                                     */
/* -------------------------------------------------------------------------- */

AP_Material *AP_CreateMaterial(const char *name, AP_MaterialType type) {
  AP_Material *mat = (AP_Material *)calloc(1, sizeof(AP_Material));
  if (!mat)
    return NULL;

  AP_MaterialZero(mat);

  if (name) {
    size_t len = strlen(name) + 1;
    char *copy = (char *)malloc(len);
    if (copy) {
      memcpy(copy, name, len);
      mat->name = copy;
    }
  }

  mat->type = type;
  return mat;
}

void AP_DestroyMaterial(AP_Material *material) {
  if (!material)
    return;

  if (material->name) {
    free((void *)material->name);
    material->name = NULL;
  }

  /* user_data is owned by the caller */
  free(material);
}

/* -------------------------------------------------------------------------- */
/* Initialization helpers                                                     */
/* -------------------------------------------------------------------------- */

void AP_MaterialInitPbrMetallicRoughness(AP_Material *mat, AP_Color base_color,
                                         AP_F32 metallic, AP_F32 roughness) {
  if (!mat)
    return;

  mat->type = AP_MATERIAL_TYPE_PBR_METALLIC_ROUGHNESS;
  mat->base_color = base_color;
  mat->metallic = metallic;
  mat->roughness = roughness;

  /* Clear legacy specular-glossiness fields to defaults */
  mat->specular_factor.r = 1.0f;
  mat->specular_factor.g = 1.0f;
  mat->specular_factor.b = 1.0f;
  mat->glossiness_factor = 1.0f;
  mat->has_specular_glossiness_texture = false;
  mat->specular_glossiness_texture = 0;
}

void AP_MaterialInitUnlit(AP_Material *mat, AP_Color color) {
  if (!mat)
    return;

  mat->type = AP_MATERIAL_TYPE_UNLIT;
  mat->base_color = color;

  /* Unlit materials typically ignore metallic/roughness */
  mat->metallic = 0.0f;
  mat->roughness = 1.0f;
  mat->has_metallic_roughness_texture = false;
  mat->metallic_roughness_texture = 0;
}

/* -------------------------------------------------------------------------- */
/* Texture setters                                                            */
/* -------------------------------------------------------------------------- */

void AP_MaterialSetBaseColorTexture(AP_Material *mat, AP_UInt texture) {
  if (!mat)
    return;
  mat->base_color_texture = texture;
  mat->has_base_color_texture = (texture != 0);
}

void AP_MaterialSetMetallicRoughnessTexture(AP_Material *mat, AP_UInt texture) {
  if (!mat)
    return;
  mat->metallic_roughness_texture = texture;
  mat->has_metallic_roughness_texture = (texture != 0);
}

void AP_MaterialSetNormalTexture(AP_Material *mat, AP_UInt texture,
                                 AP_F32 scale) {
  if (!mat)
    return;
  mat->normal_texture = texture;
  mat->normal_scale = scale;
  mat->has_normal_texture = (texture != 0);
}

void AP_MaterialSetOcclusionTexture(AP_Material *mat, AP_UInt texture,
                                    AP_F32 strength) {
  if (!mat)
    return;
  mat->occlusion_texture = texture;
  mat->occlusion_strength = strength;
  mat->has_occlusion_texture = (texture != 0);
}

void AP_MaterialSetEmissiveTexture(AP_Material *mat, AP_UInt texture) {
  if (!mat)
    return;
  mat->emissive_texture = texture;
  mat->has_emissive_texture = (texture != 0);
}

void AP_MaterialSetSpecularGlossinessTexture(AP_Material *mat,
                                             AP_UInt texture) {
  if (!mat)
    return;
  mat->specular_glossiness_texture = texture;
  mat->has_specular_glossiness_texture = (texture != 0);
}

/* -------------------------------------------------------------------------- */
/* Utility                                                                    */
/* -------------------------------------------------------------------------- */

bool AP_MaterialIsTransparent(const AP_Material *mat) {
  if (!mat)
    return false;

  if (mat->alpha_mode == AP_ALPHA_MODE_BLEND)
    return true;

  if (mat->alpha_mode == AP_ALPHA_MODE_MASK)
    return true; /* masked materials are treated as needing special handling */

  /* OPAQUE but base color alpha < 1.0 is still considered transparent by some
   * engines */
  if (mat->base_color.a < 1.0f)
    return true;

  return false;
}

bool AP_MaterialNeedsAlphaBlend(const AP_Material *mat) {
  if (!mat)
    return false;
  return mat->alpha_mode == AP_ALPHA_MODE_BLEND;
}

bool AP_MaterialIsDoubleSided(const AP_Material *mat) {
  if (!mat)
    return false;
  return mat->double_sided;
}
