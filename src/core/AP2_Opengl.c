#include "AP2/AP2_Opengl.h"

#include "AP2/AP2_Error.h"
#include "AP2/AP2_Logger.h"

#define GLFW_INCLUDE_NONE
#include <glad/gl.h>
#include <GLFW/glfw3.h>

#include <stdio.h>
#include <string.h>

/* =========================================================
 * Internal state
 * ========================================================= */

static bool g_opengl_initialized = false;
static AP_OpenGLConfig g_opengl_config;
static AP_OpenGLInfo g_opengl_info;
static AP_OpenGLLimits g_opengl_limits;
static AP_OpenGLCapabilities g_opengl_capabilities;

static char g_vendor[128];
static char g_renderer[128];
static char g_version[128];
static char g_glsl[128];

static float g_clear_color[4] = {0.0f, 0.0f, 0.0f, 1.0f};
static int g_viewport[4] = {0, 0, 0, 0};
static bool g_scissor_enabled = false;
static bool g_depth_test = false;
static bool g_blending = false;

/* =========================================================
 * Helpers
 * ========================================================= */

static bool AP_OpenGLEnsure(void) {
  if (!g_opengl_initialized) {
    AP_SET_ERROR(AP_ERROR_NOT_INITIALIZED, "OpenGL backend is not initialized");
    return false;
  }

  if (glfwGetCurrentContext() == NULL) {
    AP_SET_ERROR(AP_ERROR_OPERATION_FAILED, "No active OpenGL context");
    return false;
  }

  return true;
}

static void AP_OpenGLCopyString(char *destination, size_t capacity,
                               const GLubyte *source) {
  memset(destination, 0, capacity);

  if (source == NULL || capacity == 0) {
    return;
  }

  strncpy(destination, (const char *)source, capacity - 1);
}

static int AP_OpenGLGetInt(GLenum pname) {
  GLint value = 0;
  glGetIntegerv(pname, &value);
  return (int)value;
}

static void AP_OpenGLQueryState(void) {
  AP_OpenGLCopyString(g_vendor, sizeof(g_vendor), glGetString(GL_VENDOR));
  AP_OpenGLCopyString(g_renderer, sizeof(g_renderer), glGetString(GL_RENDERER));
  AP_OpenGLCopyString(g_version, sizeof(g_version), glGetString(GL_VERSION));
  AP_OpenGLCopyString(g_glsl, sizeof(g_glsl),
                      glGetString(GL_SHADING_LANGUAGE_VERSION));

  memset(&g_opengl_info, 0, sizeof(g_opengl_info));
  g_opengl_info.vendor = g_vendor;
  g_opengl_info.renderer = g_renderer;
  g_opengl_info.version = g_version;
  g_opengl_info.glsl_version = g_glsl;

  sscanf(g_version, "%u.%u", &g_opengl_info.version_number.major,
         &g_opengl_info.version_number.minor);

  {
    GLint profile = 0;
    glGetIntegerv(GL_CONTEXT_PROFILE_MASK, &profile);
    g_opengl_info.core_profile =
        (profile & GL_CONTEXT_CORE_PROFILE_BIT) != 0;
  }

  {
    GLint flags = 0;
    glGetIntegerv(GL_CONTEXT_FLAGS, &flags);
    g_opengl_info.debug_context = (flags & GL_CONTEXT_FLAG_DEBUG_BIT) != 0;
  }

  memset(&g_opengl_limits, 0, sizeof(g_opengl_limits));
  g_opengl_limits.max_texture_size = AP_OpenGLGetInt(GL_MAX_TEXTURE_SIZE);
  g_opengl_limits.max_3d_texture_size = AP_OpenGLGetInt(GL_MAX_3D_TEXTURE_SIZE);
  g_opengl_limits.max_cube_map_texture_size =
      AP_OpenGLGetInt(GL_MAX_CUBE_MAP_TEXTURE_SIZE);
  g_opengl_limits.max_texture_image_units =
      AP_OpenGLGetInt(GL_MAX_TEXTURE_IMAGE_UNITS);
  g_opengl_limits.max_combined_texture_image_units =
      AP_OpenGLGetInt(GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS);
  g_opengl_limits.max_vertex_attribs = AP_OpenGLGetInt(GL_MAX_VERTEX_ATTRIBS);
  g_opengl_limits.max_draw_buffers = AP_OpenGLGetInt(GL_MAX_DRAW_BUFFERS);
  g_opengl_limits.max_color_attachments =
      AP_OpenGLGetInt(GL_MAX_COLOR_ATTACHMENTS);
  g_opengl_limits.max_samples = AP_OpenGLGetInt(GL_MAX_SAMPLES);
  g_opengl_limits.max_uniform_buffer_bindings =
      AP_OpenGLGetInt(GL_MAX_UNIFORM_BUFFER_BINDINGS);
  g_opengl_limits.max_uniform_block_size =
      AP_OpenGLGetInt(GL_MAX_UNIFORM_BLOCK_SIZE);
  g_opengl_limits.max_vertex_uniform_components =
      AP_OpenGLGetInt(GL_MAX_VERTEX_UNIFORM_COMPONENTS);
  g_opengl_limits.max_fragment_uniform_components =
      AP_OpenGLGetInt(GL_MAX_FRAGMENT_UNIFORM_COMPONENTS);

  memset(&g_opengl_capabilities, 0, sizeof(g_opengl_capabilities));
  g_opengl_capabilities.shaders = true;
  g_opengl_capabilities.vertex_arrays = true;
  g_opengl_capabilities.framebuffer_objects = true;
  g_opengl_capabilities.multisampling = g_opengl_limits.max_samples > 1;
  g_opengl_capabilities.instancing =
      g_opengl_info.version_number.major > 3 ||
      (g_opengl_info.version_number.major == 3 &&
       g_opengl_info.version_number.minor >= 3);
  g_opengl_capabilities.uniform_buffers =
      g_opengl_info.version_number.major > 3 ||
      (g_opengl_info.version_number.major == 3 &&
       g_opengl_info.version_number.minor >= 1);
  g_opengl_capabilities.debug_output = g_opengl_info.debug_context;
  g_opengl_capabilities.geometry_shaders =
      g_opengl_info.version_number.major > 3 ||
      (g_opengl_info.version_number.major == 3 &&
       g_opengl_info.version_number.minor >= 2);
  g_opengl_capabilities.tessellation_shaders =
      g_opengl_info.version_number.major >= 4;
  g_opengl_capabilities.compute_shaders =
      g_opengl_info.version_number.major > 4 ||
      (g_opengl_info.version_number.major == 4 &&
       g_opengl_info.version_number.minor >= 3);
}

static const char *AP_OpenGLShaderTypeName(GLenum type) {
  switch (type) {
  case GL_VERTEX_SHADER:
    return "vertex";
  case GL_FRAGMENT_SHADER:
    return "fragment";
  case GL_GEOMETRY_SHADER:
    return "geometry";
  case GL_TESS_CONTROL_SHADER:
    return "tessellation control";
  case GL_TESS_EVALUATION_SHADER:
    return "tessellation evaluation";
  case GL_COMPUTE_SHADER:
    return "compute";
  default:
    return "unknown";
  }
}

static GLuint AP_OpenGLCompileShader(GLenum type, const char *source) {
  GLuint shader;
  GLint status = GL_FALSE;
  char log[4096];

  shader = glCreateShader(type);
  glShaderSource(shader, 1, &source, NULL);
  glCompileShader(shader);
  glGetShaderiv(shader, GL_COMPILE_STATUS, &status);

  if (status != GL_TRUE) {
    glGetShaderInfoLog(shader, (GLsizei)sizeof(log), NULL, log);
    AP_ERROR("%s shader compilation failed: %s", AP_OpenGLShaderTypeName(type),
             log);
    AP_SET_ERROR(AP_ERROR_OPERATION_FAILED, "Shader compilation failed");
    glDeleteShader(shader);
    return 0;
  }

  return shader;
}

static void AP_OpenGLDeleteShaderList(GLuint *shaders, int count) {
  int index;

  for (index = 0; index < count; ++index) {
    if (shaders[index] != 0) {
      glDeleteShader(shaders[index]);
      shaders[index] = 0;
    }
  }
}

static bool AP_OpenGLAttachStage(GLuint program, GLenum type, const char *source,
                                 GLuint *shaders, int *shader_count) {
  GLuint shader;

  if (source == NULL) {
    return true;
  }

  shader = AP_OpenGLCompileShader(type, source);
  if (shader == 0) {
    return false;
  }

  glAttachShader(program, shader);
  shaders[*shader_count] = shader;
  *shader_count += 1;
  return true;
}

/* =========================================================
 * Configuration
 * ========================================================= */

AP_OpenGLConfig AP_OpenGLDefaultConfig(void) {
  AP_OpenGLConfig config;

  memset(&config, 0, sizeof(config));
  config.major_version = 4;
  config.minor_version = 6;
  config.debug = false;
  config.vsync = true;
  config.double_buffer = true;
  config.multisample = true;
  config.multisample_samples = 4;
  return config;
}

/* =========================================================
 * Initialization
 * ========================================================= */

bool AP_OpenGLInit(const AP_OpenGLConfig *config) {
  GLFWwindow *context;

  if (g_opengl_initialized) {
    return true;
  }

  g_opengl_config = config != NULL ? *config : AP_OpenGLDefaultConfig();

  context = glfwGetCurrentContext();
  if (context == NULL) {
    AP_SET_ERROR(AP_ERROR_OPERATION_FAILED, "No active OpenGL context");
    return false;
  }

  if (!gladLoadGL((GLADloadfunc)glfwGetProcAddress)) {
    AP_SET_ERROR(AP_ERROR_INITIALIZATION_FAILED, "Failed to load OpenGL functions");
    return false;
  }

  AP_OpenGLQueryState();

  if (g_opengl_info.version_number.major < 3 ||
      (g_opengl_info.version_number.major == 3 &&
       g_opengl_info.version_number.minor < 3)) {
    AP_SET_ERROR(AP_ERROR_UNSUPPORTED, "OpenGL 3.3 or newer is required");
    return false;
  }

  glfwSwapInterval(g_opengl_config.vsync ? 1 : 0);

  glGetIntegerv(GL_VIEWPORT, g_viewport);
  glDisable(GL_DEPTH_TEST);
  glDisable(GL_CULL_FACE);
  glEnable(GL_BLEND);
  glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE,
                      GL_ONE_MINUS_SRC_ALPHA);
  g_blending = true;
  g_depth_test = false;

  if (g_opengl_config.multisample && g_opengl_capabilities.multisampling) {
    glEnable(GL_MULTISAMPLE);
  }

  g_opengl_initialized = true;

  AP_INFO("OpenGL initialized: %s / %s / %s", g_vendor, g_renderer, g_version);
  return true;
}

void AP_OpenGLClose(void) {
  if (!g_opengl_initialized) {
    return;
  }

  AP_INFO("Closing OpenGL backend");
  memset(&g_opengl_config, 0, sizeof(g_opengl_config));
  memset(&g_opengl_info, 0, sizeof(g_opengl_info));
  memset(&g_opengl_limits, 0, sizeof(g_opengl_limits));
  memset(&g_opengl_capabilities, 0, sizeof(g_opengl_capabilities));
  g_opengl_initialized = false;
}

bool AP_OpenGLIsInitialized(void) { return g_opengl_initialized; }

/* =========================================================
 * Context
 * ========================================================= */

bool AP_OpenGLMakeContextCurrent(void *glfw_window) {
  GLFWwindow *window = (GLFWwindow *)glfw_window;

  if (window == NULL) {
    AP_SET_ERROR(AP_ERROR_INVALID_ARGUMENT, "GLFW window cannot be NULL");
    return false;
  }

  glfwMakeContextCurrent(window);

  if (glfwGetCurrentContext() != window) {
    AP_SET_ERROR(AP_ERROR_OPERATION_FAILED,
                 "Failed to make OpenGL context current");
    return false;
  }

  return true;
}

bool AP_OpenGLHasContext(void) { return glfwGetCurrentContext() != NULL; }

void *AP_OpenGLGetCurrentContext(void) {
  return (void *)glfwGetCurrentContext();
}

const AP_OpenGLConfig *AP_OpenGLGetConfig(void) {
  return g_opengl_initialized ? &g_opengl_config : NULL;
}

bool AP_OpenGLSetVSync(bool enabled) {
  if (!AP_OpenGLEnsure()) {
    return false;
  }

  glfwSwapInterval(enabled ? 1 : 0);
  g_opengl_config.vsync = enabled;
  return true;
}

bool AP_OpenGLGetVSync(void) {
  return g_opengl_initialized && g_opengl_config.vsync;
}

/* =========================================================
 * Information
 * ========================================================= */

const AP_OpenGLInfo *AP_OpenGLGetInfo(void) {
  return g_opengl_initialized ? &g_opengl_info : NULL;
}

AP_OpenGLVersion AP_OpenGLGetVersion(void) {
  AP_OpenGLVersion version = {0, 0, 0};

  if (g_opengl_initialized) {
    version = g_opengl_info.version_number;
  }

  return version;
}

const char *AP_OpenGLGetVendor(void) {
  return g_opengl_initialized ? g_vendor : NULL;
}

const char *AP_OpenGLGetRenderer(void) {
  return g_opengl_initialized ? g_renderer : NULL;
}

const char *AP_OpenGLGetVersionString(void) {
  return g_opengl_initialized ? g_version : NULL;
}

const char *AP_OpenGLGetGLSLVersion(void) {
  return g_opengl_initialized ? g_glsl : NULL;
}

const AP_OpenGLCapabilities *AP_OpenGLGetCapabilities(void) {
  return g_opengl_initialized ? &g_opengl_capabilities : NULL;
}

const AP_OpenGLLimits *AP_OpenGLGetLimits(void) {
  return g_opengl_initialized ? &g_opengl_limits : NULL;
}

/* =========================================================
 * Clear / viewport / scissor
 * ========================================================= */

bool AP_OpenGLClear(bool color, bool depth, bool stencil) {
  GLbitfield mask = 0;

  if (!AP_OpenGLEnsure()) {
    return false;
  }

  if (color) {
    mask |= GL_COLOR_BUFFER_BIT;
  }

  if (depth) {
    mask |= GL_DEPTH_BUFFER_BIT;
  }

  if (stencil) {
    mask |= GL_STENCIL_BUFFER_BIT;
  }

  if (mask != 0) {
    glClear(mask);
  }

  return true;
}

bool AP_OpenGLSetClearColor(float red, float green, float blue, float alpha) {
  if (!AP_OpenGLEnsure()) {
    return false;
  }

  g_clear_color[0] = red;
  g_clear_color[1] = green;
  g_clear_color[2] = blue;
  g_clear_color[3] = alpha;
  glClearColor(red, green, blue, alpha);
  return true;
}

bool AP_OpenGLGetClearColor(float *red, float *green, float *blue,
                            float *alpha) {
  if (!g_opengl_initialized) {
    return false;
  }

  if (red != NULL) {
    *red = g_clear_color[0];
  }

  if (green != NULL) {
    *green = g_clear_color[1];
  }

  if (blue != NULL) {
    *blue = g_clear_color[2];
  }

  if (alpha != NULL) {
    *alpha = g_clear_color[3];
  }

  return true;
}

bool AP_OpenGLSetViewport(int x, int y, int width, int height) {
  if (!AP_OpenGLEnsure()) {
    return false;
  }

  if (width < 0 || height < 0) {
    AP_SET_ERROR(AP_ERROR_INVALID_ARGUMENT,
                 "Viewport dimensions cannot be negative");
    return false;
  }

  g_viewport[0] = x;
  g_viewport[1] = y;
  g_viewport[2] = width;
  g_viewport[3] = height;
  glViewport(x, y, width, height);
  return true;
}

bool AP_OpenGLGetViewport(int *x, int *y, int *width, int *height) {
  if (!g_opengl_initialized) {
    return false;
  }

  if (x != NULL) {
    *x = g_viewport[0];
  }

  if (y != NULL) {
    *y = g_viewport[1];
  }

  if (width != NULL) {
    *width = g_viewport[2];
  }

  if (height != NULL) {
    *height = g_viewport[3];
  }

  return true;
}

bool AP_OpenGLSetScissor(int x, int y, int width, int height) {
  if (!AP_OpenGLEnsure()) {
    return false;
  }

  glScissor(x, y, width, height);
  return true;
}

bool AP_OpenGLSetScissorEnabled(bool enabled) {
  if (!AP_OpenGLEnsure()) {
    return false;
  }

  if (enabled) {
    glEnable(GL_SCISSOR_TEST);
  } else {
    glDisable(GL_SCISSOR_TEST);
  }

  g_scissor_enabled = enabled;
  return true;
}

bool AP_OpenGLGetScissorEnabled(void) { return g_scissor_enabled; }

bool AP_OpenGLSetDepthTest(bool enabled) {
  if (!AP_OpenGLEnsure()) {
    return false;
  }

  if (enabled) {
    glEnable(GL_DEPTH_TEST);
  } else {
    glDisable(GL_DEPTH_TEST);
  }

  g_depth_test = enabled;
  return true;
}

bool AP_OpenGLGetDepthTest(void) { return g_depth_test; }

bool AP_OpenGLSetBlending(bool enabled) {
  if (!AP_OpenGLEnsure()) {
    return false;
  }

  if (enabled) {
    glEnable(GL_BLEND);
  } else {
    glDisable(GL_BLEND);
  }

  g_blending = enabled;
  return true;
}

bool AP_OpenGLGetBlending(void) { return g_blending; }

bool AP_OpenGLSetBlendFunc(AP_UInt source, AP_UInt destination) {
  if (!AP_OpenGLEnsure()) {
    return false;
  }

  glBlendFunc((GLenum)source, (GLenum)destination);
  return true;
}

bool AP_OpenGLSetBlendFuncSeparate(AP_UInt source_rgb, AP_UInt destination_rgb,
                                   AP_UInt source_alpha,
                                   AP_UInt destination_alpha) {
  if (!AP_OpenGLEnsure()) {
    return false;
  }

  glBlendFuncSeparate((GLenum)source_rgb, (GLenum)destination_rgb,
                      (GLenum)source_alpha, (GLenum)destination_alpha);
  return true;
}

bool AP_OpenGLSetCulling(bool enabled) {
  if (!AP_OpenGLEnsure()) {
    return false;
  }

  if (enabled) {
    glEnable(GL_CULL_FACE);
  } else {
    glDisable(GL_CULL_FACE);
  }

  return true;
}

bool AP_OpenGLSetColorMask(bool red, bool green, bool blue, bool alpha) {
  if (!AP_OpenGLEnsure()) {
    return false;
  }

  glColorMask(red ? GL_TRUE : GL_FALSE, green ? GL_TRUE : GL_FALSE,
              blue ? GL_TRUE : GL_FALSE, alpha ? GL_TRUE : GL_FALSE);
  return true;
}

/* =========================================================
 * Presentation
 * ========================================================= */

bool AP_OpenGLSwapBuffers(void *glfw_window) {
  GLFWwindow *window = (GLFWwindow *)glfw_window;

  if (window == NULL) {
    AP_SET_ERROR(AP_ERROR_INVALID_ARGUMENT, "GLFW window cannot be NULL");
    return false;
  }

  if (glfwGetCurrentContext() != window) {
    glfwMakeContextCurrent(window);
  }

  glfwSwapBuffers(window);
  return true;
}

bool AP_OpenGLFlush(void) {
  if (!AP_OpenGLEnsure()) {
    return false;
  }

  glFlush();
  return true;
}

bool AP_OpenGLFinish(void) {
  if (!AP_OpenGLEnsure()) {
    return false;
  }

  glFinish();
  return true;
}

/* =========================================================
 * Errors
 * ========================================================= */

AP_UInt AP_OpenGLGetError(void) { return (AP_UInt)glGetError(); }

const char *AP_OpenGLErrorName(AP_UInt error) {
  switch (error) {
  case GL_NO_ERROR:
    return "GL_NO_ERROR";
  case GL_INVALID_ENUM:
    return "GL_INVALID_ENUM";
  case GL_INVALID_VALUE:
    return "GL_INVALID_VALUE";
  case GL_INVALID_OPERATION:
    return "GL_INVALID_OPERATION";
  case GL_INVALID_FRAMEBUFFER_OPERATION:
    return "GL_INVALID_FRAMEBUFFER_OPERATION";
  case GL_OUT_OF_MEMORY:
    return "GL_OUT_OF_MEMORY";
  default:
    return "GL_UNKNOWN";
  }
}

bool AP_OpenGLCheckError(const char *operation) {
  GLenum error = glGetError();

  if (error == GL_NO_ERROR) {
    return true;
  }

  if (operation != NULL) {
    AP_ERROR("OpenGL error during %s: %s", operation, AP_OpenGLErrorName(error));
  } else {
    AP_ERROR("OpenGL error: %s", AP_OpenGLErrorName(error));
  }

  return false;
}

void AP_OpenGLClearErrors(void) {
  while (glGetError() != GL_NO_ERROR) {
  }
}

bool AP_OpenGLResetState(void) {
  if (!AP_OpenGLEnsure()) {
    return false;
  }

  glDisable(GL_DEPTH_TEST);
  glDisable(GL_CULL_FACE);
  glDisable(GL_SCISSOR_TEST);
  glEnable(GL_BLEND);
  glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE,
                      GL_ONE_MINUS_SRC_ALPHA);
  glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
  g_depth_test = false;
  g_blending = true;
  g_scissor_enabled = false;
  return true;
}

/* =========================================================
 * Programs
 * ========================================================= */

AP_OpenGLProgramConfig AP_OpenGLDefaultProgramConfig(void) {
  AP_OpenGLProgramConfig config;

  memset(&config, 0, sizeof(config));
  config.transform_feedback_interleaved = true;
  return config;
}

AP_UInt AP_OpenGLCreateProgram(const char *vertex_source,
                               const char *fragment_source) {
  AP_OpenGLProgramConfig config = AP_OpenGLDefaultProgramConfig();

  config.vertex_source = vertex_source;
  config.fragment_source = fragment_source;
  return AP_OpenGLCreateProgramEx(&config);
}

AP_UInt AP_OpenGLCreateProgramEx(const AP_OpenGLProgramConfig *config) {
  const AP_OpenGLCapabilities *capabilities;
  GLuint shaders[6];
  int shader_count = 0;
  GLuint program;
  GLint status = GL_FALSE;
  char log[4096];
  int index;
  bool has_graphics;
  bool has_compute;

  memset(shaders, 0, sizeof(shaders));

  if (!AP_OpenGLEnsure()) {
    return 0;
  }

  if (config == NULL) {
    AP_SET_ERROR(AP_ERROR_INVALID_ARGUMENT, "Program configuration cannot be NULL");
    return 0;
  }

  has_compute = config->compute_source != NULL;
  has_graphics = config->vertex_source != NULL || config->fragment_source != NULL ||
                 config->geometry_source != NULL ||
                 config->tess_control_source != NULL ||
                 config->tess_eval_source != NULL;

  if (has_compute && has_graphics) {
    AP_SET_ERROR(AP_ERROR_INVALID_ARGUMENT,
                 "Compute programs cannot include graphics shader stages");
    return 0;
  }

  if (!has_compute &&
      (config->vertex_source == NULL || config->fragment_source == NULL)) {
    AP_SET_ERROR(AP_ERROR_INVALID_ARGUMENT,
                 "Graphics programs require vertex and fragment sources");
    return 0;
  }

  if ((config->tess_control_source != NULL) !=
      (config->tess_eval_source != NULL)) {
    AP_SET_ERROR(AP_ERROR_INVALID_ARGUMENT,
                 "Tessellation requires both control and evaluation shaders");
    return 0;
  }

  capabilities = AP_OpenGLGetCapabilities();

  if (config->geometry_source != NULL && !capabilities->geometry_shaders) {
    AP_SET_ERROR(AP_ERROR_UNSUPPORTED, "Geometry shaders are not supported");
    return 0;
  }

  if (config->tess_control_source != NULL &&
      !capabilities->tessellation_shaders) {
    AP_SET_ERROR(AP_ERROR_UNSUPPORTED, "Tessellation shaders are not supported");
    return 0;
  }

  if (has_compute && !capabilities->compute_shaders) {
    AP_SET_ERROR(AP_ERROR_UNSUPPORTED, "Compute shaders are not supported");
    return 0;
  }

  program = glCreateProgram();

  if (config->separable) {
    glProgramParameteri(program, GL_PROGRAM_SEPARABLE, GL_TRUE);
  }

  if (config->binary_retrievable) {
    glProgramParameteri(program, GL_PROGRAM_BINARY_RETRIEVABLE_HINT, GL_TRUE);
  }

  if (!AP_OpenGLAttachStage(program, GL_VERTEX_SHADER, config->vertex_source,
                            shaders, &shader_count) ||
      !AP_OpenGLAttachStage(program, GL_FRAGMENT_SHADER, config->fragment_source,
                            shaders, &shader_count) ||
      !AP_OpenGLAttachStage(program, GL_GEOMETRY_SHADER, config->geometry_source,
                            shaders, &shader_count) ||
      !AP_OpenGLAttachStage(program, GL_TESS_CONTROL_SHADER,
                            config->tess_control_source, shaders,
                            &shader_count) ||
      !AP_OpenGLAttachStage(program, GL_TESS_EVALUATION_SHADER,
                            config->tess_eval_source, shaders, &shader_count) ||
      !AP_OpenGLAttachStage(program, GL_COMPUTE_SHADER, config->compute_source,
                            shaders, &shader_count)) {
    AP_OpenGLDeleteShaderList(shaders, shader_count);
    glDeleteProgram(program);
    return 0;
  }

  if (config->attribs != NULL) {
    for (index = 0; index < config->attrib_count; ++index) {
      if (config->attribs[index].name != NULL &&
          config->attribs[index].location >= 0) {
        glBindAttribLocation(program, (GLuint)config->attribs[index].location,
                             config->attribs[index].name);
      }
    }
  }

  if (config->frag_outputs != NULL) {
    for (index = 0; index < config->frag_output_count; ++index) {
      if (config->frag_outputs[index].name != NULL &&
          config->frag_outputs[index].location >= 0) {
        glBindFragDataLocation(program,
                               (GLuint)config->frag_outputs[index].location,
                               config->frag_outputs[index].name);
      }
    }
  }

  if (config->transform_feedback_varyings != NULL &&
      config->transform_feedback_count > 0) {
    glTransformFeedbackVaryings(
        program, config->transform_feedback_count,
        (const GLchar *const *)config->transform_feedback_varyings,
        config->transform_feedback_interleaved ? GL_INTERLEAVED_ATTRIBS
                                               : GL_SEPARATE_ATTRIBS);
  }

  glLinkProgram(program);
  AP_OpenGLDeleteShaderList(shaders, shader_count);

  glGetProgramiv(program, GL_LINK_STATUS, &status);
  if (status != GL_TRUE) {
    glGetProgramInfoLog(program, (GLsizei)sizeof(log), NULL, log);
    AP_ERROR("Program link failed: %s", log);
    AP_SET_ERROR(AP_ERROR_OPERATION_FAILED, "Shader program link failed");
    glDeleteProgram(program);
    return 0;
  }

  return (AP_UInt)program;
}

void AP_OpenGLDeleteProgram(AP_UInt program) {
  if (program != 0 && g_opengl_initialized) {
    glDeleteProgram(program);
  }
}

static bool AP_OpenGLSubsystemInit(void) { return AP_OpenGLInit(NULL); }

const AP_SubsystemMetadata AP_OpenGLSubsystem = {
    .init = AP_OpenGLSubsystemInit,
    .close = AP_OpenGLClose,
};
