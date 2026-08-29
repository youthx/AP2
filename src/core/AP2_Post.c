/*
 * AP2 — Application Primitives
 * Copyright (c) 2024-2026 Jack Waechter
 *
 * Licensed under the MIT License.
 * See LICENSE in the project root for full terms.
 */

#include "AP2/AP2_Post.h"
#include "AP2/AP2_Gui.h"

#include "AP2_Internal.h"

#include "AP2/AP2_Logger.h"
#include "AP2/AP2_Opengl.h"
#include "AP2/AP2_Renderer.h"
#include "AP2/AP2_Shader.h"
#include "AP2/AP2_Texture.h"
#include "AP2/AP2_Window.h"

#define GLFW_INCLUDE_NONE
#include <glad/gl.h>

#include <string.h>

typedef struct AP_PostState {
  AP_PostConfig config;
  AP_Texture *scene;
  AP_Texture *gui;
  AP_Texture *scratch;
  AP_Shader *builtin;
  int width;
  int height;
  bool capturing;
  bool applying;
  bool gui_active;
  bool gui_cleared;
} AP_PostState;

static AP_PostState g_post;
static bool g_config_ready = false;

static const char *AP_POST_FRAGMENT =
    "#version 330 core\n"
    "in vec4 v_color;\n"
    "in vec2 v_uv;\n"
    "uniform sampler2D u_texture;\n"
    "uniform vec2 u_resolution;\n"
    "uniform float u_time;\n"
    "uniform float u_vignette;\n"
    "uniform float u_bloom_threshold;\n"
    "uniform float u_bloom_intensity;\n"
    "uniform float u_saturation;\n"
    "uniform float u_contrast;\n"
    "uniform float u_brightness;\n"
    "uniform float u_chromatic;\n"
    "uniform float u_grain;\n"
    "uniform float u_sharpen;\n"
    "out vec4 frag_color;\n"
    "float hash(vec2 p) {\n"
    "  return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453);\n"
    "}\n"
    "void main() {\n"
    "  vec2 uv = v_uv;\n"
    "  vec2 texel = 1.0 / max(u_resolution, vec2(1.0));\n"
    "  vec2 from_center = uv - 0.5;\n"
    "  vec3 color;\n"
    "  if (u_chromatic > 0.0) {\n"
    "    vec2 shift = from_center * u_chromatic;\n"
    "    color.r = texture(u_texture, uv + shift).r;\n"
    "    color.g = texture(u_texture, uv).g;\n"
    "    color.b = texture(u_texture, uv - shift).b;\n"
    "  } else {\n"
    "    color = texture(u_texture, uv).rgb;\n"
    "  }\n"
    "  if (u_sharpen > 0.0) {\n"
    "    vec3 sharp = color * 5.0;\n"
    "    sharp -= texture(u_texture, uv + vec2(texel.x, 0.0)).rgb;\n"
    "    sharp -= texture(u_texture, uv - vec2(texel.x, 0.0)).rgb;\n"
    "    sharp -= texture(u_texture, uv + vec2(0.0, texel.y)).rgb;\n"
    "    sharp -= texture(u_texture, uv - vec2(0.0, texel.y)).rgb;\n"
    "    color = mix(color, sharp, u_sharpen);\n"
    "  }\n"
    "  if (u_bloom_intensity > 0.0) {\n"
    "    vec3 bloom = vec3(0.0);\n"
    "    bloom += texture(u_texture, uv + vec2(texel.x, 0.0) * 2.0).rgb;\n"
    "    bloom += texture(u_texture, uv - vec2(texel.x, 0.0) * 2.0).rgb;\n"
    "    bloom += texture(u_texture, uv + vec2(0.0, texel.y) * 2.0).rgb;\n"
    "    bloom += texture(u_texture, uv - vec2(0.0, texel.y) * 2.0).rgb;\n"
    "    bloom += texture(u_texture, uv + texel * 2.0).rgb;\n"
    "    bloom += texture(u_texture, uv - texel * 2.0).rgb;\n"
    "    bloom *= 0.16666667;\n"
    "    bloom = max(bloom - vec3(u_bloom_threshold), vec3(0.0));\n"
    "    color += bloom * u_bloom_intensity;\n"
    "  }\n"
    "  float gray = dot(color, vec3(0.2126, 0.7152, 0.0722));\n"
    "  color = mix(vec3(gray), color, u_saturation);\n"
    "  color = (color - 0.5) * u_contrast + 0.5;\n"
    "  color += u_brightness;\n"
    "  if (u_vignette > 0.0) {\n"
    "    float radius = dot(from_center, from_center);\n"
    "    color *= 1.0 - u_vignette * radius * 1.6;\n"
    "  }\n"
    "  if (u_grain > 0.0) {\n"
    "    float n = hash(uv * u_resolution + u_time);\n"
    "    color += (n - 0.5) * u_grain;\n"
    "  }\n"
    "  frag_color = vec4(color, 1.0) * v_color;\n"
    "}\n";

static void AP_PostEnsureConfig(void) {
  if (!g_config_ready) {
    g_post.config = AP_PostDefaultConfig();
    g_config_ready = true;
  }
}

static float AP_PostClampf(float value, float minimum, float maximum) {
  if (value < minimum) {
    return minimum;
  }
  if (value > maximum) {
    return maximum;
  }
  return value;
}

static void AP_PostDestroyTarget(AP_Texture **texture) {
  if (texture == NULL || *texture == NULL) {
    return;
  }

  if (AP_GetRenderTarget() == *texture) {
    AP_SetRenderTarget(NULL);
  }

  AP_DestroyTexture(*texture);
  *texture = NULL;
}

static void AP_PostDestroyTargets(void) {
  AP_PostDestroyTarget(&g_post.scene);
  AP_PostDestroyTarget(&g_post.gui);
  AP_PostDestroyTarget(&g_post.scratch);
  g_post.width = 0;
  g_post.height = 0;
}

static AP_Texture *AP_PostMakeTarget(int width, int height) {
  AP_Texture *texture =
      AP_CreateTextureWithAccess(width, height, AP_TEXTUREACCESS_TARGET);
  if (texture == NULL) {
    return NULL;
  }

  AP_SetTextureScaleMode(texture, AP_SCALEMODE_LINEAR);
  AP_SetTextureAddressMode(texture, AP_TEXTUREADDRESS_CLAMP);
  AP_SetTextureBlendMode(texture, AP_BLENDMODE_BLEND);
  return texture;
}

static bool AP_PostEnsureTargets(int width, int height) {
  if (width <= 0 || height <= 0) {
    return false;
  }

  if (g_post.scene != NULL && g_post.gui != NULL && g_post.scratch != NULL &&
      g_post.width == width && g_post.height == height) {
    return true;
  }

  AP_PostDestroyTargets();
  g_post.scene = AP_PostMakeTarget(width, height);
  g_post.gui = AP_PostMakeTarget(width, height);
  g_post.scratch = AP_PostMakeTarget(width, height);
  if (g_post.scene == NULL || g_post.gui == NULL || g_post.scratch == NULL) {
    AP_PostDestroyTargets();
    return false;
  }

  g_post.width = width;
  g_post.height = height;
  return true;
}

static bool AP_PostEnsureShader(void) {
  if (g_post.builtin != NULL) {
    return true;
  }

  g_post.builtin = AP_CreateShader(NULL, AP_POST_FRAGMENT);
  if (g_post.builtin == NULL) {
    AP_WARN("Post shader failed to compile; post-processing disabled");
    return false;
  }

  return true;
}

static uint32_t AP_PostActiveFlags(void) {
  uint32_t flags;

  AP_PostEnsureConfig();
  flags = g_post.config.flags;
  if (g_post.config.custom == NULL) {
    flags &= ~(uint32_t)AP_POST_CUSTOM;
  }
  return flags;
}

static bool AP_PostBlit(AP_Texture *source, AP_Shader *shader) {
  AP_Shader *previous;
  AP_BlendMode blend;
  bool ok;

  if (source == NULL) {
    return false;
  }

  previous = AP_GetShader();
  blend = AP_GetTextureBlendMode(source);
  AP_SetTextureBlendMode(source, AP_BLENDMODE_NONE);
  AP_UseShader(shader);

  if (shader != NULL) {
    uint32_t flags = AP_PostActiveFlags();
    AP_SetUniformF("u_time", (float)AP_GetTime());
    AP_SetUniformF("u_vignette",
                   (flags & (uint32_t)AP_POST_VIGNETTE) != 0
                       ? g_post.config.vignette
                       : 0.0f);
    AP_SetUniformF("u_bloom_threshold", g_post.config.bloom_threshold);
    AP_SetUniformF("u_bloom_intensity",
                   (flags & (uint32_t)AP_POST_BLOOM) != 0
                       ? g_post.config.bloom_intensity
                       : 0.0f);
    AP_SetUniformF("u_saturation",
                   (flags & (uint32_t)AP_POST_COLOR_GRADE) != 0
                       ? g_post.config.saturation
                       : 1.0f);
    AP_SetUniformF("u_contrast",
                   (flags & (uint32_t)AP_POST_COLOR_GRADE) != 0
                       ? g_post.config.contrast
                       : 1.0f);
    AP_SetUniformF("u_brightness",
                   (flags & (uint32_t)AP_POST_COLOR_GRADE) != 0
                       ? g_post.config.brightness
                       : 0.0f);
    AP_SetUniformF("u_chromatic",
                   (flags & (uint32_t)AP_POST_CHROMATIC) != 0
                       ? g_post.config.chromatic
                       : 0.0f);
    AP_SetUniformF("u_grain",
                   (flags & (uint32_t)AP_POST_GRAIN) != 0 ? g_post.config.grain
                                                            : 0.0f);
    AP_SetUniformF("u_sharpen",
                   (flags & (uint32_t)AP_POST_SHARPEN) != 0
                       ? g_post.config.sharpen
                       : 0.0f);
  }

  ok = AP_RenderTexture(source, NULL, NULL);
  AP_UseShader(previous);
  AP_SetTextureBlendMode(source, blend);
  return ok;
}

static void AP_PostApplyEffects(void) {
  uint32_t flags = AP_PostActiveFlags();
  AP_Texture *source = g_post.scene;
  bool used_builtin = false;

  if ((flags & (uint32_t)AP_POST_CUSTOM) != 0 && g_post.config.custom != NULL) {
    if (!AP_SetRenderTarget(g_post.scratch)) {
      AP_SetRenderTarget(NULL);
      AP_PostBlit(source, NULL);
      return;
    }

    AP_OpenGLSetClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    AP_OpenGLClear(true, false, false);
    if (AP_PostEnsureShader() && (flags & ~(uint32_t)AP_POST_CUSTOM) != 0) {
      AP_PostBlit(source, g_post.builtin);
      source = g_post.scratch;
      used_builtin = true;
    }

    AP_SetRenderTarget(NULL);
    AP_PostBlit(used_builtin ? g_post.scratch : source, g_post.config.custom);
    return;
  }

  AP_SetRenderTarget(NULL);
  if (AP_PostEnsureShader() && flags != 0) {
    AP_PostBlit(source, g_post.builtin);
  } else {
    AP_PostBlit(source, NULL);
  }
}

AP_PostConfig AP_PostDefaultConfig(void) {
  AP_PostConfig config;
  memset(&config, 0, sizeof(config));
  config.enabled = false;
  config.flags = AP_POST_NONE;
  config.vignette = 0.35f;
  config.bloom_threshold = 0.7f;
  config.bloom_intensity = 0.45f;
  config.saturation = 1.0f;
  config.contrast = 1.0f;
  config.brightness = 0.0f;
  config.chromatic = 0.0025f;
  config.grain = 0.04f;
  config.sharpen = 0.15f;
  config.custom = NULL;
  return config;
}

bool AP_SetPostConfig(const AP_PostConfig *config) {
  AP_PostEnsureConfig();
  if (config == NULL) {
    g_post.config = AP_PostDefaultConfig();
    return true;
  }

  g_post.config = *config;
  g_post.config.vignette = AP_PostClampf(g_post.config.vignette, 0.0f, 2.0f);
  g_post.config.bloom_threshold =
      AP_PostClampf(g_post.config.bloom_threshold, 0.0f, 1.0f);
  g_post.config.bloom_intensity =
      AP_PostClampf(g_post.config.bloom_intensity, 0.0f, 4.0f);
  g_post.config.saturation = AP_PostClampf(g_post.config.saturation, 0.0f, 2.0f);
  g_post.config.contrast = AP_PostClampf(g_post.config.contrast, 0.0f, 3.0f);
  g_post.config.brightness =
      AP_PostClampf(g_post.config.brightness, -1.0f, 1.0f);
  g_post.config.chromatic = AP_PostClampf(g_post.config.chromatic, 0.0f, 0.05f);
  g_post.config.grain = AP_PostClampf(g_post.config.grain, 0.0f, 0.5f);
  g_post.config.sharpen = AP_PostClampf(g_post.config.sharpen, 0.0f, 1.0f);
  return true;
}

AP_PostConfig *AP_GetPostConfig(void) {
  AP_PostEnsureConfig();
  return &g_post.config;
}

bool AP_SetPostEnabled(bool enabled) {
  AP_PostEnsureConfig();
  g_post.config.enabled = enabled;
  if (!enabled && g_post.capturing) {
    AP_SetRenderTarget(NULL);
    g_post.capturing = false;
    g_post.gui_active = false;
  }
  return true;
}

bool AP_PostEnabled(void) {
  AP_PostEnsureConfig();
  return g_post.config.enabled;
}

bool AP_SetPostFlags(uint32_t flags) {
  AP_PostEnsureConfig();
  g_post.config.flags = flags;
  return true;
}

uint32_t AP_GetPostFlags(void) {
  AP_PostEnsureConfig();
  return g_post.config.flags;
}

bool AP_EnablePostFlag(AP_PostFlags flag) {
  AP_PostEnsureConfig();
  g_post.config.flags |= (uint32_t)flag;
  return true;
}

bool AP_DisablePostFlag(AP_PostFlags flag) {
  AP_PostEnsureConfig();
  g_post.config.flags &= ~(uint32_t)flag;
  return true;
}

bool AP_PostFlagEnabled(AP_PostFlags flag) {
  AP_PostEnsureConfig();
  return (g_post.config.flags & (uint32_t)flag) != 0;
}

bool AP_SetPostVignette(float amount) {
  AP_PostEnsureConfig();
  g_post.config.vignette = AP_PostClampf(amount, 0.0f, 2.0f);
  if (amount > 0.0f) {
    AP_EnablePostFlag(AP_POST_VIGNETTE);
  }
  return true;
}

bool AP_SetPostBloom(float threshold, float intensity) {
  AP_PostEnsureConfig();
  g_post.config.bloom_threshold = AP_PostClampf(threshold, 0.0f, 1.0f);
  g_post.config.bloom_intensity = AP_PostClampf(intensity, 0.0f, 4.0f);
  if (intensity > 0.0f) {
    AP_EnablePostFlag(AP_POST_BLOOM);
  }
  return true;
}

bool AP_SetPostColorGrade(float saturation, float contrast, float brightness) {
  AP_PostEnsureConfig();
  g_post.config.saturation = AP_PostClampf(saturation, 0.0f, 2.0f);
  g_post.config.contrast = AP_PostClampf(contrast, 0.0f, 3.0f);
  g_post.config.brightness = AP_PostClampf(brightness, -1.0f, 1.0f);
  AP_EnablePostFlag(AP_POST_COLOR_GRADE);
  return true;
}

bool AP_SetPostChromatic(float amount) {
  AP_PostEnsureConfig();
  g_post.config.chromatic = AP_PostClampf(amount, 0.0f, 0.05f);
  if (amount > 0.0f) {
    AP_EnablePostFlag(AP_POST_CHROMATIC);
  }
  return true;
}

bool AP_SetPostGrain(float amount) {
  AP_PostEnsureConfig();
  g_post.config.grain = AP_PostClampf(amount, 0.0f, 0.5f);
  if (amount > 0.0f) {
    AP_EnablePostFlag(AP_POST_GRAIN);
  }
  return true;
}

bool AP_SetPostSharpen(float amount) {
  AP_PostEnsureConfig();
  g_post.config.sharpen = AP_PostClampf(amount, 0.0f, 1.0f);
  if (amount > 0.0f) {
    AP_EnablePostFlag(AP_POST_SHARPEN);
  }
  return true;
}

bool AP_SetPostShader(AP_Shader *shader) {
  AP_PostEnsureConfig();
  g_post.config.custom = shader;
  if (shader != NULL) {
    AP_EnablePostFlag(AP_POST_CUSTOM);
  } else {
    AP_DisablePostFlag(AP_POST_CUSTOM);
  }
  return true;
}

AP_Shader *AP_GetPostShader(void) {
  AP_PostEnsureConfig();
  return g_post.config.custom;
}

void AP_SetPostIncludeGui(bool include) {
  if (AP_GetGuiLayer() == AP_GUI_LAYER_OFF) {
    return;
  }

  AP_SetGuiLayer(include ? AP_GUI_LAYER_SCENE : AP_GUI_LAYER_OVERLAY);
}

bool AP_PostIncludeGui(void) {
  return AP_GetGuiLayer() == AP_GUI_LAYER_SCENE;
}

void AP_PostBeginFrame(void) {
  AP_Texture *current;
  int width = 0;
  int height = 0;

  AP_PostEnsureConfig();
  if (g_post.applying) {
    return;
  }

  g_post.gui_active = false;
  g_post.gui_cleared = false;

  if (!g_post.config.enabled) {
    g_post.capturing = false;
    return;
  }

  current = AP_GetRenderTarget();
  if (current != NULL && current != g_post.scene && current != g_post.gui) {
    g_post.capturing = false;
    return;
  }

  AP_GetWindowSizeInPixels(&width, &height);
  if (!AP_PostEnsureTargets(width, height)) {
    g_post.capturing = false;
    return;
  }

  if (!AP_SetRenderTarget(g_post.scene)) {
    g_post.capturing = false;
    return;
  }

  g_post.capturing = true;
}

bool AP_PostBeginGuiLayer(void) {
  AP_GuiLayer layer = AP_GetGuiLayer();

  if (layer == AP_GUI_LAYER_OFF) {
    return false;
  }

  if (!g_post.capturing || layer == AP_GUI_LAYER_SCENE) {
    return true;
  }

  if (g_post.gui == NULL) {
    return true;
  }

  if (!g_post.gui_active) {
    if (!AP_SetRenderTarget(g_post.gui)) {
      return true;
    }

    if (!g_post.gui_cleared) {
      AP_OpenGLSetClearColor(0.0f, 0.0f, 0.0f, 0.0f);
      AP_OpenGLClear(true, true, true);
      g_post.gui_cleared = true;
    }

    g_post.gui_active = true;
  }

  return true;
}

void AP_PostPresent(void) {
  if (!g_post.capturing) {
    return;
  }

  g_post.applying = true;
  AP_FlushRenderer();
  AP_PostApplyEffects();

  if (g_post.gui_active && g_post.gui != NULL) {
    AP_BlendMode blend = AP_GetTextureBlendMode(g_post.gui);
    AP_SetTextureBlendMode(g_post.gui, AP_BLENDMODE_BLEND);
    AP_RenderTexture(g_post.gui, NULL, NULL);
    AP_SetTextureBlendMode(g_post.gui, blend);
  }

  AP_FlushRenderer();
  g_post.applying = false;
  g_post.capturing = false;
  g_post.gui_active = false;
}

void AP_PostNotifyResize(int width, int height) {
  (void)width;
  (void)height;
  if (g_post.scene != NULL) {
    AP_PostDestroyTargets();
  }
}

void AP_PostShutdown(void) {
  if (g_post.builtin != NULL) {
    AP_ShaderDestroyInternal(g_post.builtin);
    g_post.builtin = NULL;
  }

  AP_PostDestroyTargets();
  g_post.capturing = false;
  g_post.applying = false;
  g_post.gui_active = false;
  g_post.gui_cleared = false;
  g_config_ready = false;
  memset(&g_post.config, 0, sizeof(g_post.config));
}
