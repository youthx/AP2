/*
 * AP2 — Application Primitives
 * Copyright (c) 2024-2026 Jack Waechter
 *
 * Licensed under the MIT License.
 * See LICENSE in the project root for full terms.
 */

#ifndef AP2_SHADER_H
#define AP2_SHADER_H

#include "AP2/AP2_Types.h"

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * AP2 Shader
 *
 * Custom GLSL programs for the immediate renderer, or for advanced
 * GPU work (geometry, tessellation, compute).
 *
 * Simple path — fragment effect on FillRect / DrawLine / etc.:
 *
 *     AP_Shader *wave = AP_CreateShader(NULL, fragment_source);
 *     AP_UseShader(wave);
 *     AP_SetUniformF("u_time", time);
 *     AP_FillRect(&(AP_FRect){0.0f, 0.0f, 1280.0f, 720.0f});
 *     AP_UseShader(NULL);
 *
 * Advanced path — extra stages, defines, explicit bindings:
 *
 *     AP_ShaderConfig config = AP_ShaderDefaultConfig();
 *     config.vertex.source = vs;
 *     config.fragment.source = fs;
 *     config.geometry.source = gs;
 *     config.defines = defines;
 *     config.define_count = 2;
 *     AP_Shader *shader = AP_CreateShaderEx(&config);
 *
 * Immediate-mode vertex layout (when using AP_UseShader):
 *   location 0 : vec2  a_position
 *   location 1 : vec4  a_color
 *   location 2 : vec2  a_uv
 *   uniform    : vec2  u_resolution   (framebuffer pixels, top-left origin)
 *   uniform    : sampler2D u_texture  (white 1x1 when drawing untextured)
 *
 * Passing NULL for vertex or fragment source uses the builtin stage so
 * a custom fragment (or vertex) can be supplied alone. Nothing is hidden:
 * AP_CreateShader() is a thin wrapper over AP_CreateShaderEx(), and
 * AP_ShaderGetNativeHandle() returns the backend program object.
 */

typedef struct AP_Shader AP_Shader;

/* =========================================================
 * Uniform types
 *
 * Used by the generic AP_SetUniform / AP_SetUniformN path.
 * Named helpers (AP_SetUniformF, AP_SetUniformVec2, ...) cover
 * the common cases without this enum.
 * ========================================================= */

typedef enum AP_UniformType {
  AP_UNIFORM_F1 = 0,
  AP_UNIFORM_F2,
  AP_UNIFORM_F3,
  AP_UNIFORM_F4,
  AP_UNIFORM_I1,
  AP_UNIFORM_I2,
  AP_UNIFORM_I3,
  AP_UNIFORM_I4,
  AP_UNIFORM_U1,
  AP_UNIFORM_U2,
  AP_UNIFORM_U3,
  AP_UNIFORM_U4,
  AP_UNIFORM_MAT2,
  AP_UNIFORM_MAT3,
  AP_UNIFORM_MAT4,
  AP_UNIFORM_MAT2X3,
  AP_UNIFORM_MAT3X2,
  AP_UNIFORM_MAT2X4,
  AP_UNIFORM_MAT4X2,
  AP_UNIFORM_MAT3X4,
  AP_UNIFORM_MAT4X3
} AP_UniformType;

typedef enum AP_TransformFeedbackMode {
  AP_TRANSFORM_FEEDBACK_INTERLEAVED = 0,
  AP_TRANSFORM_FEEDBACK_SEPARATE
} AP_TransformFeedbackMode;

/* =========================================================
 * Configuration
 * ========================================================= */

typedef struct AP_ShaderStage {
  /*
   * GLSL source. Takes priority over path when both are set.
   */
  const char *source;

  /*
   * UTF-8 filesystem path loaded when source is NULL.
   */
  const char *path;
} AP_ShaderStage;

typedef struct AP_ShaderBinding {
  const char *name;
  AP_Int location;
} AP_ShaderBinding;

typedef struct AP_ShaderConfig {
  /*
   * Optional debug label.
   */
  const char *name;

  AP_ShaderStage vertex;
  AP_ShaderStage fragment;
  AP_ShaderStage geometry;
  AP_ShaderStage tess_control;
  AP_ShaderStage tess_eval;
  AP_ShaderStage compute;

  /*
   * Injected after the #version line as `#define NAME value`.
   * Each entry is "NAME" or "NAME=value".
   */
  const char *const *defines;
  int define_count;

  /*
   * Bound with glBindAttribLocation before linking.
   */
  const AP_ShaderBinding *attribs;
  int attrib_count;

  /*
   * Bound with glBindFragDataLocation before linking.
   */
  const AP_ShaderBinding *frag_outputs;
  int frag_output_count;

  const char *const *transform_feedback_varyings;
  int transform_feedback_count;
  AP_TransformFeedbackMode transform_feedback_mode;

  bool separable;
  bool binary_retrievable;
} AP_ShaderConfig;

/*
 * Zeroed config. Transform feedback defaults to interleaved.
 */
AP_ShaderConfig AP_ShaderDefaultConfig(void);

/*
 * Builtin immediate-mode stages. Use these to wrap or extend
 * the default vertex/fragment shaders without copying GLSL.
 */
const char *AP_ShaderBuiltinVertexSource(void);

const char *AP_ShaderBuiltinFragmentSource(void);

/* =========================================================
 * Creation
 *
 * AP_CreateShader() compiles vertex + fragment source.
 * NULL vertex uses the builtin vertex stage.
 * NULL fragment uses the builtin fragment stage.
 * At least one source must be non-NULL.
 *
 * AP_CreateShaderFromFile() is the same using filesystem paths.
 * AP_CreateComputeShader() compiles a compute-only program.
 * AP_CreateShaderEx() is the full configuration path.
 * ========================================================= */

AP_Shader *AP_CreateShader(const char *vertex_source,
                           const char *fragment_source);

AP_Shader *AP_CreateShaderFromFile(const char *vertex_path,
                                   const char *fragment_path);

AP_Shader *AP_CreateComputeShader(const char *compute_source);

AP_Shader *AP_CreateShaderEx(const AP_ShaderConfig *config);

/*
 * Destroys a shader created by the functions above.
 * If the shader is currently bound via AP_UseShader(), the
 * renderer returns to the builtin shader.
 *
 * The builtin renderer shader cannot be destroyed.
 */
void AP_DestroyShader(AP_Shader *shader);

bool AP_ShaderIsValid(const AP_Shader *shader);

bool AP_ShaderIsCompute(const AP_Shader *shader);

const char *AP_ShaderGetName(const AP_Shader *shader);

/*
 * Backend program object (OpenGL GLuint). 0 if invalid.
 */
AP_UInt AP_ShaderGetNativeHandle(const AP_Shader *shader);

/* =========================================================
 * Binding
 *
 * AP_UseShader() selects the program used by immediate drawing
 * (points, lines, rects, circles, triangles). Pass NULL to restore
 * the builtin shader. Pending geometry is flushed first.
 *
 * AP_ShaderBind() binds a program for manual GPU work without
 * changing the immediate-mode shader. AP_ShaderUnbind() binds 0.
 * ========================================================= */

bool AP_UseShader(AP_Shader *shader);

AP_Shader *AP_GetShader(void);

bool AP_ResetShader(void);

bool AP_ShaderBind(AP_Shader *shader);

bool AP_ShaderUnbind(void);

/* =========================================================
 * Compute
 * ========================================================= */

bool AP_DispatchCompute(AP_UInt groups_x, AP_UInt groups_y, AP_UInt groups_z);

/* =========================================================
 * Locations
 *
 * Names are cached on the shader after the first lookup.
 * ========================================================= */

AP_Int AP_ShaderGetUniformLocation(AP_Shader *shader, const char *name);

AP_Int AP_ShaderGetAttribLocation(AP_Shader *shader, const char *name);

AP_Int AP_ShaderGetUniformBlockIndex(AP_Shader *shader, const char *name);

bool AP_ShaderSetUniformBlockBinding(AP_Shader *shader, const char *name,
                                     AP_UInt binding);

/* =========================================================
 * Uniforms — active shader
 *
 * Target the shader selected with AP_UseShader(), or the builtin
 * shader when none is selected. Location -1 uniforms are ignored.
 * ========================================================= */

bool AP_SetUniformF(const char *name, AP_F32 x);

bool AP_SetUniformF2(const char *name, AP_F32 x, AP_F32 y);

bool AP_SetUniformF3(const char *name, AP_F32 x, AP_F32 y, AP_F32 z);

bool AP_SetUniformF4(const char *name, AP_F32 x, AP_F32 y, AP_F32 z, AP_F32 w);

bool AP_SetUniformI(const char *name, AP_Int x);

bool AP_SetUniformI2(const char *name, AP_Int x, AP_Int y);

bool AP_SetUniformI3(const char *name, AP_Int x, AP_Int y, AP_Int z);

bool AP_SetUniformI4(const char *name, AP_Int x, AP_Int y, AP_Int z, AP_Int w);

bool AP_SetUniformU(const char *name, AP_UInt x);

bool AP_SetUniformU2(const char *name, AP_UInt x, AP_UInt y);

bool AP_SetUniformU3(const char *name, AP_UInt x, AP_UInt y, AP_UInt z);

bool AP_SetUniformU4(const char *name, AP_UInt x, AP_UInt y, AP_UInt z,
                     AP_UInt w);

bool AP_SetUniformVec2(const char *name, AP_Vec2 value);

bool AP_SetUniformVec3(const char *name, AP_Vec3 value);

bool AP_SetUniformVec4(const char *name, AP_Vec4 value);

bool AP_SetUniformColor(const char *name, AP_Color value);

bool AP_SetUniformMat3(const char *name, const AP_F32 *matrix, bool transpose);

bool AP_SetUniformMat4(const char *name, const AP_F32 *matrix, bool transpose);

bool AP_SetUniform(const char *name, AP_UniformType type, const void *value);

bool AP_SetUniformN(const char *name, AP_UniformType type, const void *value,
                    int count);

/* =========================================================
 * Uniforms — explicit shader
 *
 * Same setters, targeting any compiled shader. The program does
 * not need to be the active immediate-mode shader.
 * ========================================================= */

bool AP_ShaderSetUniformF(AP_Shader *shader, const char *name, AP_F32 x);

bool AP_ShaderSetUniformF2(AP_Shader *shader, const char *name, AP_F32 x,
                           AP_F32 y);

bool AP_ShaderSetUniformF3(AP_Shader *shader, const char *name, AP_F32 x,
                           AP_F32 y, AP_F32 z);

bool AP_ShaderSetUniformF4(AP_Shader *shader, const char *name, AP_F32 x,
                           AP_F32 y, AP_F32 z, AP_F32 w);

bool AP_ShaderSetUniformI(AP_Shader *shader, const char *name, AP_Int x);

bool AP_ShaderSetUniformI2(AP_Shader *shader, const char *name, AP_Int x,
                           AP_Int y);

bool AP_ShaderSetUniformI3(AP_Shader *shader, const char *name, AP_Int x,
                           AP_Int y, AP_Int z);

bool AP_ShaderSetUniformI4(AP_Shader *shader, const char *name, AP_Int x,
                           AP_Int y, AP_Int z, AP_Int w);

bool AP_ShaderSetUniformU(AP_Shader *shader, const char *name, AP_UInt x);

bool AP_ShaderSetUniformU2(AP_Shader *shader, const char *name, AP_UInt x,
                           AP_UInt y);

bool AP_ShaderSetUniformU3(AP_Shader *shader, const char *name, AP_UInt x,
                           AP_UInt y, AP_UInt z);

bool AP_ShaderSetUniformU4(AP_Shader *shader, const char *name, AP_UInt x,
                           AP_UInt y, AP_UInt z, AP_UInt w);

bool AP_ShaderSetUniformVec2(AP_Shader *shader, const char *name,
                             AP_Vec2 value);

bool AP_ShaderSetUniformVec3(AP_Shader *shader, const char *name,
                             AP_Vec3 value);

bool AP_ShaderSetUniformVec4(AP_Shader *shader, const char *name,
                             AP_Vec4 value);

bool AP_ShaderSetUniformColor(AP_Shader *shader, const char *name,
                              AP_Color value);

bool AP_ShaderSetUniformMat3(AP_Shader *shader, const char *name,
                             const AP_F32 *matrix, bool transpose);

bool AP_ShaderSetUniformMat4(AP_Shader *shader, const char *name,
                             const AP_F32 *matrix, bool transpose);

bool AP_ShaderSetUniform(AP_Shader *shader, const char *name,
                         AP_UniformType type, const void *value);

bool AP_ShaderSetUniformN(AP_Shader *shader, const char *name,
                          AP_UniformType type, const void *value, int count);

#ifdef __cplusplus
}
#endif

#endif /* AP2_SHADER_H */
