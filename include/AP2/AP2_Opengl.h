/*
 * AP2 — Application Primitives
 * Copyright (c) 2026 Jack Waechter
 *
 * Licensed under the MIT License.
 * See LICENSE in the project root for full terms.
 */

#ifndef AP2_OPENGL_H
#define AP2_OPENGL_H


#include "AP2/AP2_Init.h"
#include "AP2/AP2_Types.h"

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct AP_OpenGLConfig {
  AP_UInt major_version;
  AP_UInt minor_version;

  bool debug;
  bool vsync;
  bool double_buffer;
  bool multisample;
  AP_UInt multisample_samples;
} AP_OpenGLConfig;

typedef struct AP_OpenGLVersion {
  AP_UInt major;
  AP_UInt minor;
  AP_UInt patch;
} AP_OpenGLVersion;

typedef struct AP_OpenGLInfo {
  const char *vendor;
  const char *renderer;
  const char *version;
  const char *glsl_version;
  AP_OpenGLVersion version_number;
  bool core_profile;
  bool debug_context;
} AP_OpenGLInfo;

typedef struct AP_OpenGLLimits {
  int max_texture_size;
  int max_3d_texture_size;
  int max_cube_map_texture_size;
  int max_texture_image_units;
  int max_combined_texture_image_units;
  int max_vertex_attribs;
  int max_draw_buffers;
  int max_color_attachments;
  int max_samples;
  int max_uniform_buffer_bindings;
  int max_uniform_block_size;
  int max_vertex_uniform_components;
  int max_fragment_uniform_components;
} AP_OpenGLLimits;

typedef struct AP_OpenGLCapabilities {
  bool shaders;
  bool vertex_arrays;
  bool framebuffer_objects;
  bool multisampling;
  bool instancing;
  bool uniform_buffers;
  bool debug_output;
  bool compute_shaders;
  bool geometry_shaders;
  bool tessellation_shaders;
} AP_OpenGLCapabilities;

AP_OpenGLConfig AP_OpenGLDefaultConfig(void);

bool AP_OpenGLInit(const AP_OpenGLConfig *config);

void AP_OpenGLClose(void);

bool AP_OpenGLIsInitialized(void);

bool AP_OpenGLMakeContextCurrent(void *glfw_window);

bool AP_OpenGLHasContext(void);

void *AP_OpenGLGetCurrentContext(void);

const AP_OpenGLConfig *AP_OpenGLGetConfig(void);

bool AP_OpenGLSetVSync(bool enabled);

bool AP_OpenGLGetVSync(void);

const AP_OpenGLInfo *AP_OpenGLGetInfo(void);

AP_OpenGLVersion AP_OpenGLGetVersion(void);

const char *AP_OpenGLGetVendor(void);

const char *AP_OpenGLGetRenderer(void);

const char *AP_OpenGLGetVersionString(void);

const char *AP_OpenGLGetGLSLVersion(void);

const AP_OpenGLCapabilities *AP_OpenGLGetCapabilities(void);

const AP_OpenGLLimits *AP_OpenGLGetLimits(void);

bool AP_OpenGLClear(bool color, bool depth, bool stencil);

bool AP_OpenGLSetClearColor(float red, float green, float blue, float alpha);

bool AP_OpenGLGetClearColor(float *red, float *green, float *blue,
                            float *alpha);

bool AP_OpenGLSetViewport(int x, int y, int width, int height);

bool AP_OpenGLGetViewport(int *x, int *y, int *width, int *height);

bool AP_OpenGLSetScissor(int x, int y, int width, int height);

bool AP_OpenGLSetScissorEnabled(bool enabled);

bool AP_OpenGLGetScissorEnabled(void);

bool AP_OpenGLSetDepthTest(bool enabled);

bool AP_OpenGLGetDepthTest(void);

bool AP_OpenGLSetBlending(bool enabled);

bool AP_OpenGLGetBlending(void);

bool AP_OpenGLSetBlendFunc(AP_UInt source, AP_UInt destination);

bool AP_OpenGLSetBlendFuncSeparate(AP_UInt source_rgb, AP_UInt destination_rgb,
                                   AP_UInt source_alpha,
                                   AP_UInt destination_alpha);

bool AP_OpenGLSetCulling(bool enabled);

bool AP_OpenGLSetColorMask(bool red, bool green, bool blue, bool alpha);

bool AP_OpenGLSwapBuffers(void *glfw_window);

bool AP_OpenGLFlush(void);

bool AP_OpenGLFinish(void);

AP_UInt AP_OpenGLGetError(void);

const char *AP_OpenGLErrorName(AP_UInt error);

bool AP_OpenGLCheckError(const char *operation);

void AP_OpenGLClearErrors(void);

bool AP_OpenGLResetState(void);

/* =========================================================
 * Programs
 *
 * AP_OpenGLCreateProgram() is the simple vertex + fragment path.
 * AP_OpenGLCreateProgramEx() exposes every link-time option.
 * ========================================================= */

typedef struct AP_OpenGLAttribBind {
  const char *name;
  int location;
} AP_OpenGLAttribBind;

typedef struct AP_OpenGLProgramConfig {
  const char *vertex_source;
  const char *fragment_source;
  const char *geometry_source;
  const char *tess_control_source;
  const char *tess_eval_source;
  const char *compute_source;

  const AP_OpenGLAttribBind *attribs;
  int attrib_count;

  const AP_OpenGLAttribBind *frag_outputs;
  int frag_output_count;

  const char *const *transform_feedback_varyings;
  int transform_feedback_count;
  bool transform_feedback_interleaved;

  bool separable;
  bool binary_retrievable;
} AP_OpenGLProgramConfig;

AP_OpenGLProgramConfig AP_OpenGLDefaultProgramConfig(void);

AP_UInt AP_OpenGLCreateProgram(const char *vertex_source,
                               const char *fragment_source);

AP_UInt AP_OpenGLCreateProgramEx(const AP_OpenGLProgramConfig *config);

void AP_OpenGLDeleteProgram(AP_UInt program);
extern const AP_SubsystemMetadata AP_OpenGLSubsystem;

#ifdef __cplusplus
}
#endif

#endif /* AP2_OPENGL_H */
