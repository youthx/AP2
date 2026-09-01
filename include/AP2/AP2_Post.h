/*
 * AP2 — Application Primitives
 * Copyright (c) 2024-2026 Jack Waechter
 *
 * Licensed under the MIT License.
 * See LICENSE in the project root for full terms.
 */

#ifndef AP2_POST_H
#define AP2_POST_H

#include "AP2/AP2_Types.h"

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

  typedef struct AP_Shader AP_Shader;

  /*
   * AP2 Post-processing
   *
   * When enabled, AP_Clear() captures the frame offscreen.
   * Effects run at AP_Present(). The GUI layer is separate:
   *
   *     AP_SetPostEnabled(true);
   *     AP_SetPostVignette(0.4f);
   *     AP_SetGuiLayer(AP_GUI_LAYER_OVERLAY);  // UI not processed
   *
   *     AP_SetDrawColor(0.1f, 0.1f, 0.1f, 1.0f);
   *     AP_Clear();
   *     // scene / 3D
   *     AP_GuiBeginPanel("Settings", 24.0f, 24.0f, 280.0f, 400.0f);
   *     AP_GuiEndPanel();
   *     AP_Present();
   *
   * AP_GUI_LAYER_OFF      skip GUI draw and input capture
   * AP_GUI_LAYER_SCENE    GUI is in the scene (affected by post)
   * AP_GUI_LAYER_OVERLAY  GUI is composited after post (default, and what you want)
   *
   * Exclude with AP2_NO_POST.
   */

  typedef enum AP_PostFlags
  {
    AP_POST_NONE = 0,
    AP_POST_VIGNETTE = 1u << 0,
    AP_POST_BLOOM = 1u << 1,
    AP_POST_COLOR_GRADE = 1u << 2,
    AP_POST_CHROMATIC = 1u << 3,
    AP_POST_GRAIN = 1u << 4,
    AP_POST_SHARPEN = 1u << 5,
    AP_POST_CUSTOM = 1u << 6
  } AP_PostFlags;

  typedef struct AP_PostConfig
  {
    bool enabled;
    uint32_t flags;

    float vignette;
    float bloom_threshold;
    float bloom_intensity;
    float saturation;
    float contrast;
    float brightness;
    float chromatic;
    float grain;
    float sharpen;

    /*
     * Real hardware multisampling for the offscreen scene/GUI capture used
     * while post-processing is enabled. 0 disables; otherwise clamped to
     * GL_MAX_SAMPLES. Independent of AP_WINDOW_MSAA, which only
     * antialiases direct-to-backbuffer rendering (post disabled).
     */
    int msaa_samples;

    AP_Shader *custom;
  } AP_PostConfig;

  AP_PostConfig AP_PostDefaultConfig(void);

  bool AP_SetPostConfig(const AP_PostConfig *config);

  AP_PostConfig *AP_GetPostConfig(void);

  bool AP_SetPostEnabled(bool enabled);

  bool AP_PostEnabled(void);

  bool AP_SetPostFlags(uint32_t flags);

  uint32_t AP_GetPostFlags(void);

  bool AP_EnablePostFlag(AP_PostFlags flag);

  bool AP_DisablePostFlag(AP_PostFlags flag);

  bool AP_PostFlagEnabled(AP_PostFlags flag);

  bool AP_SetPostVignette(float amount);

  bool AP_SetPostBloom(float threshold, float intensity);

  bool AP_SetPostColorGrade(float saturation, float contrast, float brightness);

  bool AP_SetPostChromatic(float amount);

  bool AP_SetPostGrain(float amount);

  bool AP_SetPostSharpen(float amount);

  /* Antialiases the 3D/2D scene and captured GUI while post is enabled. */
  bool AP_SetPostMSAA(int samples);

  int AP_GetPostMSAA(void);

  bool AP_SetPostShader(AP_Shader *shader);

  AP_Shader *AP_GetPostShader(void);

  /*
   * When true, GUI is drawn into the scene target and is processed.
   * When false, GUI is an overlay after post. No-op if the GUI
   * layer is AP_GUI_LAYER_OFF.
   */
  void AP_SetPostIncludeGui(bool include);

  bool AP_PostIncludeGui(void);

#ifdef __cplusplus
}
#endif

#endif /* AP2_POST_H */
