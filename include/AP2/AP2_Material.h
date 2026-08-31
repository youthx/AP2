#ifndef AP2_MATERIAL_H
#define AP2_MATERIAL_H

#ifdef __cplusplus
extern "C" {
#endif

#include "AP2_Types.h"
#include <stdbool.h>

/* -------------------------------------------------------------------------- */
/* Material types / workflows                                                 */
/* -------------------------------------------------------------------------- */

typedef enum AP_MaterialType {
  AP_MATERIAL_TYPE_PBR_METALLIC_ROUGHNESS =
      0,                                    /* Standard glTF / most engines */
  AP_MATERIAL_TYPE_PBR_SPECULAR_GLOSSINESS, /* Older glTF extension */
  AP_MATERIAL_TYPE_UNLIT,                   /* Simple unlit / emissive only */
  AP_MATERIAL_TYPE_CUSTOM                   /* User-defined / extension */
} AP_MaterialType;

/* -------------------------------------------------------------------------- */
/* Core material structure                                                    */
/* -------------------------------------------------------------------------- */

typedef struct AP_Material {
  /* Identification */
  const char *name;
  AP_MaterialType type;

  /* ----- Base / Albedo -------------------------------------------------- */
  AP_Color base_color;        /* RGBA, linear */
  AP_UInt base_color_texture; /* Texture handle (0 = none) */
  bool has_base_color_texture;

  /* ----- Metallic-Roughness (PBR) --------------------------------------- */
  AP_F32 metallic;  /* 0.0 – 1.0 */
  AP_F32 roughness; /* 0.0 – 1.0 */
  AP_UInt
      metallic_roughness_texture; /* R = unused, G = roughness, B = metallic */
  bool has_metallic_roughness_texture;

  /* ----- Specular-Glossiness (legacy) ----------------------------------- */
  AP_Color specular_factor; /* RGB */
  AP_F32 glossiness_factor; /* 0.0 – 1.0 */
  AP_UInt specular_glossiness_texture;
  bool has_specular_glossiness_texture;

  /* ----- Normal mapping ------------------------------------------------- */
  AP_UInt normal_texture;
  AP_F32 normal_scale; /* Usually 1.0 */
  bool has_normal_texture;

  /* ----- Occlusion ------------------------------------------------------ */
  AP_UInt occlusion_texture; /* R channel */
  AP_F32 occlusion_strength; /* 0.0 – 1.0 */
  bool has_occlusion_texture;

  /* ----- Emissive ------------------------------------------------------- */
  AP_Color emissive_factor; /* RGB, can be > 1.0 for HDR */
  AP_UInt emissive_texture;
  bool has_emissive_texture;

  /* ----- Alpha / Transparency ------------------------------------------- */
  AP_F32 alpha_cutoff; /* Used with MASK mode */
  enum {
    AP_ALPHA_MODE_OPAQUE = 0,
    AP_ALPHA_MODE_MASK,
    AP_ALPHA_MODE_BLEND
  } alpha_mode;
  bool double_sided;

  /* ----- Optional extensions / extra data ------------------------------- */
  void *user_data; /* For custom materials */
} AP_Material;

/* -------------------------------------------------------------------------- */
/* Creation / Destruction                                                     */
/* -------------------------------------------------------------------------- */

AP_Material *AP_CreateMaterial(const char *name, AP_MaterialType type);
void AP_DestroyMaterial(AP_Material *material);

/* -------------------------------------------------------------------------- */
/* Initialization helpers                                                     */
/* -------------------------------------------------------------------------- */

void AP_MaterialInitPbrMetallicRoughness(AP_Material *mat, AP_Color base_color,
                                         AP_F32 metallic, AP_F32 roughness);

void AP_MaterialInitUnlit(AP_Material *mat, AP_Color color);

/* -------------------------------------------------------------------------- */
/* Texture setters (automatically set the corresponding has_* flag)           */
/* -------------------------------------------------------------------------- */

void AP_MaterialSetBaseColorTexture(AP_Material *mat, AP_UInt texture);
void AP_MaterialSetMetallicRoughnessTexture(AP_Material *mat, AP_UInt texture);
void AP_MaterialSetNormalTexture(AP_Material *mat, AP_UInt texture,
                                 AP_F32 scale);
void AP_MaterialSetOcclusionTexture(AP_Material *mat, AP_UInt texture,
                                    AP_F32 strength);
void AP_MaterialSetEmissiveTexture(AP_Material *mat, AP_UInt texture);
void AP_MaterialSetSpecularGlossinessTexture(AP_Material *mat, AP_UInt texture);

/* -------------------------------------------------------------------------- */
/* Utility                                                                    */
/* -------------------------------------------------------------------------- */

bool AP_MaterialIsTransparent(const AP_Material *mat);
bool AP_MaterialNeedsAlphaBlend(const AP_Material *mat);
bool AP_MaterialIsDoubleSided(const AP_Material *mat);

#ifdef __cplusplus
}
#endif

#endif /* AP2_MATERIAL_H */