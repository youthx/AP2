/*
 * AP2 â€” Application Primitives
 * Copyright (c) 2024-2026 Jack Waechter
 *
 * Licensed under the MIT License.
 * See LICENSE in the project root for full terms.
 */

#include "AP2/AP2_Post.h"
#include "AP2/AP2_Gui.h"
#include "AP2/AP2_Post_extra.h"

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
} AP_PostState;

static AP_PostState g_post;
static bool g_config_ready = false;

typedef struct AP_PostExtra {
  /* Master */
  float intensity;

  /* Tone / color */
  float exposure;
  float gamma;
  float filmic;
  float contrast;
  float saturation;
  float brightness;
  float temperature;
  float tint;

  /* Enhancement */
  float sharpen;
  float clarity;
  float detail;

  /* Bloom / lens */
  float bloom_radius;
  float bloom_softness;
  float lens_dirt;
  float chromatic;
  float lens_distortion;
  float barrel;
  float vignette;

  /* Film / atmosphere */
  float grain;
  float film_grain;
  float film_response;
  float halation;
  float fog;
  float fog_density;
  float fog_height;

  /* Depth / motion approximations */
  float depth_of_field;
  float dof_focus;
  float dof_aperture;
  float motion_blur;

  /* Creative */
  float pixelate;
  float posterize;
  float scanlines;
  float crt;
  float vhs;
  float rgb_split;
  float sepia;
  float grayscale;
  float invert;
  float solarize;
  float colorize;
  float colorize_hue;
  float edge;
  float outline;
  float cel_shade;
  float dither;
  float halftone;
  float glitch;
  float noise;
  float displacement;
  float kaleidoscope;
  float wave;
  float ripple;
  float fisheye;
} AP_PostExtra;

static AP_PostExtra g_post_extra;
static bool g_post_extra_ready = false;

static void AP_PostEnsureExtra(void) {
  if (g_post_extra_ready) {
    return;
  }

  memset(&g_post_extra, 0, sizeof(g_post_extra));

  g_post_extra.intensity = 1.0f;
  g_post_extra.exposure = 1.0f;
  g_post_extra.gamma = 1.0f;
  g_post_extra.filmic = 0.35f;
  g_post_extra.contrast = 1.0f;
  g_post_extra.saturation = 1.0f;
  g_post_extra.brightness = 0.0f;

  g_post_extra.sharpen = 0.15f;
  g_post_extra.clarity = 0.0f;
  g_post_extra.detail = 0.0f;

  g_post_extra.bloom_radius = 4.0f;
  g_post_extra.bloom_softness = 0.5f;
  g_post_extra.lens_dirt = 0.0f;
  g_post_extra.vignette = 0.35f;

  g_post_extra.chromatic = 0.0025f;
  g_post_extra.lens_distortion = 0.0f;
  g_post_extra.barrel = 0.0f;

  g_post_extra.grain = 0.04f;
  g_post_extra.film_grain = 0.0f;
  g_post_extra.film_response = 0.0f;
  g_post_extra.halation = 0.0f;

  g_post_extra.fog = 0.0f;
  g_post_extra.fog_density = 0.0f;
  g_post_extra.fog_height = 0.0f;

  g_post_extra.depth_of_field = 0.0f;
  g_post_extra.dof_focus = 0.5f;
  g_post_extra.dof_aperture = 0.25f;
  g_post_extra.motion_blur = 0.0f;

  g_post_extra.pixelate = 0.0f;
  g_post_extra.posterize = 0.0f;
  g_post_extra.scanlines = 0.0f;
  g_post_extra.crt = 0.0f;
  g_post_extra.vhs = 0.0f;
  g_post_extra.rgb_split = 0.0f;

  g_post_extra.sepia = 0.0f;
  g_post_extra.grayscale = 0.0f;
  g_post_extra.invert = 0.0f;
  g_post_extra.solarize = 0.0f;
  g_post_extra.colorize = 0.0f;
  g_post_extra.colorize_hue = 0.0f;

  g_post_extra.edge = 0.0f;
  g_post_extra.outline = 0.0f;
  g_post_extra.cel_shade = 0.0f;
  g_post_extra.dither = 0.0f;
  g_post_extra.halftone = 0.0f;

  g_post_extra.glitch = 0.0f;
  g_post_extra.noise = 0.0f;
  g_post_extra.displacement = 0.0f;

  g_post_extra.kaleidoscope = 0.0f;
  g_post_extra.wave = 0.0f;
  g_post_extra.ripple = 0.0f;
  g_post_extra.fisheye = 0.0f;

  g_post_extra_ready = true;
}

/* Grain hash. Looks like numerology. Grain doesn't care. */
static const char *AP_POST_FRAGMENT =
    "#version 330 core\n"
    "in vec4 v_color;\n"
    "in vec2 v_uv;\n"
    "uniform sampler2D u_texture;\n"
    "uniform vec2 u_resolution;\n"
    "uniform float u_time;\n"
    "uniform float u_intensity;\n"
    "uniform float u_vignette;\n"
    "uniform float u_bloom_threshold;\n"
    "uniform float u_bloom_intensity;\n"
    "uniform float u_bloom_radius;\n"
    "uniform float u_bloom_softness;\n"
    "uniform float u_saturation;\n"
    "uniform float u_contrast;\n"
    "uniform float u_brightness;\n"
    "uniform float u_temperature;\n"
    "uniform float u_tint;\n"
    "uniform float u_chromatic;\n"
    "uniform float u_lens_distortion;\n"
    "uniform float u_barrel;\n"
    "uniform float u_grain;\n"
    "uniform float u_film_grain;\n"
    "uniform float u_film_response;\n"
    "uniform float u_filmic;\n"
    "uniform float u_halation;\n"
    "uniform float u_sharpen;\n"
    "uniform float u_clarity;\n"
    "uniform float u_detail;\n"
    "uniform float u_exposure;\n"
    "uniform float u_gamma;\n"
    "uniform float u_pixelate;\n"
    "uniform float u_scanlines;\n"
    "uniform float u_crt;\n"
    "uniform float u_vhs;\n"
    "uniform float u_rgb_split;\n"
    "uniform float u_sepia;\n"
    "uniform float u_grayscale;\n"
    "uniform float u_invert;\n"
    "uniform float u_solarize;\n"
    "uniform float u_colorize;\n"
    "uniform float u_colorize_hue;\n"
    "uniform float u_posterize;\n"
    "uniform float u_edge;\n"
    "uniform float u_outline;\n"
    "uniform float u_cel_shade;\n"
    "uniform float u_dither;\n"
    "uniform float u_halftone;\n"
    "uniform float u_glitch;\n"
    "uniform float u_noise;\n"
    "uniform float u_displacement;\n"
    "uniform float u_kaleidoscope;\n"
    "uniform float u_wave;\n"
    "uniform float u_ripple;\n"
    "uniform float u_fisheye;\n"
    "uniform float u_lens_dirt;\n"
    "uniform float u_fog;\n"
    "uniform float u_fog_density;\n"
    "uniform float u_fog_height;\n"
    "uniform float u_depth_of_field;\n"
    "uniform float u_dof_focus;\n"
    "uniform float u_dof_aperture;\n"
    "uniform float u_motion_blur;\n"
    "out vec4 frag_color;\n"
    "float hash(vec2 p) { return fract(sin(dot(p, vec2(127.1, 311.7))) * "
    "43758.5453); }\n"
    "float luminance(vec3 c) { return dot(c, vec3(0.2126, 0.7152, 0.0722)); }\n"
    "vec3 filmic_tonemap(vec3 x) { vec3 a=x*(2.51*x+0.03); vec3 "
    "b=x*(2.43*x+0.59)+0.14; return clamp(a/b,0.0,1.0); }\n"
    "vec3 hsv2rgb(vec3 c) { vec3 "
    "p=abs(fract(c.xxx+vec3(0.0,0.6666667,0.3333333))*6.0-3.0); return "
    "c.z*mix(vec3(1.0),clamp(p-1.0,0.0,1.0),c.y); }\n"
    "vec3 sample_rgb(vec2 uv, vec2 shift) { return vec3(texture(u_texture, "
    "uv+shift).r, texture(u_texture, uv).g, texture(u_texture, uv-shift).b); "
    "}\n"
    "vec3 blur9(vec2 uv, vec2 d) { vec3 c=texture(u_texture,uv).rgb*0.20; "
    "c+=texture(u_texture,uv+d).rgb*0.12; c+=texture(u_texture,uv-d).rgb*0.12; "
    "c+=texture(u_texture,uv+d*2.0).rgb*0.10; "
    "c+=texture(u_texture,uv-d*2.0).rgb*0.10; "
    "c+=texture(u_texture,uv+vec2(d.x,-d.y)).rgb*0.09; "
    "c+=texture(u_texture,uv-vec2(d.x,-d.y)).rgb*0.09; "
    "c+=texture(u_texture,uv+vec2(-d.x,d.y)).rgb*0.09; "
    "c+=texture(u_texture,uv-vec2(-d.x,d.y)).rgb*0.09; return c; }\n"
    "void main() {\n"
    "  vec2 uv=vec2(v_uv.x,1.0-v_uv.y); vec2 original_uv=uv; vec2 "
    "texel=1.0/max(u_resolution,vec2(1.0)); vec2 c=uv-0.5;\n"
    "  if(u_wave>0.0) uv.y += sin(uv.x*24.0+u_time*3.0)*0.008*u_wave;\n"
    "  if(u_ripple>0.0) { float r=length(c); uv += "
    "normalize(c+vec2(0.0001))*sin(r*45.0-u_time*4.0)*0.008*u_ripple*(1.0-r); "
    "}\n"
    "  if(u_displacement>0.0) { vec2 "
    "n=vec2(hash(uv*91.7+u_time)-0.5,hash(uv*43.1-u_time)-0.5); uv += "
    "n*texel*18.0*u_displacement; }\n"
    "  if(u_glitch>0.0) { float band=floor(uv.y*80.0); float "
    "g=hash(vec2(band,floor(u_time*8.0))); if(g<u_glitch*0.35) uv.x += "
    "(g-0.5)*0.12*u_glitch; }\n"
    "  if(u_vhs>0.0) uv.x += sin(uv.y*500.0+u_time*20.0)*0.0015*u_vhs;\n"
    "  if(u_kaleidoscope>0.0) { float segments=max(2.0,floor(u_kaleidoscope)); "
    "float r=length(c); float a=atan(c.y,c.x); float "
    "sector=6.2831853/segments; a=mod(a+sector*0.5,sector)-sector*0.5; "
    "a=abs(a); uv=0.5+vec2(cos(a),sin(a))*r; }\n"
    "  if(u_fisheye>0.0) { float r=length(c); float k=1.0+u_fisheye*r*r; "
    "uv=0.5+c*k; }\n"
    "  float distortion=u_barrel+u_lens_distortion; if(abs(distortion)>0.0001) "
    "{ float r2=dot(c,c); uv=0.5+c*(1.0+distortion*r2); }\n"
    "  if(u_pixelate>1.0) { vec2 grid=max(u_resolution/u_pixelate,vec2(1.0)); "
    "uv=(floor(uv*grid)+0.5)/grid; }\n"
    "  vec2 chroma_c=uv-0.5; vec2 "
    "chroma_shift=chroma_c*(u_chromatic+u_rgb_split*0.01);\n"
    "  vec3 color=sample_rgb(uv,chroma_shift);\n"
    "  if(u_motion_blur>0.0) { vec2 "
    "d=vec2(texel.x*4.0,texel.y*2.0)*u_motion_blur; "
    "color=mix(color,(texture(u_texture,uv+d).rgb+texture(u_texture,uv-d).rgb)*"
    "0.5,u_motion_blur); }\n"
    "  if(u_depth_of_field>0.0) { float focus=max(u_dof_focus,0.001); float "
    "radial=abs(length(chroma_c)-focus); float "
    "blur=clamp(radial*u_dof_aperture*4.0,0.0,1.0)*u_depth_of_field; "
    "color=mix(color,blur9(uv,texel*max(1.0,u_dof_aperture*3.0)),blur); }\n"
    "  if(u_sharpen>0.0 || u_clarity>0.0 || u_detail>0.0) { vec3 "
    "n=texture(u_texture,uv+vec2(0,-texel.y)).rgb; vec3 "
    "s=texture(u_texture,uv+vec2(0,texel.y)).rgb; vec3 "
    "e=texture(u_texture,uv+vec2(texel.x,0)).rgb; vec3 "
    "w=texture(u_texture,uv-vec2(texel.x,0)).rgb; vec3 avg=(n+s+e+w)*0.25; "
    "float strength=u_sharpen+u_clarity*0.5+u_detail*0.35; color+= "
    "(color-avg)*strength; }\n"
    "  if(u_bloom_intensity>0.0) { float radius=max(1.0,u_bloom_radius); vec3 "
    "bloom=blur9(uv,texel*radius); float lum=luminance(bloom); float "
    "knee=max(0.001,u_bloom_softness); float "
    "mask=smoothstep(u_bloom_threshold-knee,u_bloom_threshold+knee,lum); "
    "color+=bloom*mask*u_bloom_intensity; }\n"
    "  if(u_lens_dirt>0.0) { float dirt=hash(floor(uv*u_resolution/3.0)); "
    "float lum=luminance(color); color+=color*dirt*lum*0.08*u_lens_dirt; }\n"
    "  color*=max(u_exposure,0.0); if(u_filmic>0.0) "
    "color=mix(color,filmic_tonemap(color),clamp(u_filmic,0.0,1.0));\n"
    "  float gray=luminance(color); color=mix(vec3(gray),color,u_saturation); "
    "color=(color-0.5)*u_contrast+0.5; color+=u_brightness;\n"
    "  color.r += u_temperature*0.06; color.b -= u_temperature*0.06; color.g "
    "+= u_tint*0.025; color.r -= u_tint*0.015;\n"
    "  if(u_grayscale>0.0) "
    "color=mix(color,vec3(luminance(color)),clamp(u_grayscale,0.0,1.0));\n"
    "  if(u_sepia>0.0) { vec3 "
    "sep=vec3(dot(color,vec3(0.393,0.769,0.189)),dot(color,vec3(0.349,0.686,0."
    "168)),dot(color,vec3(0.272,0.534,0.131))); color=mix(color,sep,u_sepia); "
    "}\n"
    "  if(u_invert>0.0) color=mix(color,vec3(1.0)-color,u_invert);\n"
    "  if(u_solarize>0.0) color=mix(color,1.0-abs(1.0-2.0*color),u_solarize);\n"
    "  if(u_colorize>0.0) "
    "color=mix(color,hsv2rgb(vec3(u_colorize_hue,0.65,gray)),u_colorize);\n"
    "  if(u_posterize>1.0) color=floor(color*u_posterize+0.5)/u_posterize;\n"
    "  if(u_cel_shade>0.0) { float bands=4.0; float q=floor(gray*bands)/bands; "
    "color=mix(color,color*(0.55+0.45*q/max(gray,0.05)),u_cel_shade); }\n"
    "  if(u_edge>0.0 || u_outline>0.0) { vec3 "
    "n=texture(u_texture,uv+vec2(0,-texel.y)).rgb; vec3 "
    "s=texture(u_texture,uv+vec2(0,texel.y)).rgb; vec3 "
    "e=texture(u_texture,uv+vec2(texel.x,0)).rgb; vec3 "
    "w=texture(u_texture,uv-vec2(texel.x,0)).rgb; float "
    "edge=length(e-w)+length(n-s); vec3 edgec=vec3(edge*0.8); "
    "color=mix(color,edgec,u_edge); "
    "color*=1.0-u_outline*clamp(edge*2.0,0.0,1.0); }\n"
    "  if(u_halftone>0.0) { float "
    "p=0.5+0.5*sin(uv.x*u_resolution.x*0.12)*sin(uv.y*u_resolution.y*0.12); "
    "color=mix(color,vec3(step(0.5,p)),u_halftone*0.35); }\n"
    "  if(u_dither>0.0) { float d=hash(floor(uv*u_resolution))*0.00390625; "
    "color+=((d-0.001953125)*u_dither); }\n"
    "  if(u_scanlines>0.0) { float "
    "line=0.5+0.5*sin(uv.y*u_resolution.y*3.14159265); "
    "color*=1.0-u_scanlines*0.25*line; }\n"
    "  if(u_crt>0.0) { float r2=dot(chroma_c,chroma_c); "
    "color*=1.0-u_crt*r2*1.8; float "
    "mask=0.94+0.06*sin(uv.x*u_resolution.x*3.14159); "
    "color*=mix(1.0,mask,u_crt); }\n"
    "  if(u_halation>0.0) { vec3 red=texture(u_texture,uv+texel*2.0).rgb; "
    "float bright=smoothstep(0.6,1.0,luminance(red)); "
    "color+=red*vec3(1.0,0.25,0.12)*bright*u_halation*0.12; }\n"
    "  if(u_film_response>0.0) { "
    "color+=vec3(0.02,0.0,-0.01)*gray*u_film_response; "
    "color=pow(max(color,vec3(0.0)),vec3(1.0-0.08*u_film_response)); }\n"
    "  if(u_fog>0.0) { float "
    "f=clamp(length(original_uv-0.5)*2.0*u_fog_density+u_fog_height,0.0,1.0)*u_"
    "fog; color=mix(color,vec3(0.58,0.64,0.72),f); }\n"
    "  float vig=1.0-u_vignette*dot(chroma_c,chroma_c)*1.8; "
    "color*=max(vig,0.0);\n"
    "  if(u_noise>0.0) { float nn=hash(uv*u_resolution+u_time*31.0)-0.5; "
    "color+=nn*0.08*u_noise; }\n"
    "  if(u_glitch>0.0) { float "
    "g=hash(vec2(floor(uv.y*120.0),floor(u_time*12.0))); "
    "color=mix(color,color.bgr,g*u_glitch*0.12); }\n"
    "  if(u_vhs>0.0) { float "
    "n=hash(vec2(floor(uv.y*u_resolution.y),floor(u_time*6.0))); "
    "color.rg+=vec2(n-0.5,0.5-n)*u_vhs*0.025; }\n"
    "  float grain_amount=u_grain+u_film_grain; if(grain_amount>0.0) { float "
    "n=hash(uv*u_resolution+u_time*12.0); float "
    "n2=hash(uv*u_resolution*1.7-u_time*7.0); "
    "color+=(n+n2-1.0)*0.5*grain_amount; }\n"
    "  color=clamp(color,0.0,1.0); "
    "color=pow(max(color,vec3(0.0)),vec3(1.0/max(u_gamma,0.01)));\n"
    "  "
    "frag_color=vec4(mix(texture(u_texture,original_uv).rgb,color,clamp(u_"
    "intensity,0.0,1.0)),1.0)*v_color;\n"
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

  AP_TextureSetMSAA(g_post.scene, g_post.config.msaa_samples);
  AP_TextureSetMSAA(g_post.gui, g_post.config.msaa_samples);

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
    AP_SetUniformF("u_vignette", (flags & (uint32_t)AP_POST_VIGNETTE) != 0
                                     ? g_post.config.vignette
                                     : 0.0f);
    AP_SetUniformF("u_bloom_threshold", g_post.config.bloom_threshold);
    AP_SetUniformF("u_bloom_intensity", (flags & (uint32_t)AP_POST_BLOOM) != 0
                                            ? g_post.config.bloom_intensity
                                            : 0.0f);
    /* u_saturation/u_contrast/u_brightness/u_chromatic/u_grain/u_sharpen are
     * set once below, merged with the extra-effects fallback values. */
    AP_PostEnsureExtra();
    AP_SetUniformF("u_intensity", g_post_extra.intensity);
    AP_SetUniformF("u_exposure", g_post_extra.exposure);
    AP_SetUniformF("u_gamma", g_post_extra.gamma);
    AP_SetUniformF("u_filmic", g_post_extra.filmic);
    AP_SetUniformF("u_saturation", (flags & (uint32_t)AP_POST_COLOR_GRADE) != 0
                                       ? g_post.config.saturation
                                       : g_post_extra.saturation);
    AP_SetUniformF("u_contrast", (flags & (uint32_t)AP_POST_COLOR_GRADE) != 0
                                     ? g_post.config.contrast
                                     : g_post_extra.contrast);
    AP_SetUniformF("u_brightness", (flags & (uint32_t)AP_POST_COLOR_GRADE) != 0
                                       ? g_post.config.brightness
                                       : g_post_extra.brightness);
    AP_SetUniformF("u_temperature", g_post_extra.temperature);
    AP_SetUniformF("u_tint", g_post_extra.tint);
    AP_SetUniformF("u_sharpen", (flags & (uint32_t)AP_POST_SHARPEN) != 0
                                    ? g_post.config.sharpen
                                    : g_post_extra.sharpen);
    AP_SetUniformF("u_clarity", g_post_extra.clarity);
    AP_SetUniformF("u_detail", g_post_extra.detail);
    AP_SetUniformF("u_bloom_radius", g_post_extra.bloom_radius);
    AP_SetUniformF("u_bloom_softness", g_post_extra.bloom_softness);
    AP_SetUniformF("u_lens_dirt", g_post_extra.lens_dirt);
    AP_SetUniformF("u_pixelate", g_post_extra.pixelate);
    AP_SetUniformF("u_scanlines", g_post_extra.scanlines);
    AP_SetUniformF("u_crt", g_post_extra.crt);
    AP_SetUniformF("u_vhs", g_post_extra.vhs);
    AP_SetUniformF("u_rgb_split", g_post_extra.rgb_split);
    AP_SetUniformF("u_barrel", g_post_extra.barrel);
    AP_SetUniformF("u_lens_distortion", g_post_extra.lens_distortion);
    AP_SetUniformF("u_sepia", g_post_extra.sepia);
    AP_SetUniformF("u_grayscale", g_post_extra.grayscale);
    AP_SetUniformF("u_invert", g_post_extra.invert);
    AP_SetUniformF("u_solarize", g_post_extra.solarize);
    AP_SetUniformF("u_colorize", g_post_extra.colorize);
    AP_SetUniformF("u_colorize_hue", g_post_extra.colorize_hue);
    AP_SetUniformF("u_posterize", g_post_extra.posterize);
    AP_SetUniformF("u_edge", g_post_extra.edge);
    AP_SetUniformF("u_outline", g_post_extra.outline);
    AP_SetUniformF("u_cel_shade", g_post_extra.cel_shade);
    AP_SetUniformF("u_dither", g_post_extra.dither);
    AP_SetUniformF("u_halftone", g_post_extra.halftone);
    AP_SetUniformF("u_glitch", g_post_extra.glitch);
    AP_SetUniformF("u_noise", g_post_extra.noise);
    AP_SetUniformF("u_displacement", g_post_extra.displacement);
    AP_SetUniformF("u_kaleidoscope", g_post_extra.kaleidoscope);
    AP_SetUniformF("u_wave", g_post_extra.wave);
    AP_SetUniformF("u_ripple", g_post_extra.ripple);
    AP_SetUniformF("u_fisheye", g_post_extra.fisheye);
    AP_SetUniformF("u_chromatic", (flags & (uint32_t)AP_POST_CHROMATIC) != 0
                                      ? g_post.config.chromatic
                                      : g_post_extra.chromatic);
    AP_SetUniformF("u_grain", (flags & (uint32_t)AP_POST_GRAIN) != 0
                                  ? g_post.config.grain
                                  : g_post_extra.grain);
    AP_SetUniformF("u_film_grain", g_post_extra.film_grain);
    AP_SetUniformF("u_film_response", g_post_extra.film_response);
    AP_SetUniformF("u_halation", g_post_extra.halation);
    AP_SetUniformF("u_fog", g_post_extra.fog);
    AP_SetUniformF("u_fog_density", g_post_extra.fog_density);
    AP_SetUniformF("u_fog_height", g_post_extra.fog_height);
    AP_SetUniformF("u_depth_of_field", g_post_extra.depth_of_field);
    AP_SetUniformF("u_dof_focus", g_post_extra.dof_focus);
    AP_SetUniformF("u_dof_aperture", g_post_extra.dof_aperture);
    AP_SetUniformF("u_motion_blur", g_post_extra.motion_blur);
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
  config.msaa_samples = 0;
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
  g_post.config.saturation =
      AP_PostClampf(g_post.config.saturation, 0.0f, 2.0f);
  g_post.config.contrast = AP_PostClampf(g_post.config.contrast, 0.0f, 3.0f);
  g_post.config.brightness =
      AP_PostClampf(g_post.config.brightness, -1.0f, 1.0f);
  g_post.config.chromatic = AP_PostClampf(g_post.config.chromatic, 0.0f, 0.05f);
  g_post.config.grain = AP_PostClampf(g_post.config.grain, 0.0f, 0.5f);
  g_post.config.sharpen = AP_PostClampf(g_post.config.sharpen, 0.0f, 1.0f);
  if (g_post.config.msaa_samples < 0) {
    g_post.config.msaa_samples = 0;
  }
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
  AP_PostEnsureExtra();
  g_post.config.vignette = AP_PostClampf(amount, 0.0f, 2.0f);
  g_post_extra.vignette = g_post.config.vignette;
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

bool AP_SetPostMSAA(int samples) {
  AP_PostEnsureConfig();
  g_post.config.msaa_samples = samples < 0 ? 0 : samples;
  if (g_post.scene != NULL) {
    AP_TextureSetMSAA(g_post.scene, g_post.config.msaa_samples);
  }
  if (g_post.gui != NULL) {
    AP_TextureSetMSAA(g_post.gui, g_post.config.msaa_samples);
  }
  return true;
}

int AP_GetPostMSAA(void) {
  AP_PostEnsureConfig();
  return g_post.config.msaa_samples;
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

#define AP_POST_EXTRA_SETTER(name, field, lo, hi)                              \
  bool name(float value) {                                                     \
    AP_PostEnsureExtra();                                                      \
    g_post_extra.field = AP_PostClampf(value, lo, hi);                         \
    return true;                                                               \
  }

AP_POST_EXTRA_SETTER(AP_SetPostIntensity, intensity, 0.0f, 1.0f)
AP_POST_EXTRA_SETTER(AP_SetPostExposure, exposure, 0.0f, 8.0f)
AP_POST_EXTRA_SETTER(AP_SetPostGamma, gamma, 0.1f, 4.0f)
AP_POST_EXTRA_SETTER(AP_SetPostFilmic, filmic, 0.0f, 1.0f)
AP_POST_EXTRA_SETTER(AP_SetPostContrast, contrast, 0.0f, 3.0f)
AP_POST_EXTRA_SETTER(AP_SetPostSaturation, saturation, 0.0f, 3.0f)
AP_POST_EXTRA_SETTER(AP_SetPostBrightness, brightness, -1.0f, 1.0f)
AP_POST_EXTRA_SETTER(AP_SetPostTemperature, temperature, -1.0f, 1.0f)
AP_POST_EXTRA_SETTER(AP_SetPostTint, tint, -1.0f, 1.0f)
// AP_POST_EXTRA_SETTER(AP_SetPostSharpen, sharpen, 0.0f, 2.0f)
AP_POST_EXTRA_SETTER(AP_SetPostClarity, clarity, 0.0f, 2.0f)
AP_POST_EXTRA_SETTER(AP_SetPostDetail, detail, 0.0f, 2.0f)
AP_POST_EXTRA_SETTER(AP_SetPostBloomRadius, bloom_radius, 0.0f, 32.0f)
AP_POST_EXTRA_SETTER(AP_SetPostBloomSoftness, bloom_softness, 0.0f, 1.0f)
AP_POST_EXTRA_SETTER(AP_SetPostLensDirt, lens_dirt, 0.0f, 1.0f)
AP_POST_EXTRA_SETTER(AP_SetPostLensDistortion, lens_distortion, -1.0f, 1.0f)
AP_POST_EXTRA_SETTER(AP_SetPostBarrel, barrel, -1.0f, 1.0f)
AP_POST_EXTRA_SETTER(AP_SetPostFilmGrain, film_grain, 0.0f, 1.0f)
AP_POST_EXTRA_SETTER(AP_SetPostFilmResponse, film_response, 0.0f, 1.0f)
AP_POST_EXTRA_SETTER(AP_SetPostHalation, halation, 0.0f, 1.0f)
AP_POST_EXTRA_SETTER(AP_SetPostFog, fog, 0.0f, 1.0f)
AP_POST_EXTRA_SETTER(AP_SetPostFogDensity, fog_density, 0.0f, 10.0f)
AP_POST_EXTRA_SETTER(AP_SetPostFogHeight, fog_height, -1.0f, 1.0f)
AP_POST_EXTRA_SETTER(AP_SetPostDepthOfField, depth_of_field, 0.0f, 1.0f)
AP_POST_EXTRA_SETTER(AP_SetPostDOFFocus, dof_focus, 0.0f, 2.0f)
AP_POST_EXTRA_SETTER(AP_SetPostDOFAperture, dof_aperture, 0.0f, 8.0f)
AP_POST_EXTRA_SETTER(AP_SetPostMotionBlur, motion_blur, 0.0f, 1.0f)
AP_POST_EXTRA_SETTER(AP_SetPostPixelate, pixelate, 0.0f, 4096.0f)
AP_POST_EXTRA_SETTER(AP_SetPostScanlines, scanlines, 0.0f, 1.0f)
AP_POST_EXTRA_SETTER(AP_SetPostCRT, crt, 0.0f, 1.0f)
AP_POST_EXTRA_SETTER(AP_SetPostVHS, vhs, 0.0f, 1.0f)
AP_POST_EXTRA_SETTER(AP_SetPostRGBSplit, rgb_split, 0.0f, 1.0f)
AP_POST_EXTRA_SETTER(AP_SetPostSepia, sepia, 0.0f, 1.0f)
AP_POST_EXTRA_SETTER(AP_SetPostGrayscale, grayscale, 0.0f, 1.0f)
AP_POST_EXTRA_SETTER(AP_SetPostInvert, invert, 0.0f, 1.0f)
AP_POST_EXTRA_SETTER(AP_SetPostSolarize, solarize, 0.0f, 1.0f)
AP_POST_EXTRA_SETTER(AP_SetPostColorize, colorize, 0.0f, 1.0f)
AP_POST_EXTRA_SETTER(AP_SetPostColorizeHue, colorize_hue, 0.0f, 1.0f)
AP_POST_EXTRA_SETTER(AP_SetPostPosterize, posterize, 2.0f, 256.0f)
AP_POST_EXTRA_SETTER(AP_SetPostEdge, edge, 0.0f, 1.0f)
AP_POST_EXTRA_SETTER(AP_SetPostOutline, outline, 0.0f, 1.0f)
AP_POST_EXTRA_SETTER(AP_SetPostCelShade, cel_shade, 0.0f, 1.0f)
AP_POST_EXTRA_SETTER(AP_SetPostDither, dither, 0.0f, 1.0f)
AP_POST_EXTRA_SETTER(AP_SetPostHalftone, halftone, 0.0f, 1.0f)
AP_POST_EXTRA_SETTER(AP_SetPostGlitch, glitch, 0.0f, 1.0f)
AP_POST_EXTRA_SETTER(AP_SetPostNoise, noise, 0.0f, 1.0f)
AP_POST_EXTRA_SETTER(AP_SetPostDisplacement, displacement, 0.0f, 1.0f)
AP_POST_EXTRA_SETTER(AP_SetPostKaleidoscope, kaleidoscope, 0.0f, 64.0f)
AP_POST_EXTRA_SETTER(AP_SetPostWave, wave, 0.0f, 1.0f)
AP_POST_EXTRA_SETTER(AP_SetPostRipple, ripple, 0.0f, 1.0f)
AP_POST_EXTRA_SETTER(AP_SetPostFisheye, fisheye, 0.0f, 1.0f)

#undef AP_POST_EXTRA_SETTER

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

bool AP_PostIncludeGui(void) { return AP_GetGuiLayer() == AP_GUI_LAYER_SCENE; }

void AP_PostBeginFrame(void) {
  AP_Texture *current;
  int width = 0;
  int height = 0;

  AP_PostEnsureConfig();
  if (g_post.applying) {
    return;
  }

  g_post.gui_active = false;

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
  if (AP_PostIncludeGui()) {
    AP_SetGuiLayer(AP_GUI_LAYER_SCENE);
  } else {
    AP_SetGuiLayer(AP_GUI_LAYER_OVERLAY);
  }
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

    AP_OpenGLSetClearColor(0.0f, 0.0f, 0.0f, 0.0f);
    AP_OpenGLClear(true, true, true);

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
  AP_TextureResolveMSAA(g_post.scene);
  AP_PostApplyEffects();

  if (g_post.gui_active && g_post.gui != NULL) {
    AP_TextureResolveMSAA(g_post.gui);
    AP_BlendMode blend = AP_GetTextureBlendMode(g_post.gui);
    AP_SetTextureBlendMode(g_post.gui, AP_BLENDMODE_PREMULTIPLIED);
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
  g_config_ready = false;
  g_post_extra_ready = false;
  memset(&g_post.config, 0, sizeof(g_post.config));
  memset(&g_post_extra, 0, sizeof(g_post_extra));
}
