/*
 * AP2 — Application Primitives
 * Copyright (c) 2024-2026 Jack Waechter
 *
 * Licensed under the MIT License.
 * See LICENSE in the project root for full terms.
 */

#include "AP2/AP2_Shader.h"

#include "AP2_Internal.h"

#include "AP2/AP2_Error.h"
#include "AP2/AP2_Logger.h"
#include "AP2/AP2_Opengl.h"

#define GLFW_INCLUDE_NONE
#include <glad/gl.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define AP_SHADER_SCRATCH_MAX 16
#define AP_SHADER_UNIFORM_GROW 16

typedef struct AP_ShaderUniformEntry {
  char *name;
  GLint location;
} AP_ShaderUniformEntry;

struct AP_Shader {
  GLuint program;
  bool compute;
  char *name;

  AP_ShaderUniformEntry *uniforms;
  int uniform_count;
  int uniform_capacity;

  GLint u_resolution;
  bool u_resolution_queried;
  GLint u_texture;
  bool u_texture_queried;
};

typedef struct AP_ShaderScratch {
  char *owned[AP_SHADER_SCRATCH_MAX];
  int count;
} AP_ShaderScratch;

static const char *AP_SHADER_BUILTIN_VERTEX =
    "#version 330 core\n"
    "layout(location = 0) in vec2 a_position;\n"
    "layout(location = 1) in vec4 a_color;\n"
    "layout(location = 2) in vec2 a_uv;\n"
    "uniform vec2 u_resolution;\n"
    "out vec4 v_color;\n"
    "out vec2 v_uv;\n"
    "void main() {\n"
    "  vec2 ndc = (a_position / u_resolution) * 2.0 - 1.0;\n"
    "  ndc.y = -ndc.y;\n"
    "  gl_Position = vec4(ndc, 0.0, 1.0);\n"
    "  v_color = a_color;\n"
    "  v_uv = a_uv;\n"
    "}\n";

static const char *AP_SHADER_BUILTIN_FRAGMENT =
    "#version 330 core\n"
    "in vec4 v_color;\n"
    "in vec2 v_uv;\n"
    "uniform sampler2D u_texture;\n"
    "out vec4 frag_color;\n"
    "void main() {\n"
    "  frag_color = v_color * texture(u_texture, v_uv);\n"
    "}\n";

/* =========================================================
 * Scratch / strings
 * ========================================================= */

static void AP_ShaderScratchInit(AP_ShaderScratch *scratch) {
  memset(scratch, 0, sizeof(*scratch));
}

static bool AP_ShaderScratchAdd(AP_ShaderScratch *scratch, char *memory) {
  if (memory == NULL) {
    return true;
  }

  if (scratch->count >= AP_SHADER_SCRATCH_MAX) {
    free(memory);
    AP_SET_ERROR(AP_ERROR_OUT_OF_MEMORY, "Shader scratch overflow");
    return false;
  }

  scratch->owned[scratch->count++] = memory;
  return true;
}

static void AP_ShaderScratchFree(AP_ShaderScratch *scratch) {
  int index;

  for (index = 0; index < scratch->count; ++index) {
    free(scratch->owned[index]);
    scratch->owned[index] = NULL;
  }

  scratch->count = 0;
}

static char *AP_ShaderCopyText(const char *text) {
  size_t length;
  char *copy;

  if (text == NULL) {
    return NULL;
  }

  length = strlen(text);
  copy = (char *)malloc(length + 1);
  if (copy == NULL) {
    return NULL;
  }

  memcpy(copy, text, length + 1);
  return copy;
}

static char *AP_ShaderLoadFile(const char *path) {
  FILE *file;
  long size;
  char *buffer;
  size_t read;

  file = fopen(path, "rb");
  if (file == NULL) {
    AP_SET_ERROR(AP_ERROR_NOT_FOUND, "Shader file could not be opened");
    return NULL;
  }

  if (fseek(file, 0, SEEK_END) != 0) {
    fclose(file);
    AP_SET_ERROR(AP_ERROR_OPERATION_FAILED, "Shader file could not be read");
    return NULL;
  }

  size = ftell(file);
  if (size < 0) {
    fclose(file);
    AP_SET_ERROR(AP_ERROR_OPERATION_FAILED, "Shader file could not be read");
    return NULL;
  }

  if (fseek(file, 0, SEEK_SET) != 0) {
    fclose(file);
    AP_SET_ERROR(AP_ERROR_OPERATION_FAILED, "Shader file could not be read");
    return NULL;
  }

  buffer = (char *)malloc((size_t)size + 1);
  if (buffer == NULL) {
    fclose(file);
    AP_SET_ERROR(AP_ERROR_OUT_OF_MEMORY, "Failed to allocate shader source");
    return NULL;
  }

  read = fread(buffer, 1, (size_t)size, file);
  fclose(file);

  if (read != (size_t)size) {
    free(buffer);
    AP_SET_ERROR(AP_ERROR_OPERATION_FAILED, "Shader file could not be read");
    return NULL;
  }

  buffer[size] = '\0';
  return buffer;
}

static bool AP_ShaderStageProvided(const AP_ShaderStage *stage) {
  return stage != NULL && (stage->source != NULL || stage->path != NULL);
}

static char *AP_ShaderBuildDefinedSource(const char *source,
                                         const char *const *defines,
                                         int define_count, bool *error) {
  const char *version;
  const char *insert;
  size_t prefix_length;
  size_t source_length;
  size_t extra = 0;
  size_t offset;
  int index;
  char *result;

  *error = false;

  if (source == NULL || defines == NULL || define_count <= 0) {
    return NULL;
  }

  for (index = 0; index < define_count; ++index) {
    if (defines[index] != NULL) {
      extra += 9 + strlen(defines[index]);
    }
  }

  if (extra == 0) {
    return NULL;
  }

  source_length = strlen(source);
  version = strstr(source, "#version");
  if (version != NULL) {
    const char *newline = strchr(version, '\n');
    insert = newline != NULL ? newline + 1 : source + source_length;
  } else {
    insert = source;
  }

  prefix_length = (size_t)(insert - source);
  result = (char *)malloc(source_length + extra + 1);
  if (result == NULL) {
    *error = true;
    AP_SET_ERROR(AP_ERROR_OUT_OF_MEMORY, "Failed to allocate shader source");
    return NULL;
  }

  memcpy(result, source, prefix_length);
  offset = prefix_length;

  for (index = 0; index < define_count; ++index) {
    const char *entry = defines[index];
    const char *equals;
    size_t name_length;

    if (entry == NULL || entry[0] == '\0') {
      continue;
    }

    memcpy(result + offset, "#define ", 8);
    offset += 8;

    equals = strchr(entry, '=');
    if (equals == NULL) {
      name_length = strlen(entry);
      memcpy(result + offset, entry, name_length);
      offset += name_length;
    } else {
      name_length = (size_t)(equals - entry);
      memcpy(result + offset, entry, name_length);
      offset += name_length;
      result[offset++] = ' ';
      name_length = strlen(equals + 1);
      memcpy(result + offset, equals + 1, name_length);
      offset += name_length;
    }

    result[offset++] = '\n';
  }

  memcpy(result + offset, insert, source_length - prefix_length);
  offset += source_length - prefix_length;
  result[offset] = '\0';
  return result;
}

static const char *AP_ShaderResolveStage(const AP_ShaderStage *stage,
                                         const char *fallback,
                                         AP_ShaderScratch *scratch,
                                         const char *const *defines,
                                         int define_count, bool *ok) {
  const char *source = NULL;
  char *loaded = NULL;
  char *processed;
  bool define_error = false;

  *ok = true;

  if (AP_ShaderStageProvided(stage)) {
    if (stage->source != NULL) {
      source = stage->source;
    } else {
      loaded = AP_ShaderLoadFile(stage->path);
      if (loaded == NULL) {
        *ok = false;
        return NULL;
      }

      if (!AP_ShaderScratchAdd(scratch, loaded)) {
        *ok = false;
        return NULL;
      }

      source = loaded;
    }
  } else {
    source = fallback;
  }

  if (source == NULL) {
    return NULL;
  }

  processed = AP_ShaderBuildDefinedSource(source, defines, define_count,
                                          &define_error);
  if (define_error) {
    *ok = false;
    return NULL;
  }

  if (processed != NULL) {
    if (!AP_ShaderScratchAdd(scratch, processed)) {
      *ok = false;
      return NULL;
    }

    return processed;
  }

  return source;
}

static bool AP_ShaderCopyBindings(const AP_ShaderBinding *bindings, int count,
                                  AP_OpenGLAttribBind **out_binds) {
  AP_OpenGLAttribBind *binds;
  int index;

  *out_binds = NULL;

  if (bindings == NULL || count <= 0) {
    return true;
  }

  binds = (AP_OpenGLAttribBind *)malloc((size_t)count * sizeof(*binds));
  if (binds == NULL) {
    AP_SET_ERROR(AP_ERROR_OUT_OF_MEMORY, "Failed to allocate shader bindings");
    return false;
  }

  for (index = 0; index < count; ++index) {
    binds[index].name = bindings[index].name;
    binds[index].location = (int)bindings[index].location;
  }

  *out_binds = binds;
  return true;
}

/* =========================================================
 * Public config / builtin sources
 * ========================================================= */

AP_ShaderConfig AP_ShaderDefaultConfig(void) {
  AP_ShaderConfig config;

  memset(&config, 0, sizeof(config));
  config.transform_feedback_mode = AP_TRANSFORM_FEEDBACK_INTERLEAVED;
  return config;
}

const char *AP_ShaderBuiltinVertexSource(void) {
  return AP_SHADER_BUILTIN_VERTEX;
}

const char *AP_ShaderBuiltinFragmentSource(void) {
  return AP_SHADER_BUILTIN_FRAGMENT;
}

/* =========================================================
 * Destruction / queries
 * ========================================================= */

static void AP_ShaderFree(AP_Shader *shader) {
  int index;

  if (shader == NULL) {
    return;
  }

  if (shader->program != 0) {
    AP_OpenGLDeleteProgram((AP_UInt)shader->program);
    shader->program = 0;
  }

  for (index = 0; index < shader->uniform_count; ++index) {
    free(shader->uniforms[index].name);
  }

  free(shader->uniforms);
  free(shader->name);
  free(shader);
}

void AP_ShaderDestroyInternal(AP_Shader *shader) { AP_ShaderFree(shader); }

void AP_DestroyShader(AP_Shader *shader) {
  if (shader == NULL) {
    return;
  }

  if (shader == AP_RendererGetBuiltinShader()) {
    AP_SET_ERROR(AP_ERROR_INVALID_ARGUMENT,
                 "Cannot destroy the default renderer shader");
    return;
  }

  if (AP_RendererGetUserShader() == shader) {
    AP_RendererSetUserShader(NULL);
  }

  AP_ShaderFree(shader);
}

bool AP_ShaderIsValid(const AP_Shader *shader) {
  return shader != NULL && shader->program != 0;
}

bool AP_ShaderIsCompute(const AP_Shader *shader) {
  return shader != NULL && shader->compute;
}

const char *AP_ShaderGetName(const AP_Shader *shader) {
  if (shader == NULL) {
    return NULL;
  }

  return shader->name;
}

AP_UInt AP_ShaderGetNativeHandle(const AP_Shader *shader) {
  if (shader == NULL) {
    return 0;
  }

  return (AP_UInt)shader->program;
}

AP_UInt AP_ShaderNativeProgram(const AP_Shader *shader) {
  return AP_ShaderGetNativeHandle(shader);
}

/* =========================================================
 * Creation
 * ========================================================= */

AP_Shader *AP_CreateShaderEx(const AP_ShaderConfig *config) {
  AP_ShaderScratch scratch;
  AP_OpenGLProgramConfig program_config;
  AP_OpenGLAttribBind *attrib_binds = NULL;
  AP_OpenGLAttribBind *frag_binds = NULL;
  AP_Shader *shader;
  AP_UInt program;
  bool has_compute;
  bool has_graphics;
  const char *vertex;
  const char *fragment;
  const char *geometry;
  const char *tess_control;
  const char *tess_eval;
  const char *compute;
  bool ok;

  if (config == NULL) {
    AP_SET_ERROR(AP_ERROR_INVALID_ARGUMENT, "Shader configuration cannot be NULL");
    return NULL;
  }

  has_compute = AP_ShaderStageProvided(&config->compute);
  has_graphics = AP_ShaderStageProvided(&config->vertex) ||
                 AP_ShaderStageProvided(&config->fragment) ||
                 AP_ShaderStageProvided(&config->geometry) ||
                 AP_ShaderStageProvided(&config->tess_control) ||
                 AP_ShaderStageProvided(&config->tess_eval);

  if (has_compute && has_graphics) {
    AP_SET_ERROR(AP_ERROR_INVALID_ARGUMENT,
                 "Compute programs cannot include graphics shader stages");
    return NULL;
  }

  if (!has_compute && !has_graphics) {
    AP_SET_ERROR(AP_ERROR_INVALID_ARGUMENT, "Shader configuration is empty");
    return NULL;
  }

  AP_ShaderScratchInit(&scratch);

  vertex = AP_ShaderResolveStage(&config->vertex,
                                 has_compute ? NULL : AP_SHADER_BUILTIN_VERTEX,
                                 &scratch, config->defines, config->define_count,
                                 &ok);
  if (!ok) {
    AP_ShaderScratchFree(&scratch);
    return NULL;
  }

  fragment =
      AP_ShaderResolveStage(&config->fragment,
                            has_compute ? NULL : AP_SHADER_BUILTIN_FRAGMENT,
                            &scratch, config->defines, config->define_count,
                            &ok);
  if (!ok) {
    AP_ShaderScratchFree(&scratch);
    return NULL;
  }

  geometry = AP_ShaderResolveStage(&config->geometry, NULL, &scratch,
                                   config->defines, config->define_count, &ok);
  if (!ok) {
    AP_ShaderScratchFree(&scratch);
    return NULL;
  }

  tess_control = AP_ShaderResolveStage(&config->tess_control, NULL, &scratch,
                                       config->defines, config->define_count,
                                       &ok);
  if (!ok) {
    AP_ShaderScratchFree(&scratch);
    return NULL;
  }

  tess_eval = AP_ShaderResolveStage(&config->tess_eval, NULL, &scratch,
                                    config->defines, config->define_count, &ok);
  if (!ok) {
    AP_ShaderScratchFree(&scratch);
    return NULL;
  }

  compute = AP_ShaderResolveStage(&config->compute, NULL, &scratch,
                                  config->defines, config->define_count, &ok);
  if (!ok) {
    AP_ShaderScratchFree(&scratch);
    return NULL;
  }

  program_config = AP_OpenGLDefaultProgramConfig();
  program_config.vertex_source = has_compute ? NULL : vertex;
  program_config.fragment_source = has_compute ? NULL : fragment;
  program_config.geometry_source = geometry;
  program_config.tess_control_source = tess_control;
  program_config.tess_eval_source = tess_eval;
  program_config.compute_source = compute;
  program_config.transform_feedback_varyings =
      config->transform_feedback_varyings;
  program_config.transform_feedback_count = config->transform_feedback_count;
  program_config.transform_feedback_interleaved =
      config->transform_feedback_mode != AP_TRANSFORM_FEEDBACK_SEPARATE;
  program_config.separable = config->separable;
  program_config.binary_retrievable = config->binary_retrievable;

  if (!AP_ShaderCopyBindings(config->attribs, config->attrib_count,
                             &attrib_binds) ||
      !AP_ShaderCopyBindings(config->frag_outputs, config->frag_output_count,
                             &frag_binds)) {
    free(attrib_binds);
    free(frag_binds);
    AP_ShaderScratchFree(&scratch);
    return NULL;
  }
  program_config.attribs = attrib_binds;
  program_config.attrib_count = attrib_binds != NULL ? config->attrib_count : 0;
  program_config.frag_outputs = frag_binds;
  program_config.frag_output_count =
      frag_binds != NULL ? config->frag_output_count : 0;

  program = AP_OpenGLCreateProgramEx(&program_config);

  free(attrib_binds);
  free(frag_binds);
  AP_ShaderScratchFree(&scratch);

  if (program == 0) {
    return NULL;
  }

  shader = (AP_Shader *)calloc(1, sizeof(AP_Shader));
  if (shader == NULL) {
    AP_OpenGLDeleteProgram(program);
    AP_SET_ERROR(AP_ERROR_OUT_OF_MEMORY, "Failed to allocate shader");
    return NULL;
  }

  shader->program = (GLuint)program;
  shader->compute = has_compute;
  shader->u_resolution = -1;
  shader->name = AP_ShaderCopyText(config->name);

  if (config->name != NULL && shader->name == NULL) {
    AP_ShaderFree(shader);
    AP_SET_ERROR(AP_ERROR_OUT_OF_MEMORY, "Failed to allocate shader");
    return NULL;
  }

  AP_INFO("Shader compiled%s%s", shader->name != NULL ? ": " : "",
          shader->name != NULL ? shader->name : "");
  return shader;
}

AP_Shader *AP_CreateShader(const char *vertex_source,
                           const char *fragment_source) {
  AP_ShaderConfig config;

  if (vertex_source == NULL && fragment_source == NULL) {
    AP_SET_ERROR(AP_ERROR_INVALID_ARGUMENT, "Shader source cannot be NULL");
    return NULL;
  }

  config = AP_ShaderDefaultConfig();
  config.vertex.source = vertex_source;
  config.fragment.source = fragment_source;
  return AP_CreateShaderEx(&config);
}

AP_Shader *AP_CreateShaderFromFile(const char *vertex_path,
                                   const char *fragment_path) {
  AP_ShaderConfig config;

  if (vertex_path == NULL && fragment_path == NULL) {
    AP_SET_ERROR(AP_ERROR_INVALID_ARGUMENT, "Shader path cannot be NULL");
    return NULL;
  }

  config = AP_ShaderDefaultConfig();
  config.vertex.path = vertex_path;
  config.fragment.path = fragment_path;
  return AP_CreateShaderEx(&config);
}

AP_Shader *AP_CreateComputeShader(const char *compute_source) {
  AP_ShaderConfig config;

  if (compute_source == NULL) {
    AP_SET_ERROR(AP_ERROR_INVALID_ARGUMENT, "Compute source cannot be NULL");
    return NULL;
  }

  config = AP_ShaderDefaultConfig();
  config.compute.source = compute_source;
  return AP_CreateShaderEx(&config);
}

/* =========================================================
 * Binding
 * ========================================================= */

bool AP_UseShader(AP_Shader *shader) {
  if (shader != NULL) {
    if (!AP_ShaderIsValid(shader)) {
      AP_SET_ERROR(AP_ERROR_INVALID_ARGUMENT, "Shader is not valid");
      return false;
    }

    if (shader->compute) {
      AP_SET_ERROR(AP_ERROR_INVALID_ARGUMENT,
                   "Compute shaders cannot be used for immediate drawing");
      return false;
    }
  }

  return AP_RendererSetUserShader(shader);
}

AP_Shader *AP_GetShader(void) { return AP_RendererGetUserShader(); }

bool AP_ResetShader(void) { return AP_UseShader(NULL); }

bool AP_ShaderBind(AP_Shader *shader) {
  if (!AP_ShaderIsValid(shader)) {
    AP_SET_ERROR(AP_ERROR_INVALID_ARGUMENT, "Shader is not valid");
    return false;
  }

  glUseProgram(shader->program);
  return true;
}

bool AP_ShaderUnbind(void) {
  glUseProgram(0);
  return true;
}

bool AP_DispatchCompute(AP_UInt groups_x, AP_UInt groups_y, AP_UInt groups_z) {
  GLint program = 0;

  if (groups_x == 0 || groups_y == 0 || groups_z == 0) {
    AP_SET_ERROR(AP_ERROR_INVALID_ARGUMENT, "Compute group counts must be positive");
    return false;
  }

  glGetIntegerv(GL_CURRENT_PROGRAM, &program);
  if (program == 0) {
    AP_SET_ERROR(AP_ERROR_INVALID_STATE, "No compute shader is bound");
    return false;
  }

  glDispatchCompute(groups_x, groups_y, groups_z);
  return true;
}

/* =========================================================
 * Locations
 * ========================================================= */

static GLint AP_ShaderLookupUniform(AP_Shader *shader, const char *name) {
  AP_ShaderUniformEntry *entries;
  AP_ShaderUniformEntry *entry;
  GLint location;
  int index;
  int capacity;

  for (index = 0; index < shader->uniform_count; ++index) {
    if (strcmp(shader->uniforms[index].name, name) == 0) {
      return shader->uniforms[index].location;
    }
  }

  location = glGetUniformLocation(shader->program, name);

  if (shader->uniform_count == shader->uniform_capacity) {
    capacity = shader->uniform_capacity == 0 ? AP_SHADER_UNIFORM_GROW
                                             : shader->uniform_capacity * 2;
    entries = (AP_ShaderUniformEntry *)realloc(
        shader->uniforms, (size_t)capacity * sizeof(*entries));
    if (entries == NULL) {
      return location;
    }

    shader->uniforms = entries;
    shader->uniform_capacity = capacity;
  }

  entry = &shader->uniforms[shader->uniform_count];
  entry->name = AP_ShaderCopyText(name);
  if (entry->name == NULL) {
    return location;
  }

  entry->location = location;
  shader->uniform_count += 1;
  return location;
}

AP_Int AP_ShaderResolutionUniform(AP_Shader *shader) {
  if (shader == NULL) {
    return -1;
  }

  if (!shader->u_resolution_queried) {
    shader->u_resolution = AP_ShaderLookupUniform(shader, "u_resolution");
    shader->u_resolution_queried = true;
  }

  return (AP_Int)shader->u_resolution;
}

AP_Int AP_ShaderTextureUniform(AP_Shader *shader) {
  if (shader == NULL) {
    return -1;
  }

  if (!shader->u_texture_queried) {
    shader->u_texture = AP_ShaderLookupUniform(shader, "u_texture");
    shader->u_texture_queried = true;
  }

  return (AP_Int)shader->u_texture;
}

AP_Int AP_ShaderGetUniformLocation(AP_Shader *shader, const char *name) {
  if (!AP_ShaderIsValid(shader) || name == NULL) {
    AP_SET_ERROR(AP_ERROR_INVALID_ARGUMENT, "Invalid shader uniform query");
    return -1;
  }

  return (AP_Int)AP_ShaderLookupUniform(shader, name);
}

AP_Int AP_ShaderGetAttribLocation(AP_Shader *shader, const char *name) {
  if (!AP_ShaderIsValid(shader) || name == NULL) {
    AP_SET_ERROR(AP_ERROR_INVALID_ARGUMENT, "Invalid shader attribute query");
    return -1;
  }

  return (AP_Int)glGetAttribLocation(shader->program, name);
}

AP_Int AP_ShaderGetUniformBlockIndex(AP_Shader *shader, const char *name) {
  GLuint index;

  if (!AP_ShaderIsValid(shader) || name == NULL) {
    AP_SET_ERROR(AP_ERROR_INVALID_ARGUMENT, "Invalid uniform block query");
    return -1;
  }

  index = glGetUniformBlockIndex(shader->program, name);
  if (index == GL_INVALID_INDEX) {
    return -1;
  }

  return (AP_Int)index;
}

bool AP_ShaderSetUniformBlockBinding(AP_Shader *shader, const char *name,
                                     AP_UInt binding) {
  AP_Int index = AP_ShaderGetUniformBlockIndex(shader, name);

  if (index < 0) {
    AP_SET_ERROR(AP_ERROR_NOT_FOUND, "Uniform block not found");
    return false;
  }

  glUniformBlockBinding(shader->program, (GLuint)index, binding);
  return true;
}

/* =========================================================
 * Uniform writes
 * ========================================================= */

static AP_Shader *AP_ShaderCurrent(void) {
  AP_Shader *shader = AP_RendererGetUserShader();

  if (shader != NULL) {
    return shader;
  }

  return AP_RendererGetBuiltinShader();
}

static bool AP_ShaderPrepareUniform(AP_Shader *shader, const char *name,
                                    GLint *location) {
  if (!AP_ShaderIsValid(shader)) {
    AP_SET_ERROR(AP_ERROR_INVALID_ARGUMENT, "Shader is not valid");
    return false;
  }

  if (name == NULL || location == NULL) {
    AP_SET_ERROR(AP_ERROR_INVALID_ARGUMENT, "Uniform name cannot be NULL");
    return false;
  }

  *location = AP_ShaderLookupUniform(shader, name);
  if (*location >= 0) {
    glUseProgram(shader->program);
  }

  return true;
}

bool AP_ShaderSetUniformF(AP_Shader *shader, const char *name, AP_F32 x) {
  GLint location;

  if (!AP_ShaderPrepareUniform(shader, name, &location)) {
    return false;
  }

  if (location >= 0) {
    glUniform1f(location, x);
  }

  return true;
}

bool AP_ShaderSetUniformF2(AP_Shader *shader, const char *name, AP_F32 x,
                           AP_F32 y) {
  GLint location;

  if (!AP_ShaderPrepareUniform(shader, name, &location)) {
    return false;
  }

  if (location >= 0) {
    glUniform2f(location, x, y);
  }

  return true;
}

bool AP_ShaderSetUniformF3(AP_Shader *shader, const char *name, AP_F32 x,
                           AP_F32 y, AP_F32 z) {
  GLint location;

  if (!AP_ShaderPrepareUniform(shader, name, &location)) {
    return false;
  }

  if (location >= 0) {
    glUniform3f(location, x, y, z);
  }

  return true;
}

bool AP_ShaderSetUniformF4(AP_Shader *shader, const char *name, AP_F32 x,
                           AP_F32 y, AP_F32 z, AP_F32 w) {
  GLint location;

  if (!AP_ShaderPrepareUniform(shader, name, &location)) {
    return false;
  }

  if (location >= 0) {
    glUniform4f(location, x, y, z, w);
  }

  return true;
}

bool AP_ShaderSetUniformI(AP_Shader *shader, const char *name, AP_Int x) {
  GLint location;

  if (!AP_ShaderPrepareUniform(shader, name, &location)) {
    return false;
  }

  if (location >= 0) {
    glUniform1i(location, x);
  }

  return true;
}

bool AP_ShaderSetUniformI2(AP_Shader *shader, const char *name, AP_Int x,
                           AP_Int y) {
  GLint location;

  if (!AP_ShaderPrepareUniform(shader, name, &location)) {
    return false;
  }

  if (location >= 0) {
    glUniform2i(location, x, y);
  }

  return true;
}

bool AP_ShaderSetUniformI3(AP_Shader *shader, const char *name, AP_Int x,
                           AP_Int y, AP_Int z) {
  GLint location;

  if (!AP_ShaderPrepareUniform(shader, name, &location)) {
    return false;
  }

  if (location >= 0) {
    glUniform3i(location, x, y, z);
  }

  return true;
}

bool AP_ShaderSetUniformI4(AP_Shader *shader, const char *name, AP_Int x,
                           AP_Int y, AP_Int z, AP_Int w) {
  GLint location;

  if (!AP_ShaderPrepareUniform(shader, name, &location)) {
    return false;
  }

  if (location >= 0) {
    glUniform4i(location, x, y, z, w);
  }

  return true;
}

bool AP_ShaderSetUniformU(AP_Shader *shader, const char *name, AP_UInt x) {
  GLint location;

  if (!AP_ShaderPrepareUniform(shader, name, &location)) {
    return false;
  }

  if (location >= 0) {
    glUniform1ui(location, x);
  }

  return true;
}

bool AP_ShaderSetUniformU2(AP_Shader *shader, const char *name, AP_UInt x,
                           AP_UInt y) {
  GLint location;

  if (!AP_ShaderPrepareUniform(shader, name, &location)) {
    return false;
  }

  if (location >= 0) {
    glUniform2ui(location, x, y);
  }

  return true;
}

bool AP_ShaderSetUniformU3(AP_Shader *shader, const char *name, AP_UInt x,
                           AP_UInt y, AP_UInt z) {
  GLint location;

  if (!AP_ShaderPrepareUniform(shader, name, &location)) {
    return false;
  }

  if (location >= 0) {
    glUniform3ui(location, x, y, z);
  }

  return true;
}

bool AP_ShaderSetUniformU4(AP_Shader *shader, const char *name, AP_UInt x,
                           AP_UInt y, AP_UInt z, AP_UInt w) {
  GLint location;

  if (!AP_ShaderPrepareUniform(shader, name, &location)) {
    return false;
  }

  if (location >= 0) {
    glUniform4ui(location, x, y, z, w);
  }

  return true;
}

bool AP_ShaderSetUniformVec2(AP_Shader *shader, const char *name,
                             AP_Vec2 value) {
  return AP_ShaderSetUniformF2(shader, name, value.x, value.y);
}

bool AP_ShaderSetUniformVec3(AP_Shader *shader, const char *name,
                             AP_Vec3 value) {
  return AP_ShaderSetUniformF3(shader, name, value.x, value.y, value.z);
}

bool AP_ShaderSetUniformVec4(AP_Shader *shader, const char *name,
                             AP_Vec4 value) {
  return AP_ShaderSetUniformF4(shader, name, value.x, value.y, value.z, value.w);
}

bool AP_ShaderSetUniformColor(AP_Shader *shader, const char *name,
                              AP_Color value) {
  return AP_ShaderSetUniformF4(shader, name, value.r, value.g, value.b, value.a);
}

bool AP_ShaderSetUniformMat3(AP_Shader *shader, const char *name,
                             const AP_F32 *matrix, bool transpose) {
  GLint location;

  if (matrix == NULL) {
    AP_SET_ERROR(AP_ERROR_INVALID_ARGUMENT, "Matrix cannot be NULL");
    return false;
  }

  if (!AP_ShaderPrepareUniform(shader, name, &location)) {
    return false;
  }

  if (location >= 0) {
    glUniformMatrix3fv(location, 1, transpose ? GL_TRUE : GL_FALSE, matrix);
  }

  return true;
}

bool AP_ShaderSetUniformMat4(AP_Shader *shader, const char *name,
                             const AP_F32 *matrix, bool transpose) {
  GLint location;

  if (matrix == NULL) {
    AP_SET_ERROR(AP_ERROR_INVALID_ARGUMENT, "Matrix cannot be NULL");
    return false;
  }

  if (!AP_ShaderPrepareUniform(shader, name, &location)) {
    return false;
  }

  if (location >= 0) {
    glUniformMatrix4fv(location, 1, transpose ? GL_TRUE : GL_FALSE, matrix);
  }

  return true;
}

bool AP_ShaderSetUniformN(AP_Shader *shader, const char *name,
                          AP_UniformType type, const void *value, int count) {
  GLint location;
  GLboolean transpose = GL_FALSE;

  if (value == NULL || count <= 0) {
    AP_SET_ERROR(AP_ERROR_INVALID_ARGUMENT, "Uniform value cannot be NULL");
    return false;
  }

  if (!AP_ShaderPrepareUniform(shader, name, &location)) {
    return false;
  }

  if (location < 0) {
    return true;
  }

  switch (type) {
  case AP_UNIFORM_F1:
    glUniform1fv(location, count, (const GLfloat *)value);
    break;
  case AP_UNIFORM_F2:
    glUniform2fv(location, count, (const GLfloat *)value);
    break;
  case AP_UNIFORM_F3:
    glUniform3fv(location, count, (const GLfloat *)value);
    break;
  case AP_UNIFORM_F4:
    glUniform4fv(location, count, (const GLfloat *)value);
    break;
  case AP_UNIFORM_I1:
    glUniform1iv(location, count, (const GLint *)value);
    break;
  case AP_UNIFORM_I2:
    glUniform2iv(location, count, (const GLint *)value);
    break;
  case AP_UNIFORM_I3:
    glUniform3iv(location, count, (const GLint *)value);
    break;
  case AP_UNIFORM_I4:
    glUniform4iv(location, count, (const GLint *)value);
    break;
  case AP_UNIFORM_U1:
    glUniform1uiv(location, count, (const GLuint *)value);
    break;
  case AP_UNIFORM_U2:
    glUniform2uiv(location, count, (const GLuint *)value);
    break;
  case AP_UNIFORM_U3:
    glUniform3uiv(location, count, (const GLuint *)value);
    break;
  case AP_UNIFORM_U4:
    glUniform4uiv(location, count, (const GLuint *)value);
    break;
  case AP_UNIFORM_MAT2:
    glUniformMatrix2fv(location, count, transpose, (const GLfloat *)value);
    break;
  case AP_UNIFORM_MAT3:
    glUniformMatrix3fv(location, count, transpose, (const GLfloat *)value);
    break;
  case AP_UNIFORM_MAT4:
    glUniformMatrix4fv(location, count, transpose, (const GLfloat *)value);
    break;
  case AP_UNIFORM_MAT2X3:
    glUniformMatrix2x3fv(location, count, transpose, (const GLfloat *)value);
    break;
  case AP_UNIFORM_MAT3X2:
    glUniformMatrix3x2fv(location, count, transpose, (const GLfloat *)value);
    break;
  case AP_UNIFORM_MAT2X4:
    glUniformMatrix2x4fv(location, count, transpose, (const GLfloat *)value);
    break;
  case AP_UNIFORM_MAT4X2:
    glUniformMatrix4x2fv(location, count, transpose, (const GLfloat *)value);
    break;
  case AP_UNIFORM_MAT3X4:
    glUniformMatrix3x4fv(location, count, transpose, (const GLfloat *)value);
    break;
  case AP_UNIFORM_MAT4X3:
    glUniformMatrix4x3fv(location, count, transpose, (const GLfloat *)value);
    break;
  default:
    AP_SET_ERROR(AP_ERROR_INVALID_ARGUMENT, "Unknown uniform type");
    return false;
  }

  return true;
}

bool AP_ShaderSetUniform(AP_Shader *shader, const char *name,
                         AP_UniformType type, const void *value) {
  return AP_ShaderSetUniformN(shader, name, type, value, 1);
}

bool AP_SetUniformF(const char *name, AP_F32 x) {
  return AP_ShaderSetUniformF(AP_ShaderCurrent(), name, x);
}

bool AP_SetUniformF2(const char *name, AP_F32 x, AP_F32 y) {
  return AP_ShaderSetUniformF2(AP_ShaderCurrent(), name, x, y);
}

bool AP_SetUniformF3(const char *name, AP_F32 x, AP_F32 y, AP_F32 z) {
  return AP_ShaderSetUniformF3(AP_ShaderCurrent(), name, x, y, z);
}

bool AP_SetUniformF4(const char *name, AP_F32 x, AP_F32 y, AP_F32 z, AP_F32 w) {
  return AP_ShaderSetUniformF4(AP_ShaderCurrent(), name, x, y, z, w);
}

bool AP_SetUniformI(const char *name, AP_Int x) {
  return AP_ShaderSetUniformI(AP_ShaderCurrent(), name, x);
}

bool AP_SetUniformI2(const char *name, AP_Int x, AP_Int y) {
  return AP_ShaderSetUniformI2(AP_ShaderCurrent(), name, x, y);
}

bool AP_SetUniformI3(const char *name, AP_Int x, AP_Int y, AP_Int z) {
  return AP_ShaderSetUniformI3(AP_ShaderCurrent(), name, x, y, z);
}

bool AP_SetUniformI4(const char *name, AP_Int x, AP_Int y, AP_Int z, AP_Int w) {
  return AP_ShaderSetUniformI4(AP_ShaderCurrent(), name, x, y, z, w);
}

bool AP_SetUniformU(const char *name, AP_UInt x) {
  return AP_ShaderSetUniformU(AP_ShaderCurrent(), name, x);
}

bool AP_SetUniformU2(const char *name, AP_UInt x, AP_UInt y) {
  return AP_ShaderSetUniformU2(AP_ShaderCurrent(), name, x, y);
}

bool AP_SetUniformU3(const char *name, AP_UInt x, AP_UInt y, AP_UInt z) {
  return AP_ShaderSetUniformU3(AP_ShaderCurrent(), name, x, y, z);
}

bool AP_SetUniformU4(const char *name, AP_UInt x, AP_UInt y, AP_UInt z,
                     AP_UInt w) {
  return AP_ShaderSetUniformU4(AP_ShaderCurrent(), name, x, y, z, w);
}

bool AP_SetUniformVec2(const char *name, AP_Vec2 value) {
  return AP_ShaderSetUniformVec2(AP_ShaderCurrent(), name, value);
}

bool AP_SetUniformVec3(const char *name, AP_Vec3 value) {
  return AP_ShaderSetUniformVec3(AP_ShaderCurrent(), name, value);
}

bool AP_SetUniformVec4(const char *name, AP_Vec4 value) {
  return AP_ShaderSetUniformVec4(AP_ShaderCurrent(), name, value);
}

bool AP_SetUniformColor(const char *name, AP_Color value) {
  return AP_ShaderSetUniformColor(AP_ShaderCurrent(), name, value);
}

bool AP_SetUniformMat3(const char *name, const AP_F32 *matrix, bool transpose) {
  return AP_ShaderSetUniformMat3(AP_ShaderCurrent(), name, matrix, transpose);
}

bool AP_SetUniformMat4(const char *name, const AP_F32 *matrix, bool transpose) {
  return AP_ShaderSetUniformMat4(AP_ShaderCurrent(), name, matrix, transpose);
}

bool AP_SetUniform(const char *name, AP_UniformType type, const void *value) {
  return AP_ShaderSetUniform(AP_ShaderCurrent(), name, type, value);
}

bool AP_SetUniformN(const char *name, AP_UniformType type, const void *value,
                    int count) {
  return AP_ShaderSetUniformN(AP_ShaderCurrent(), name, type, value, count);
}
