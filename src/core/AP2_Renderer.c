/*
 * AP2 — Application Primitives
 * Copyright (c) 2024-2026 Jack Waechter
 *
 * Licensed under the MIT License.
 * See LICENSE in the project root for full terms.
 */

#define AP2_RENDERER_NO_SHORT_NAMES
#include "AP2/AP2_Renderer.h"

#include "AP2_Internal.h"

#include "AP2/AP2_Error.h"
#include "AP2/AP2_Logger.h"
#include "AP2/AP2_Opengl.h"
#include "AP2/AP2_Shader.h"
#include "AP2/AP2_Window.h"

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#include <glad/gl.h>

#include <math.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#define AP_RENDERER_MAX_VERTICES 32768
#define AP_RENDERER_TRANSFORM_STACK 32
#define AP_PI 3.14159265358979323846f
#define AP_DEG2RAD (AP_PI / 180.0f)

typedef struct AP_Vertex2D {
  float x;
  float y;
  float r;
  float g;
  float b;
  float a;
  float u;
  float v;
} AP_Vertex2D;

typedef struct AP_Renderer {
  AP_Window *window;

  bool initialized;

  AP_Color draw_color;
  AP_Color clear_color;
  AP_Color color_mod;
  float alpha_mod;

  AP_BlendMode blend_mode;
  float line_width;
  float point_size;
  AP_LineCap line_cap;
  AP_LineJoin line_join;
  int circle_segments;
  uint32_t draw_flags;

  AP_Transform transform;
  AP_Transform transform_stack[AP_RENDERER_TRANSFORM_STACK];
  int transform_depth;

  AP_Rect viewport;
  AP_Rect clip;
  bool clip_enabled;
  bool viewport_locked;

  int fb_width;
  int fb_height;
  int window_fb_width;
  int window_fb_height;
  GLuint target_fbo;

  AP_Shader *builtin_shader;
  AP_Shader *user_shader;

  GLuint vao;
  GLuint vbo;
  GLuint white_texture;
  GLuint batch_texture;
  AP_BlendMode batch_blend;

  AP_Vertex2D *vertices;
  int vertex_count;
  int vertex_capacity;
} AP_Renderer;

static AP_Renderer *g_renderer = NULL;

/* =========================================================
 * Helpers
 * ========================================================= */

static float AP_Clampf(float value, float minimum, float maximum) {
  if (value < minimum) {
    return minimum;
  }

  if (value > maximum) {
    return maximum;
  }

  return value;
}

static AP_Color AP_MakeColor(AP_F32 red, AP_F32 green, AP_F32 blue,
                             AP_F32 alpha) {
  AP_Color color;
  color.r = AP_Clampf(red, 0.0f, 1.0f);
  color.g = AP_Clampf(green, 0.0f, 1.0f);
  color.b = AP_Clampf(blue, 0.0f, 1.0f);
  color.a = AP_Clampf(alpha, 0.0f, 1.0f);
  return color;
}

static AP_Color AP_RendererVertexColor(const AP_Renderer *renderer,
                                       AP_Color color) {
  color.r = AP_Clampf(color.r * renderer->color_mod.r, 0.0f, 1.0f);
  color.g = AP_Clampf(color.g * renderer->color_mod.g, 0.0f, 1.0f);
  color.b = AP_Clampf(color.b * renderer->color_mod.b, 0.0f, 1.0f);
  color.a = AP_Clampf(color.a * renderer->alpha_mod, 0.0f, 1.0f);
  return color;
}

static AP_Transform AP_TransformIdentity(void) {
  AP_Transform transform;
  memset(&transform, 0, sizeof(transform));
  transform.scale_x = 1.0f;
  transform.scale_y = 1.0f;
  return transform;
}

static void AP_IntersectRect(AP_Rect *result, const AP_Rect *a,
                             const AP_Rect *b) {
  int x1 = a->x > b->x ? a->x : b->x;
  int y1 = a->y > b->y ? a->y : b->y;
  int x2 = (a->x + a->w) < (b->x + b->w) ? (a->x + a->w) : (b->x + b->w);
  int y2 = (a->y + a->h) < (b->y + b->h) ? (a->y + a->h) : (b->y + b->h);

  result->x = x1;
  result->y = y1;
  result->w = x2 > x1 ? x2 - x1 : 0;
  result->h = y2 > y1 ? y2 - y1 : 0;
}

static AP_Renderer *AP_RendererActive(void) {
  if (g_renderer == NULL || !g_renderer->initialized) {
    AP_SET_ERROR(AP_ERROR_NOT_INITIALIZED, "Renderer is not initialized");
    return NULL;
  }

  return g_renderer;
}

static void AP_RendererUseTexture(AP_Renderer *renderer, GLuint texture,
                                  AP_BlendMode blend);

static void AP_RendererApplyBlend(AP_Renderer *renderer) {
  switch (renderer->blend_mode) {
  case AP_BLEND_NONE:
    AP_OpenGLSetBlending(false);
    break;

  case AP_BLEND_ADD:
    AP_OpenGLSetBlending(true);
    AP_OpenGLSetBlendFunc((AP_UInt)GL_SRC_ALPHA, (AP_UInt)GL_ONE);
    break;

  case AP_BLEND_MOD:
    AP_OpenGLSetBlending(true);
    AP_OpenGLSetBlendFunc((AP_UInt)GL_DST_COLOR, (AP_UInt)GL_ZERO);
    break;

  case AP_BLEND_MUL:
    AP_OpenGLSetBlending(true);
    AP_OpenGLSetBlendFuncSeparate(
        (AP_UInt)GL_DST_COLOR, (AP_UInt)GL_ONE_MINUS_SRC_ALPHA,
        (AP_UInt)GL_DST_ALPHA, (AP_UInt)GL_ONE_MINUS_SRC_ALPHA);
    break;

  case AP_BLEND_SCREEN:
    AP_OpenGLSetBlending(true);
    AP_OpenGLSetBlendFunc((AP_UInt)GL_ONE, (AP_UInt)GL_ONE_MINUS_SRC_COLOR);
    break;

  case AP_BLEND_PREMULTIPLIED:
    AP_OpenGLSetBlending(true);
    AP_OpenGLSetBlendFunc((AP_UInt)GL_ONE, (AP_UInt)GL_ONE_MINUS_SRC_ALPHA);
    break;

  case AP_BLEND_ALPHA:
  default:
    AP_OpenGLSetBlending(true);
    AP_OpenGLSetBlendFuncSeparate(
        (AP_UInt)GL_SRC_ALPHA, (AP_UInt)GL_ONE_MINUS_SRC_ALPHA, (AP_UInt)GL_ONE,
        (AP_UInt)GL_ONE_MINUS_SRC_ALPHA);
    break;
  }
}

static void AP_RendererApplyScissor(AP_Renderer *renderer) {
  AP_Rect area = renderer->viewport;

  if (renderer->clip_enabled) {
    AP_IntersectRect(&area, &renderer->viewport, &renderer->clip);
  }

  if (area.w <= 0 || area.h <= 0) {
    AP_OpenGLSetScissorEnabled(true);
    AP_OpenGLSetScissor(0, 0, 0, 0);
    return;
  }

  if (!renderer->clip_enabled && !renderer->viewport_locked) {
    AP_OpenGLSetScissorEnabled(false);
    return;
  }

  AP_OpenGLSetScissorEnabled(true);
  AP_OpenGLSetScissor(area.x, renderer->fb_height - area.y - area.h, area.w,
                      area.h);
}

static void AP_RendererFlushInternal(AP_Renderer *renderer) {
  AP_Shader *shader;
  AP_Int resolution;
  AP_Int sampler;

  if (renderer == NULL || renderer->vertex_count <= 0) {
    return;
  }

  shader = renderer->user_shader != NULL ? renderer->user_shader
                                         : renderer->builtin_shader;
  glUseProgram((GLuint)AP_ShaderNativeProgram(shader));
  resolution = AP_ShaderResolutionUniform(shader);
  if (resolution >= 0) {
    glUniform2f((GLint)resolution, (float)renderer->fb_width,
                (float)renderer->fb_height);
  }
  sampler = AP_ShaderTextureUniform(shader);
  if (sampler >= 0) {
    glUniform1i((GLint)sampler, 0);
  }
  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, renderer->batch_texture != 0
                                   ? renderer->batch_texture
                                   : renderer->white_texture);
  glBindVertexArray(renderer->vao);
  glBindBuffer(GL_ARRAY_BUFFER, renderer->vbo);
  glBufferSubData(
      GL_ARRAY_BUFFER, 0,
      (GLsizeiptr)(renderer->vertex_count * (int)sizeof(AP_Vertex2D)),
      renderer->vertices);
  glDrawArrays(GL_TRIANGLES, 0, renderer->vertex_count);
  glBindVertexArray(0);

  renderer->vertex_count = 0;
}

static bool AP_RendererReserve(AP_Renderer *renderer, int count) {
  if (count > renderer->vertex_capacity) {
    AP_SET_ERROR(AP_ERROR_OPERATION_FAILED,
                 "Primitive exceeds renderer batch size");
    return false;
  }

  if (renderer->vertex_count + count > renderer->vertex_capacity) {
    AP_RendererFlushInternal(renderer);
  }

  return true;
}

static void AP_RendererMapPoint(const AP_Renderer *renderer, float x, float y,
                                float *out_x, float *out_y) {
  const AP_Transform *t = &renderer->transform;
  float lx = x - t->origin_x;
  float ly = y - t->origin_y;
  float angle = t->rotation * AP_DEG2RAD;
  float cosine = cosf(angle);
  float sine = sinf(angle);
  float rx;
  float ry;

  lx *= t->scale_x;
  ly *= t->scale_y;
  rx = lx * cosine - ly * sine;
  ry = lx * sine + ly * cosine;

  *out_x = rx + t->origin_x + t->translate_x + (float)renderer->viewport.x;
  *out_y = ry + t->origin_y + t->translate_y + (float)renderer->viewport.y;
}

static void AP_RendererPushVertexColor(AP_Renderer *renderer, float x, float y,
                                       AP_Color color) {
  AP_Vertex2D *vertex;

  AP_RendererUseTexture(renderer, renderer->white_texture,
                        renderer->blend_mode);

  vertex = &renderer->vertices[renderer->vertex_count++];
  color = AP_RendererVertexColor(renderer, color);
  vertex->x = x;
  vertex->y = y;
  vertex->r = color.r;
  vertex->g = color.g;
  vertex->b = color.b;
  vertex->a = color.a;
  vertex->u = 0.0f;
  vertex->v = 0.0f;
}

static void AP_RendererPushVertex(AP_Renderer *renderer, float x, float y) {
  AP_RendererPushVertexColor(renderer, x, y, renderer->draw_color);
}

static void AP_RendererPushVertexUV(AP_Renderer *renderer, float x, float y,
                                    float u, float v, AP_Color color) {
  AP_Vertex2D *vertex = &renderer->vertices[renderer->vertex_count++];
  color = AP_RendererVertexColor(renderer, color);
  vertex->x = x;
  vertex->y = y;
  vertex->r = color.r;
  vertex->g = color.g;
  vertex->b = color.b;
  vertex->a = color.a;
  vertex->u = u;
  vertex->v = v;
}

static void AP_RendererUseTexture(AP_Renderer *renderer, GLuint texture,
                                  AP_BlendMode blend) {
  if (texture == 0) {
    texture = renderer->white_texture;
  }

  if (renderer->batch_texture == texture && renderer->batch_blend == blend) {
    return;
  }

  AP_RendererFlushInternal(renderer);
  renderer->batch_texture = texture;
  renderer->batch_blend = blend;
  {
    AP_BlendMode saved = renderer->blend_mode;
    renderer->blend_mode = blend;
    AP_RendererApplyBlend(renderer);
    renderer->blend_mode = saved;
  }
}

static void AP_RendererPushTriangle(AP_Renderer *renderer, float x1, float y1,
                                    float x2, float y2, float x3, float y3) {
  AP_RendererPushVertex(renderer, x1, y1);
  AP_RendererPushVertex(renderer, x2, y2);
  AP_RendererPushVertex(renderer, x3, y3);
}

static void AP_RendererPushTriangleColor(AP_Renderer *renderer, float x1,
                                         float y1, AP_Color c1, float x2,
                                         float y2, AP_Color c2, float x3,
                                         float y3, AP_Color c3) {
  AP_RendererPushVertexColor(renderer, x1, y1, c1);
  AP_RendererPushVertexColor(renderer, x2, y2, c2);
  AP_RendererPushVertexColor(renderer, x3, y3, c3);
}

static bool AP_RendererPushQuad(AP_Renderer *renderer, float x1, float y1,
                                float x2, float y2, float x3, float y3,
                                float x4, float y4) {
  if (!AP_RendererReserve(renderer, 6)) {
    return false;
  }

  AP_RendererPushTriangle(renderer, x1, y1, x2, y2, x3, y3);
  AP_RendererPushTriangle(renderer, x1, y1, x3, y3, x4, y4);
  return true;
}

static bool AP_RendererPushQuadColor(AP_Renderer *renderer, float x1, float y1,
                                     AP_Color c1, float x2, float y2,
                                     AP_Color c2, float x3, float y3,
                                     AP_Color c3, float x4, float y4,
                                     AP_Color c4) {
  if (!AP_RendererReserve(renderer, 6)) {
    return false;
  }

  AP_RendererPushTriangleColor(renderer, x1, y1, c1, x2, y2, c2, x3, y3, c3);
  AP_RendererPushTriangleColor(renderer, x1, y1, c1, x3, y3, c3, x4, y4, c4);
  return true;
}

static int AP_RendererEllipseSegments(const AP_Renderer *renderer, float rx,
                                      float ry) {
  int segments;
  float radius;

  if (renderer->circle_segments > 0) {
    segments = renderer->circle_segments;
  } else {
    radius = rx > ry ? rx : ry;
    segments = (int)(radius * 3.0f);
    if (segments < 16) {
      segments = 16;
    }
    if (segments > 128) {
      segments = 128;
    }
  }

  if ((renderer->draw_flags & AP_DRAW_AA) != 0 && segments < 128) {
    segments *= 2;
    if (segments > 128) {
      segments = 128;
    }
  }

  return segments;
}

static void AP_RendererApplyCentered(const AP_Renderer *renderer, float *x,
                                     float *y, float width, float height) {
  if ((renderer->draw_flags & AP_DRAW_CENTERED) != 0) {
    *x -= width * 0.5f;
    *y -= height * 0.5f;
  }
}

static float AP_NormalizeSweep(float start_deg, float end_deg) {
  float sweep = end_deg - start_deg;

  while (sweep < 0.0f) {
    sweep += 360.0f;
  }

  while (sweep > 360.0f) {
    sweep -= 360.0f;
  }

  return sweep;
}

static bool AP_RendererFillEllipseRaw(AP_Renderer *renderer, float cx, float cy,
                                      float rx, float ry, float start_deg,
                                      float sweep_deg) {
  int segments;
  int i;
  float origin_x;
  float origin_y;
  float start_rad;
  float sweep_rad;

  if (rx <= 0.0f || ry <= 0.0f || sweep_deg <= 0.0f) {
    return true;
  }

  segments = AP_RendererEllipseSegments(renderer, rx, ry);
  segments = (int)((float)segments * (sweep_deg / 360.0f) + 0.5f);
  if (segments < 3) {
    segments = 3;
  }

  if (!AP_RendererReserve(renderer, segments * 3)) {
    return false;
  }

  AP_RendererMapPoint(renderer, cx, cy, &origin_x, &origin_y);
  start_rad = start_deg * AP_DEG2RAD;
  sweep_rad = sweep_deg * AP_DEG2RAD;

  for (i = 0; i < segments; ++i) {
    float a0 = start_rad + (float)i / (float)segments * sweep_rad;
    float a1 = start_rad + (float)(i + 1) / (float)segments * sweep_rad;
    float x0;
    float y0;
    float x1;
    float y1;

    AP_RendererMapPoint(renderer, cx + cosf(a0) * rx, cy + sinf(a0) * ry, &x0,
                        &y0);
    AP_RendererMapPoint(renderer, cx + cosf(a1) * rx, cy + sinf(a1) * ry, &x1,
                        &y1);
    AP_RendererPushTriangle(renderer, origin_x, origin_y, x0, y0, x1, y1);
  }

  return true;
}

static bool AP_RendererStrokeEllipseRaw(AP_Renderer *renderer, float cx,
                                        float cy, float rx, float ry,
                                        float start_deg, float sweep_deg) {
  int segments;
  int i;
  float start_rad;
  float sweep_rad;

  if (rx <= 0.0f || ry <= 0.0f || sweep_deg <= 0.0f) {
    return true;
  }

  segments = AP_RendererEllipseSegments(renderer, rx, ry);
  segments = (int)((float)segments * (sweep_deg / 360.0f) + 0.5f);
  if (segments < 3) {
    segments = 3;
  }

  start_rad = start_deg * AP_DEG2RAD;
  sweep_rad = sweep_deg * AP_DEG2RAD;

  for (i = 0; i < segments; ++i) {
    float a0 = start_rad + (float)i / (float)segments * sweep_rad;
    float a1 = start_rad + (float)(i + 1) / (float)segments * sweep_rad;

    if (!AP_RenderLine(cx + cosf(a0) * rx, cy + sinf(a0) * ry,
                       cx + cosf(a1) * rx, cy + sinf(a1) * ry)) {
      return false;
    }
  }

  return true;
}

static bool AP_RendererFillCircleRaw(AP_Renderer *renderer, float cx, float cy,
                                     float radius) {
  return AP_RendererFillEllipseRaw(renderer, cx, cy, radius, radius, 0.0f,
                                   360.0f);
}

static bool AP_RendererEmitLine(AP_Renderer *renderer, float x1, float y1,
                                float x2, float y2) {
  float ax;
  float ay;
  float bx;
  float by;
  float dx;
  float dy;
  float length;
  float nx;
  float ny;
  float ux;
  float uy;
  float half;

  AP_RendererMapPoint(renderer, x1, y1, &ax, &ay);
  AP_RendererMapPoint(renderer, x2, y2, &bx, &by);

  dx = bx - ax;
  dy = by - ay;
  length = sqrtf(dx * dx + dy * dy);

  if (length <= 0.0001f) {
    return AP_RenderPoint(x1, y1);
  }

  ux = dx / length;
  uy = dy / length;
  nx = -uy;
  ny = ux;
  half = renderer->line_width * 0.5f;
  if (half < 0.5f) {
    half = 0.5f;
  }

  if (renderer->line_cap == AP_LINE_CAP_SQUARE) {
    ax -= ux * half;
    ay -= uy * half;
    bx += ux * half;
    by += uy * half;
  }

  nx *= half;
  ny *= half;

  if (!AP_RendererPushQuad(renderer, ax - nx, ay - ny, ax + nx, ay + ny,
                           bx + nx, by + ny, bx - nx, by - ny)) {
    return false;
  }

  if (renderer->line_cap == AP_LINE_CAP_ROUND) {
    if (!AP_RendererFillCircleRaw(renderer, x1, y1, half)) {
      return false;
    }

    if (!AP_RendererFillCircleRaw(renderer, x2, y2, half)) {
      return false;
    }
  }

  return true;
}

static bool AP_RendererEmitJoin(AP_Renderer *renderer, float x, float y) {
  float half;

  if (renderer->line_join != AP_LINE_JOIN_ROUND) {
    return true;
  }

  half = renderer->line_width * 0.5f;
  if (half < 0.5f) {
    half = 0.5f;
  }

  return AP_RendererFillCircleRaw(renderer, x, y, half);
}

static void AP_RendererDestroyResources(AP_Renderer *renderer) {
  if (renderer == NULL) {
    return;
  }

  if (renderer->vbo != 0) {
    glDeleteBuffers(1, &renderer->vbo);
    renderer->vbo = 0;
  }

  if (renderer->vao != 0) {
    glDeleteVertexArrays(1, &renderer->vao);
    renderer->vao = 0;
  }

  if (renderer->white_texture != 0) {
    glDeleteTextures(1, &renderer->white_texture);
    renderer->white_texture = 0;
  }

  renderer->user_shader = NULL;

  if (renderer->builtin_shader != NULL) {
    AP_ShaderDestroyInternal(renderer->builtin_shader);
    renderer->builtin_shader = NULL;
  }

  free(renderer->vertices);
  renderer->vertices = NULL;
}

static bool AP_RendererCreate(AP_Window *window) {
  AP_Renderer *renderer;

  renderer = (AP_Renderer *)calloc(1, sizeof(AP_Renderer));
  if (renderer == NULL) {
    AP_SET_ERROR(AP_ERROR_OUT_OF_MEMORY, "Failed to allocate renderer");
    return false;
  }

  renderer->window = window;
  renderer->draw_color = AP_MakeColor(1.0f, 1.0f, 1.0f, 1.0f);
  renderer->clear_color = AP_MakeColor(0.0f, 0.0f, 0.0f, 1.0f);
  renderer->color_mod = AP_MakeColor(1.0f, 1.0f, 1.0f, 1.0f);
  renderer->alpha_mod = 1.0f;
  renderer->blend_mode = AP_BLEND_ALPHA;
  renderer->line_width = 1.0f;
  renderer->point_size = 1.0f;
  renderer->line_cap = AP_LINE_CAP_BUTT;
  renderer->line_join = AP_LINE_JOIN_MITER;
  renderer->transform = AP_TransformIdentity();
  renderer->vertex_capacity = AP_RENDERER_MAX_VERTICES;

  renderer->vertices = (AP_Vertex2D *)malloc((size_t)renderer->vertex_capacity *
                                             sizeof(AP_Vertex2D));
  if (renderer->vertices == NULL) {
    free(renderer);
    AP_SET_ERROR(AP_ERROR_OUT_OF_MEMORY,
                 "Failed to allocate renderer vertices");
    return false;
  }

  renderer->builtin_shader = AP_CreateShader(AP_ShaderBuiltinVertexSource(),
                                             AP_ShaderBuiltinFragmentSource());
  if (renderer->builtin_shader == NULL) {
    AP_RendererDestroyResources(renderer);
    free(renderer);
    return false;
  }

  renderer->user_shader = NULL;

  glGenVertexArrays(1, &renderer->vao);
  glGenBuffers(1, &renderer->vbo);
  glBindVertexArray(renderer->vao);
  glBindBuffer(GL_ARRAY_BUFFER, renderer->vbo);
  glBufferData(
      GL_ARRAY_BUFFER,
      (GLsizeiptr)(renderer->vertex_capacity * (int)sizeof(AP_Vertex2D)), NULL,
      GL_DYNAMIC_DRAW);
  glEnableVertexAttribArray(0);
  glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(AP_Vertex2D),
                        (const void *)offsetof(AP_Vertex2D, x));
  glEnableVertexAttribArray(1);
  glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, sizeof(AP_Vertex2D),
                        (const void *)offsetof(AP_Vertex2D, r));
  glEnableVertexAttribArray(2);
  glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(AP_Vertex2D),
                        (const void *)offsetof(AP_Vertex2D, u));
  glBindVertexArray(0);

  {
    unsigned char white[4] = {255, 255, 255, 255};
    glGenTextures(1, &renderer->white_texture);
    glBindTexture(GL_TEXTURE_2D, renderer->white_texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                 white);
    glBindTexture(GL_TEXTURE_2D, 0);
  }

  renderer->batch_texture = renderer->white_texture;
  renderer->batch_blend = renderer->blend_mode;

  AP_WindowGetFramebufferPixels(window, &renderer->fb_width,
                                &renderer->fb_height);
  renderer->window_fb_width = renderer->fb_width;
  renderer->window_fb_height = renderer->fb_height;
  renderer->target_fbo = 0;
  renderer->viewport.x = 0;
  renderer->viewport.y = 0;
  renderer->viewport.w = renderer->fb_width;
  renderer->viewport.h = renderer->fb_height;

  AP_OpenGLSetViewport(0, 0, renderer->fb_width, renderer->fb_height);
  AP_OpenGLSetClearColor(renderer->clear_color.r, renderer->clear_color.g,
                         renderer->clear_color.b, renderer->clear_color.a);
  AP_OpenGLResetState();
  AP_RendererApplyBlend(renderer);
  AP_RendererApplyScissor(renderer);

  renderer->initialized = true;
  g_renderer = renderer;
  AP_FontInit();

  AP_INFO("Renderer ready (%dx%d)", renderer->fb_width, renderer->fb_height);
  return true;
}

/* =========================================================
 * Internal window binding
 * ========================================================= */

bool AP_RendererBindWindow(AP_Window *window) {
  if (window == NULL) {
    AP_SET_ERROR(AP_ERROR_INVALID_ARGUMENT, "Renderer window cannot be NULL");
    return false;
  }

  if (g_renderer != NULL) {
    if (g_renderer->window == window) {
      return true;
    }

    AP_RendererUnbindWindow(g_renderer->window);
  }

  return AP_RendererCreate(window);
}

void AP_RendererUnbindWindow(AP_Window *window) {
  if (g_renderer == NULL) {
    return;
  }

  if (window != NULL && g_renderer->window != window) {
    return;
  }

  AP_RendererFlushInternal(g_renderer);
  g_renderer->user_shader = NULL;
  AP_PostShutdown();
  AP_3DShutdown();
  AP_GuiShutdown();
  AP_FontShutdown();
  AP_RendererDestroyResources(g_renderer);
  free(g_renderer);
  g_renderer = NULL;
}

void AP_RendererFlushCurrent(void) {
  if (g_renderer != NULL) {
    AP_RendererFlushInternal(g_renderer);
  }
}

bool AP_RendererSetUserShader(AP_Shader *shader) {
  if (g_renderer == NULL || !g_renderer->initialized) {
    AP_SET_ERROR(AP_ERROR_NOT_INITIALIZED, "Renderer is not initialized");
    return false;
  }

  if (shader == g_renderer->builtin_shader) {
    shader = NULL;
  }

  if (g_renderer->user_shader == shader) {
    return true;
  }

  AP_RendererFlushInternal(g_renderer);
  g_renderer->user_shader = shader;
  return true;
}

AP_Shader *AP_RendererGetUserShader(void) {
  if (g_renderer == NULL) {
    return NULL;
  }

  return g_renderer->user_shader;
}

AP_Shader *AP_RendererGetBuiltinShader(void) {
  if (g_renderer == NULL) {
    return NULL;
  }

  return g_renderer->builtin_shader;
}

bool AP_RendererMakeCurrent(AP_Window *window) {
  if (g_renderer == NULL || window == NULL) {
    return false;
  }

  g_renderer->window = window;

  if (!AP_OpenGLMakeContextCurrent(AP_WindowGetGLFW(window))) {
    return false;
  }

  AP_WindowGetFramebufferPixels(window, &g_renderer->window_fb_width,
                                &g_renderer->window_fb_height);

  if (g_renderer->target_fbo == 0) {
    g_renderer->fb_width = g_renderer->window_fb_width;
    g_renderer->fb_height = g_renderer->window_fb_height;

    if (!g_renderer->viewport_locked) {
      g_renderer->viewport.x = 0;
      g_renderer->viewport.y = 0;
      g_renderer->viewport.w = g_renderer->fb_width;
      g_renderer->viewport.h = g_renderer->fb_height;
    }

    AP_OpenGLSetViewport(0, 0, g_renderer->fb_width, g_renderer->fb_height);
  }

  AP_RendererApplyBlend(g_renderer);
  AP_RendererApplyScissor(g_renderer);
  return true;
}

void AP_RendererNotifyResize(int width, int height) {
  if (g_renderer == NULL) {
    return;
  }

  AP_RendererFlushInternal(g_renderer);

  g_renderer->window_fb_width = width;
  g_renderer->window_fb_height = height;
  AP_PostNotifyResize(width, height);

  if (g_renderer->target_fbo != 0) {
    return;
  }

  g_renderer->fb_width = width;
  g_renderer->fb_height = height;

  if (!g_renderer->viewport_locked) {
    g_renderer->viewport.x = 0;
    g_renderer->viewport.y = 0;
    g_renderer->viewport.w = width;
    g_renderer->viewport.h = height;
  }

  AP_OpenGLSetViewport(0, 0, width, height);
  AP_RendererApplyScissor(g_renderer);
}

/* =========================================================
 * Color
 * ========================================================= */

bool AP_SetRenderDrawColorFloat(AP_F32 red, AP_F32 green, AP_F32 blue,
                                AP_F32 alpha) {
  AP_Renderer *renderer = AP_RendererActive();

  if (renderer == NULL) {
    return false;
  }

  renderer->draw_color = AP_MakeColor(red, green, blue, alpha);
  return true;
}

bool AP_SetRenderDrawColor(AP_U8 red, AP_U8 green, AP_U8 blue, AP_U8 alpha) {
  return AP_SetRenderDrawColorFloat(
      (AP_F32)red / 255.0f, (AP_F32)green / 255.0f, (AP_F32)blue / 255.0f,
      (AP_F32)alpha / 255.0f);
}

bool AP_SetRenderDrawColorC(AP_Color color) {
  return AP_SetRenderDrawColorFloat(color.r, color.g, color.b, color.a);
}

bool AP_GetRenderDrawColorFloat(AP_F32 *red, AP_F32 *green, AP_F32 *blue,
                                AP_F32 *alpha) {
  AP_Renderer *renderer = AP_RendererActive();

  if (renderer == NULL) {
    return false;
  }

  if (red != NULL) {
    *red = renderer->draw_color.r;
  }

  if (green != NULL) {
    *green = renderer->draw_color.g;
  }

  if (blue != NULL) {
    *blue = renderer->draw_color.b;
  }

  if (alpha != NULL) {
    *alpha = renderer->draw_color.a;
  }

  return true;
}

bool AP_SetClearColor(AP_F32 red, AP_F32 green, AP_F32 blue, AP_F32 alpha) {
  AP_Renderer *renderer = AP_RendererActive();

  if (renderer == NULL) {
    return false;
  }

  renderer->clear_color = AP_MakeColor(red, green, blue, alpha);
  return AP_OpenGLSetClearColor(
      renderer->clear_color.r, renderer->clear_color.g, renderer->clear_color.b,
      renderer->clear_color.a);
}

bool AP_SetClearColor8(AP_U8 red, AP_U8 green, AP_U8 blue, AP_U8 alpha) {
  return AP_SetClearColor((AP_F32)red / 255.0f, (AP_F32)green / 255.0f,
                          (AP_F32)blue / 255.0f, (AP_F32)alpha / 255.0f);
}

bool AP_SetClearColorC(AP_Color color) {
  return AP_SetClearColor(color.r, color.g, color.b, color.a);
}

bool AP_GetClearColor(AP_F32 *red, AP_F32 *green, AP_F32 *blue, AP_F32 *alpha) {
  AP_Renderer *renderer = AP_RendererActive();

  if (renderer == NULL) {
    return false;
  }

  if (red != NULL) {
    *red = renderer->clear_color.r;
  }

  if (green != NULL) {
    *green = renderer->clear_color.g;
  }

  if (blue != NULL) {
    *blue = renderer->clear_color.b;
  }

  if (alpha != NULL) {
    *alpha = renderer->clear_color.a;
  }

  return true;
}

bool AP_SetRenderColorScale(AP_F32 red, AP_F32 green, AP_F32 blue) {
  AP_Renderer *renderer = AP_RendererActive();

  if (renderer == NULL) {
    return false;
  }

  renderer->color_mod.r = AP_Clampf(red, 0.0f, 1.0f);
  renderer->color_mod.g = AP_Clampf(green, 0.0f, 1.0f);
  renderer->color_mod.b = AP_Clampf(blue, 0.0f, 1.0f);
  return true;
}

bool AP_GetRenderColorScale(AP_F32 *red, AP_F32 *green, AP_F32 *blue) {
  AP_Renderer *renderer = AP_RendererActive();

  if (renderer == NULL) {
    return false;
  }

  if (red != NULL) {
    *red = renderer->color_mod.r;
  }

  if (green != NULL) {
    *green = renderer->color_mod.g;
  }

  if (blue != NULL) {
    *blue = renderer->color_mod.b;
  }

  return true;
}

bool AP_SetAlphaMod(AP_F32 alpha) {
  AP_Renderer *renderer = AP_RendererActive();

  if (renderer == NULL) {
    return false;
  }

  renderer->alpha_mod = AP_Clampf(alpha, 0.0f, 1.0f);
  return true;
}

AP_F32 AP_GetAlphaMod(void) {
  return g_renderer != NULL ? g_renderer->alpha_mod : 1.0f;
}

/* =========================================================
 * Frame
 * ========================================================= */

bool AP_RenderClear(void) {
  AP_Renderer *renderer = AP_RendererActive();

  if (renderer == NULL) {
    return false;
  }

  AP_RendererFlushInternal(renderer);
  AP_PostBeginFrame();
  AP_OpenGLSetClearColor(renderer->draw_color.r, renderer->draw_color.g,
                         renderer->draw_color.b, renderer->draw_color.a);
  return AP_OpenGLClear(true, true, true);
}

bool AP_FlushRenderer(void) {
  AP_Renderer *renderer = AP_RendererActive();

  if (renderer == NULL) {
    return false;
  }

  AP_RendererFlushInternal(renderer);
  return true;
}

bool AP_RenderFill(AP_F32 r, AP_F32 g, AP_F32 b, AP_F32 a) {
  AP_SetRenderDrawColorFloat(r, g, b, a);
  return AP_RenderClear();
}

bool AP_RenderPresent(void) {
  AP_Renderer *renderer = AP_RendererActive();

  if (renderer == NULL) {
    return false;
  }

  AP_RendererFlushInternal(renderer);
  AP_PostPresent();
  return AP_OpenGLSwapBuffers(AP_WindowGetGLFW(renderer->window));
}

/* =========================================================
 * Draw flags
 * ========================================================= */

bool AP_SetRenderDrawFlags(uint32_t flags) {
  AP_Renderer *renderer = AP_RendererActive();

  if (renderer == NULL) {
    return false;
  }

  renderer->draw_flags = flags;
  return true;
}

uint32_t AP_GetRenderDrawFlags(void) {
  return g_renderer != NULL ? g_renderer->draw_flags : AP_DRAW_NONE;
}

bool AP_EnableRenderDrawFlag(AP_DrawFlags flag) {
  AP_Renderer *renderer = AP_RendererActive();

  if (renderer == NULL) {
    return false;
  }

  renderer->draw_flags |= (uint32_t)flag;
  return true;
}

bool AP_DisableRenderDrawFlag(AP_DrawFlags flag) {
  AP_Renderer *renderer = AP_RendererActive();

  if (renderer == NULL) {
    return false;
  }

  renderer->draw_flags &= ~(uint32_t)flag;
  return true;
}

bool AP_RenderDrawFlagEnabled(AP_DrawFlags flag) {
  return g_renderer != NULL && (g_renderer->draw_flags & (uint32_t)flag) != 0;
}

/* =========================================================
 * Points / lines / rects
 * ========================================================= */

bool AP_SetRenderPointSize(AP_F32 size) {
  AP_Renderer *renderer = AP_RendererActive();

  if (renderer == NULL) {
    return false;
  }

  renderer->point_size = size < 1.0f ? 1.0f : size;
  return true;
}

AP_F32 AP_GetRenderPointSize(void) {
  return g_renderer != NULL ? g_renderer->point_size : 1.0f;
}

bool AP_RenderPoint(AP_F32 x, AP_F32 y) {
  AP_Renderer *renderer = AP_RendererActive();
  float px;
  float py;
  float half;

  if (renderer == NULL) {
    return false;
  }

  half = renderer->point_size * 0.5f;
  if (half < 0.5f) {
    half = 0.5f;
  }

  if ((renderer->draw_flags & AP_DRAW_ROUND_POINTS) != 0) {
    return AP_RendererFillCircleRaw(renderer, x, y, half);
  }

  AP_RendererMapPoint(renderer, x, y, &px, &py);
  return AP_RendererPushQuad(renderer, px - half, py - half, px + half,
                             py - half, px + half, py + half, px - half,
                             py + half);
}

bool AP_DrawPoint(int x, int y) { return AP_RenderPoint((AP_F32)x, (AP_F32)y); }

bool AP_DrawPoints(const AP_Vec2I *points, int count) {
  int i;

  if (points == NULL || count < 0) {
    AP_SET_ERROR(AP_ERROR_INVALID_ARGUMENT, "Invalid point list");
    return false;
  }

  for (i = 0; i < count; ++i) {
    if (!AP_DrawPoint(points[i].x, points[i].y)) {
      return false;
    }
  }

  return true;
}

bool AP_RenderPoints(const AP_Vec2 *points, int count) {
  int i;

  if (points == NULL || count < 0) {
    AP_SET_ERROR(AP_ERROR_INVALID_ARGUMENT, "Invalid point list");
    return false;
  }

  for (i = 0; i < count; ++i) {
    if (!AP_RenderPoint(points[i].x, points[i].y)) {
      return false;
    }
  }

  return true;
}

bool AP_SetRenderLineWidth(AP_F32 width) {
  AP_Renderer *renderer = AP_RendererActive();

  if (renderer == NULL) {
    return false;
  }

  renderer->line_width = width < 1.0f ? 1.0f : width;
  return true;
}

AP_F32 AP_GetRenderLineWidth(void) {
  return g_renderer != NULL ? g_renderer->line_width : 1.0f;
}

bool AP_SetRenderLineCap(AP_LineCap cap) {
  AP_Renderer *renderer = AP_RendererActive();

  if (renderer == NULL) {
    return false;
  }

  renderer->line_cap = cap;
  return true;
}

AP_LineCap AP_GetRenderLineCap(void) {
  return g_renderer != NULL ? g_renderer->line_cap : AP_LINE_CAP_BUTT;
}

bool AP_SetRenderLineJoin(AP_LineJoin join) {
  AP_Renderer *renderer = AP_RendererActive();

  if (renderer == NULL) {
    return false;
  }

  renderer->line_join = join;
  return true;
}

AP_LineJoin AP_GetRenderLineJoin(void) {
  return g_renderer != NULL ? g_renderer->line_join : AP_LINE_JOIN_MITER;
}

bool AP_RenderLine(AP_F32 x1, AP_F32 y1, AP_F32 x2, AP_F32 y2) {
  AP_Renderer *renderer = AP_RendererActive();

  if (renderer == NULL) {
    return false;
  }

  return AP_RendererEmitLine(renderer, x1, y1, x2, y2);
}

bool AP_DrawLine(int x1, int y1, int x2, int y2) {
  return AP_RenderLine((AP_F32)x1, (AP_F32)y1, (AP_F32)x2, (AP_F32)y2);
}

bool AP_RenderLinesClosed(const AP_Vec2 *points, int count, bool closed) {
  AP_Renderer *renderer;
  int i;
  int last;

  if (points == NULL || count < 2) {
    AP_SET_ERROR(AP_ERROR_INVALID_ARGUMENT,
                 "A polyline requires at least two points");
    return false;
  }

  renderer = AP_RendererActive();
  if (renderer == NULL) {
    return false;
  }

  last = closed ? count : count - 1;

  for (i = 0; i < last; ++i) {
    int next = (i + 1) % count;

    if (!AP_RenderLine(points[i].x, points[i].y, points[next].x,
                       points[next].y)) {
      return false;
    }

    if (i + 1 < last || closed) {
      if (!AP_RendererEmitJoin(renderer, points[next].x, points[next].y)) {
        return false;
      }
    }
  }

  return true;
}

bool AP_DrawLines(const AP_Vec2I *points, int count) {
  AP_Vec2 *converted;
  int i;
  bool result;
  bool closed;

  if (points == NULL || count < 2) {
    AP_SET_ERROR(AP_ERROR_INVALID_ARGUMENT,
                 "A line list requires at least two points");
    return false;
  }

  converted = (AP_Vec2 *)malloc((size_t)count * sizeof(AP_Vec2));
  if (converted == NULL) {
    AP_SET_ERROR(AP_ERROR_OUT_OF_MEMORY, "Failed to convert line list");
    return false;
  }

  for (i = 0; i < count; ++i) {
    converted[i].x = (AP_F32)points[i].x;
    converted[i].y = (AP_F32)points[i].y;
  }

  closed = AP_RenderDrawFlagEnabled(AP_DRAW_CLOSED);
  result = AP_RenderLinesClosed(converted, count, closed);
  free(converted);
  return result;
}

bool AP_RenderLines(const AP_Vec2 *points, int count) {
  return AP_RenderLinesClosed(points, count,
                              AP_RenderDrawFlagEnabled(AP_DRAW_CLOSED));
}

bool AP_FillRectF(AP_F32 x, AP_F32 y, AP_F32 width, AP_F32 height) {
  AP_Renderer *renderer = AP_RendererActive();
  float x1;
  float y1;
  float x2;
  float y2;
  float x3;
  float y3;
  float x4;
  float y4;

  if (renderer == NULL) {
    return false;
  }

  if (width <= 0.0f || height <= 0.0f) {
    return true;
  }

  AP_RendererApplyCentered(renderer, &x, &y, width, height);
  AP_RendererMapPoint(renderer, x, y, &x1, &y1);
  AP_RendererMapPoint(renderer, x + width, y, &x2, &y2);
  AP_RendererMapPoint(renderer, x + width, y + height, &x3, &y3);
  AP_RendererMapPoint(renderer, x, y + height, &x4, &y4);
  return AP_RendererPushQuad(renderer, x1, y1, x2, y2, x3, y3, x4, y4);
}

bool AP_FillRect(int x, int y, int width, int height) {
  return AP_FillRectF((AP_F32)x, (AP_F32)y, (AP_F32)width, (AP_F32)height);
}

bool AP_FillRectR(const AP_Rect *rect) {
  if (rect == NULL) {
    AP_SET_ERROR(AP_ERROR_INVALID_ARGUMENT, "Rectangle cannot be NULL");
    return false;
  }

  return AP_FillRect(rect->x, rect->y, rect->w, rect->h);
}

bool AP_FillRectRF(const AP_RectF *rect) {
  if (rect == NULL) {
    AP_SET_ERROR(AP_ERROR_INVALID_ARGUMENT, "Rectangle cannot be NULL");
    return false;
  }

  return AP_FillRectF(rect->x, rect->y, rect->w, rect->h);
}

bool AP_FillRectGradient(AP_F32 x, AP_F32 y, AP_F32 width, AP_F32 height,
                         AP_Color top_left, AP_Color top_right,
                         AP_Color bottom_right, AP_Color bottom_left) {
  AP_Renderer *renderer = AP_RendererActive();
  float x1;
  float y1;
  float x2;
  float y2;
  float x3;
  float y3;
  float x4;
  float y4;

  if (renderer == NULL) {
    return false;
  }

  if (width <= 0.0f || height <= 0.0f) {
    return true;
  }

  AP_RendererApplyCentered(renderer, &x, &y, width, height);
  AP_RendererMapPoint(renderer, x, y, &x1, &y1);
  AP_RendererMapPoint(renderer, x + width, y, &x2, &y2);
  AP_RendererMapPoint(renderer, x + width, y + height, &x3, &y3);
  AP_RendererMapPoint(renderer, x, y + height, &x4, &y4);
  return AP_RendererPushQuadColor(renderer, x1, y1, top_left, x2, y2, top_right,
                                  x3, y3, bottom_right, x4, y4, bottom_left);
}

bool AP_DrawRectF(AP_F32 x, AP_F32 y, AP_F32 width, AP_F32 height) {
  AP_Renderer *renderer = AP_RendererActive();

  if (renderer == NULL) {
    return false;
  }

  if (width <= 0.0f || height <= 0.0f) {
    return true;
  }

  AP_RendererApplyCentered(renderer, &x, &y, width, height);

  if (!AP_RenderLine(x, y, x + width, y)) {
    return false;
  }

  if (!AP_RenderLine(x + width, y, x + width, y + height)) {
    return false;
  }

  if (!AP_RenderLine(x + width, y + height, x, y + height)) {
    return false;
  }

  return AP_RenderLine(x, y + height, x, y);
}

bool AP_DrawRect(int x, int y, int width, int height) {
  return AP_DrawRectF((AP_F32)x, (AP_F32)y, (AP_F32)width, (AP_F32)height);
}

bool AP_DrawRectR(const AP_Rect *rect) {
  if (rect == NULL) {
    AP_SET_ERROR(AP_ERROR_INVALID_ARGUMENT, "Rectangle cannot be NULL");
    return false;
  }

  return AP_DrawRect(rect->x, rect->y, rect->w, rect->h);
}

bool AP_DrawRectRF(const AP_RectF *rect) {
  if (rect == NULL) {
    AP_SET_ERROR(AP_ERROR_INVALID_ARGUMENT, "Rectangle cannot be NULL");
    return false;
  }

  return AP_DrawRectF(rect->x, rect->y, rect->w, rect->h);
}

bool AP_FillRoundedRectF(AP_F32 x, AP_F32 y, AP_F32 width, AP_F32 height,
                         AP_F32 radius) {
  AP_Renderer *renderer = AP_RendererActive();
  float limit;
  uint32_t flags;

  if (renderer == NULL) {
    return false;
  }

  if (width <= 0.0f || height <= 0.0f) {
    return true;
  }

  AP_RendererApplyCentered(renderer, &x, &y, width, height);

  limit = width < height ? width : height;
  limit *= 0.5f;
  if (radius > limit) {
    radius = limit;
  }

  if (radius <= 0.0f) {
    flags = renderer->draw_flags;
    renderer->draw_flags &= ~(uint32_t)AP_DRAW_CENTERED;
    if (!AP_FillRectF(x, y, width, height)) {
      renderer->draw_flags = flags;
      return false;
    }
    renderer->draw_flags = flags;
    return true;
  }

  flags = renderer->draw_flags;
  renderer->draw_flags &= ~(uint32_t)AP_DRAW_CENTERED;

  if (!AP_FillRectF(x + radius, y, width - radius * 2.0f, height) ||
      !AP_FillRectF(x, y + radius, radius, height - radius * 2.0f) ||
      !AP_FillRectF(x + width - radius, y + radius, radius,
                    height - radius * 2.0f) ||
      !AP_RendererFillEllipseRaw(renderer, x + width - radius,
                                 y + height - radius, radius, radius, 0.0f,
                                 90.0f) ||
      !AP_RendererFillEllipseRaw(renderer, x + radius, y + height - radius,
                                 radius, radius, 90.0f, 90.0f) ||
      !AP_RendererFillEllipseRaw(renderer, x + radius, y + radius, radius,
                                 radius, 180.0f, 90.0f) ||
      !AP_RendererFillEllipseRaw(renderer, x + width - radius, y + radius,
                                 radius, radius, 270.0f, 90.0f)) {
    renderer->draw_flags = flags;
    return false;
  }

  renderer->draw_flags = flags;
  return true;
}

bool AP_FillRoundedRect(int x, int y, int width, int height, int radius) {
  return AP_FillRoundedRectF((AP_F32)x, (AP_F32)y, (AP_F32)width,
                             (AP_F32)height, (AP_F32)radius);
}

bool AP_DrawRoundedRectF(AP_F32 x, AP_F32 y, AP_F32 width, AP_F32 height,
                         AP_F32 radius) {
  AP_Renderer *renderer = AP_RendererActive();
  float limit;

  if (renderer == NULL) {
    return false;
  }

  if (width <= 0.0f || height <= 0.0f) {
    return true;
  }

  AP_RendererApplyCentered(renderer, &x, &y, width, height);

  limit = width < height ? width : height;
  limit *= 0.5f;
  if (radius > limit) {
    radius = limit;
  }

  if (radius <= 0.0f) {
    uint32_t flags = renderer->draw_flags;
    renderer->draw_flags &= ~(uint32_t)AP_DRAW_CENTERED;
    if (!AP_DrawRectF(x, y, width, height)) {
      renderer->draw_flags = flags;
      return false;
    }
    renderer->draw_flags = flags;
    return true;
  }

  if (!AP_RenderLine(x + radius, y, x + width - radius, y) ||
      !AP_RenderLine(x + width, y + radius, x + width, y + height - radius) ||
      !AP_RenderLine(x + width - radius, y + height, x + radius, y + height) ||
      !AP_RenderLine(x, y + height - radius, x, y + radius) ||
      !AP_RendererStrokeEllipseRaw(renderer, x + width - radius, y + radius,
                                   radius, radius, 270.0f, 90.0f) ||
      !AP_RendererStrokeEllipseRaw(renderer, x + width - radius,
                                   y + height - radius, radius, radius, 0.0f,
                                   90.0f) ||
      !AP_RendererStrokeEllipseRaw(renderer, x + radius, y + height - radius,
                                   radius, radius, 90.0f, 90.0f) ||
      !AP_RendererStrokeEllipseRaw(renderer, x + radius, y + radius, radius,
                                   radius, 180.0f, 90.0f)) {
    return false;
  }

  return true;
}

bool AP_DrawRoundedRect(int x, int y, int width, int height, int radius) {
  return AP_DrawRoundedRectF((AP_F32)x, (AP_F32)y, (AP_F32)width,
                             (AP_F32)height, (AP_F32)radius);
}

bool AP_SetRenderCircleSegments(int segments) {
  AP_Renderer *renderer = AP_RendererActive();

  if (renderer == NULL) {
    return false;
  }

  if (segments < 0) {
    AP_SET_ERROR(AP_ERROR_INVALID_ARGUMENT,
                 "Circle segments cannot be negative");
    return false;
  }

  renderer->circle_segments = segments;
  return true;
}

int AP_GetRenderCircleSegments(void) {
  return g_renderer != NULL ? g_renderer->circle_segments : 0;
}

bool AP_RenderFillEllipse(AP_F32 cx, AP_F32 cy, AP_F32 radius_x,
                          AP_F32 radius_y) {
  AP_Renderer *renderer = AP_RendererActive();

  if (renderer == NULL) {
    return false;
  }

  return AP_RendererFillEllipseRaw(renderer, cx, cy, radius_x, radius_y, 0.0f,
                                   360.0f);
}

bool AP_FillEllipse(int cx, int cy, int radius_x, int radius_y) {
  return AP_RenderFillEllipse((AP_F32)cx, (AP_F32)cy, (AP_F32)radius_x,
                              (AP_F32)radius_y);
}

bool AP_RenderEllipse(AP_F32 cx, AP_F32 cy, AP_F32 radius_x, AP_F32 radius_y) {
  AP_Renderer *renderer = AP_RendererActive();

  if (renderer == NULL) {
    return false;
  }

  return AP_RendererStrokeEllipseRaw(renderer, cx, cy, radius_x, radius_y, 0.0f,
                                     360.0f);
}

bool AP_DrawEllipse(int cx, int cy, int radius_x, int radius_y) {
  return AP_RenderEllipse((AP_F32)cx, (AP_F32)cy, (AP_F32)radius_x,
                          (AP_F32)radius_y);
}

bool AP_RenderFillCircle(AP_F32 cx, AP_F32 cy, AP_F32 radius) {
  return AP_RenderFillEllipse(cx, cy, radius, radius);
}

bool AP_FillCircle(int cx, int cy, int radius) {
  return AP_RenderFillCircle((AP_F32)cx, (AP_F32)cy, (AP_F32)radius);
}

bool AP_RenderCircle(AP_F32 cx, AP_F32 cy, AP_F32 radius) {
  return AP_RenderEllipse(cx, cy, radius, radius);
}

bool AP_DrawCircle(int cx, int cy, int radius) {
  return AP_RenderCircle((AP_F32)cx, (AP_F32)cy, (AP_F32)radius);
}

bool AP_RenderArc(AP_F32 cx, AP_F32 cy, AP_F32 radius, AP_F32 start_deg,
                  AP_F32 end_deg) {
  AP_Renderer *renderer = AP_RendererActive();

  if (renderer == NULL) {
    return false;
  }

  return AP_RendererStrokeEllipseRaw(renderer, cx, cy, radius, radius,
                                     start_deg,
                                     AP_NormalizeSweep(start_deg, end_deg));
}

bool AP_DrawArc(int cx, int cy, int radius, AP_F32 start_deg, AP_F32 end_deg) {
  return AP_RenderArc((AP_F32)cx, (AP_F32)cy, (AP_F32)radius, start_deg,
                      end_deg);
}

bool AP_RenderFillPie(AP_F32 cx, AP_F32 cy, AP_F32 radius, AP_F32 start_deg,
                      AP_F32 end_deg) {
  AP_Renderer *renderer = AP_RendererActive();

  if (renderer == NULL) {
    return false;
  }

  return AP_RendererFillEllipseRaw(renderer, cx, cy, radius, radius, start_deg,
                                   AP_NormalizeSweep(start_deg, end_deg));
}

bool AP_FillPie(int cx, int cy, int radius, AP_F32 start_deg, AP_F32 end_deg) {
  return AP_RenderFillPie((AP_F32)cx, (AP_F32)cy, (AP_F32)radius, start_deg,
                          end_deg);
}

bool AP_RenderFillRing(AP_F32 cx, AP_F32 cy, AP_F32 inner_radius,
                       AP_F32 outer_radius) {
  AP_Renderer *renderer = AP_RendererActive();
  int segments;
  int i;

  if (renderer == NULL) {
    return false;
  }

  if (outer_radius <= 0.0f) {
    return true;
  }

  if (inner_radius < 0.0f) {
    inner_radius = 0.0f;
  }

  if (inner_radius >= outer_radius) {
    return AP_RenderCircle(cx, cy, outer_radius);
  }

  segments = AP_RendererEllipseSegments(renderer, outer_radius, outer_radius);

  for (i = 0; i < segments; ++i) {
    float a0 = (float)i / (float)segments * 2.0f * AP_PI;
    float a1 = (float)(i + 1) / (float)segments * 2.0f * AP_PI;
    float x0;
    float y0;
    float x1;
    float y1;
    float x2;
    float y2;
    float x3;
    float y3;

    AP_RendererMapPoint(renderer, cx + cosf(a0) * inner_radius,
                        cy + sinf(a0) * inner_radius, &x0, &y0);
    AP_RendererMapPoint(renderer, cx + cosf(a0) * outer_radius,
                        cy + sinf(a0) * outer_radius, &x1, &y1);
    AP_RendererMapPoint(renderer, cx + cosf(a1) * outer_radius,
                        cy + sinf(a1) * outer_radius, &x2, &y2);
    AP_RendererMapPoint(renderer, cx + cosf(a1) * inner_radius,
                        cy + sinf(a1) * inner_radius, &x3, &y3);

    if (!AP_RendererPushQuad(renderer, x0, y0, x1, y1, x2, y2, x3, y3)) {
      return false;
    }
  }

  return true;
}

bool AP_FillRing(int cx, int cy, int inner_radius, int outer_radius) {
  return AP_RenderFillRing((AP_F32)cx, (AP_F32)cy, (AP_F32)inner_radius,
                           (AP_F32)outer_radius);
}

bool AP_DrawRing(int cx, int cy, int inner_radius, int outer_radius) {
  if (!AP_DrawCircle(cx, cy, outer_radius)) {
    return false;
  }

  if (inner_radius > 0) {
    return AP_DrawCircle(cx, cy, inner_radius);
  }

  return true;
}

bool AP_RenderFillTriangle(AP_F32 x1, AP_F32 y1, AP_F32 x2, AP_F32 y2,
                           AP_F32 x3, AP_F32 y3) {
  AP_Renderer *renderer = AP_RendererActive();
  float ax;
  float ay;
  float bx;
  float by;
  float cx;
  float cy;

  if (renderer == NULL) {
    return false;
  }

  if (!AP_RendererReserve(renderer, 3)) {
    return false;
  }

  AP_RendererMapPoint(renderer, x1, y1, &ax, &ay);
  AP_RendererMapPoint(renderer, x2, y2, &bx, &by);
  AP_RendererMapPoint(renderer, x3, y3, &cx, &cy);
  AP_RendererPushTriangle(renderer, ax, ay, bx, by, cx, cy);
  return true;
}

bool AP_FillTriangle(int x1, int y1, int x2, int y2, int x3, int y3) {
  return AP_RenderFillTriangle((AP_F32)x1, (AP_F32)y1, (AP_F32)x2, (AP_F32)y2,
                               (AP_F32)x3, (AP_F32)y3);
}

bool AP_RenderFillTriangleColor(AP_F32 x1, AP_F32 y1, AP_Color c1, AP_F32 x2,
                                AP_F32 y2, AP_Color c2, AP_F32 x3, AP_F32 y3,
                                AP_Color c3) {
  AP_Renderer *renderer = AP_RendererActive();
  float ax;
  float ay;
  float bx;
  float by;
  float cx;
  float cy;

  if (renderer == NULL) {
    return false;
  }

  if (!AP_RendererReserve(renderer, 3)) {
    return false;
  }

  AP_RendererMapPoint(renderer, x1, y1, &ax, &ay);
  AP_RendererMapPoint(renderer, x2, y2, &bx, &by);
  AP_RendererMapPoint(renderer, x3, y3, &cx, &cy);
  AP_RendererPushTriangleColor(renderer, ax, ay, c1, bx, by, c2, cx, cy, c3);
  return true;
}

bool AP_RenderTriangle(AP_F32 x1, AP_F32 y1, AP_F32 x2, AP_F32 y2, AP_F32 x3,
                       AP_F32 y3) {
  if (!AP_RenderLine(x1, y1, x2, y2)) {
    return false;
  }

  if (!AP_RenderLine(x2, y2, x3, y3)) {
    return false;
  }

  return AP_RenderLine(x3, y3, x1, y1);
}

bool AP_DrawTriangle(int x1, int y1, int x2, int y2, int x3, int y3) {
  return AP_RenderTriangle((AP_F32)x1, (AP_F32)y1, (AP_F32)x2, (AP_F32)y2,
                           (AP_F32)x3, (AP_F32)y3);
}

bool AP_RenderFillPolygon(const AP_Vec2 *points, int count) {
  AP_Renderer *renderer;
  int i;
  float origin_x;
  float origin_y;

  if (points == NULL || count < 3) {
    AP_SET_ERROR(AP_ERROR_INVALID_ARGUMENT,
                 "A polygon requires at least three points");
    return false;
  }

  renderer = AP_RendererActive();
  if (renderer == NULL) {
    return false;
  }

  if (!AP_RendererReserve(renderer, (count - 2) * 3)) {
    return false;
  }

  AP_RendererMapPoint(renderer, points[0].x, points[0].y, &origin_x, &origin_y);

  for (i = 1; i < count - 1; ++i) {
    float x1;
    float y1;
    float x2;
    float y2;

    AP_RendererMapPoint(renderer, points[i].x, points[i].y, &x1, &y1);
    AP_RendererMapPoint(renderer, points[i + 1].x, points[i + 1].y, &x2, &y2);
    AP_RendererPushTriangle(renderer, origin_x, origin_y, x1, y1, x2, y2);
  }

  return true;
}

bool AP_RenderPolygon(const AP_Vec2 *points, int count) {
  return AP_RenderLinesClosed(points, count, true);
}

static bool AP_RendererRegularPolygon(int cx, int cy, int radius, int sides,
                                      bool fill) {
  AP_Vec2 *points;
  int i;
  bool result;

  if (sides < 3 || radius <= 0) {
    AP_SET_ERROR(AP_ERROR_INVALID_ARGUMENT,
                 "N-gon requires radius > 0 and sides >= 3");
    return false;
  }

  points = (AP_Vec2 *)malloc((size_t)sides * sizeof(AP_Vec2));
  if (points == NULL) {
    AP_SET_ERROR(AP_ERROR_OUT_OF_MEMORY, "Failed to allocate n-gon");
    return false;
  }

  for (i = 0; i < sides; ++i) {
    float angle = (float)i / (float)sides * 2.0f * AP_PI - AP_PI * 0.5f;
    points[i].x = (AP_F32)cx + cosf(angle) * (AP_F32)radius;
    points[i].y = (AP_F32)cy + sinf(angle) * (AP_F32)radius;
  }

  result = fill ? AP_RenderFillPolygon(points, sides)
                : AP_RenderPolygon(points, sides);
  free(points);
  return result;
}

bool AP_DrawNGon(int cx, int cy, int radius, int sides) {
  return AP_RendererRegularPolygon(cx, cy, radius, sides, false);
}

bool AP_FillNGon(int cx, int cy, int radius, int sides) {
  return AP_RendererRegularPolygon(cx, cy, radius, sides, true);
}

static bool AP_RendererStar(int cx, int cy, int outer_radius, int inner_radius,
                            int points, bool fill) {
  AP_Vec2 *vertices;
  int count;
  int i;
  bool result;

  if (points < 2 || outer_radius <= 0 || inner_radius <= 0) {
    AP_SET_ERROR(AP_ERROR_INVALID_ARGUMENT, "Star requires at least 2 points");
    return false;
  }

  count = points * 2;
  vertices = (AP_Vec2 *)malloc((size_t)count * sizeof(AP_Vec2));
  if (vertices == NULL) {
    AP_SET_ERROR(AP_ERROR_OUT_OF_MEMORY, "Failed to allocate star");
    return false;
  }

  for (i = 0; i < count; ++i) {
    float angle = (float)i / (float)count * 2.0f * AP_PI - AP_PI * 0.5f;
    float radius = (i % 2 == 0) ? (float)outer_radius : (float)inner_radius;
    vertices[i].x = (AP_F32)cx + cosf(angle) * radius;
    vertices[i].y = (AP_F32)cy + sinf(angle) * radius;
  }

  result = fill ? AP_RenderFillPolygon(vertices, count)
                : AP_RenderPolygon(vertices, count);
  free(vertices);
  return result;
}

bool AP_DrawStar(int cx, int cy, int outer_radius, int inner_radius,
                 int points) {
  return AP_RendererStar(cx, cy, outer_radius, inner_radius, points, false);
}

bool AP_FillStar(int cx, int cy, int outer_radius, int inner_radius,
                 int points) {
  return AP_RendererStar(cx, cy, outer_radius, inner_radius, points, true);
}

bool AP_RenderQuadraticBezier(AP_F32 x1, AP_F32 y1, AP_F32 cx, AP_F32 cy,
                              AP_F32 x2, AP_F32 y2) {
  AP_Vec2 points[24];
  int i;
  int count = 24;

  for (i = 0; i < count; ++i) {
    float t = (float)i / (float)(count - 1);
    float inv = 1.0f - t;
    points[i].x = inv * inv * x1 + 2.0f * inv * t * cx + t * t * x2;
    points[i].y = inv * inv * y1 + 2.0f * inv * t * cy + t * t * y2;
  }

  return AP_RenderLinesClosed(points, count, false);
}

bool AP_RenderBezier(AP_F32 x1, AP_F32 y1, AP_F32 cx1, AP_F32 cy1, AP_F32 cx2,
                     AP_F32 cy2, AP_F32 x2, AP_F32 y2) {
  AP_Vec2 points[32];
  int i;
  int count = 32;

  for (i = 0; i < count; ++i) {
    float t = (float)i / (float)(count - 1);
    float inv = 1.0f - t;
    float inv2 = inv * inv;
    float t2 = t * t;
    points[i].x = inv2 * inv * x1 + 3.0f * inv2 * t * cx1 +
                  3.0f * inv * t2 * cx2 + t2 * t * x2;
    points[i].y = inv2 * inv * y1 + 3.0f * inv2 * t * cy1 +
                  3.0f * inv * t2 * cy2 + t2 * t * y2;
  }

  return AP_RenderLinesClosed(points, count, false);
}

bool AP_RenderFillCapsule(AP_F32 x1, AP_F32 y1, AP_F32 x2, AP_F32 y2,
                          AP_F32 radius) {
  AP_Renderer *renderer = AP_RendererActive();
  AP_LineCap previous_cap;
  float previous_width;

  if (renderer == NULL) {
    return false;
  }

  if (radius <= 0.0f) {
    return true;
  }

  previous_cap = renderer->line_cap;
  previous_width = renderer->line_width;
  renderer->line_cap = AP_LINE_CAP_ROUND;
  renderer->line_width = radius * 2.0f;

  if (!AP_RenderLine(x1, y1, x2, y2)) {
    renderer->line_cap = previous_cap;
    renderer->line_width = previous_width;
    return false;
  }

  renderer->line_cap = previous_cap;
  renderer->line_width = previous_width;
  return true;
}

bool AP_RenderCapsule(AP_F32 x1, AP_F32 y1, AP_F32 x2, AP_F32 y2,
                      AP_F32 radius) {
  float dx = x2 - x1;
  float dy = y2 - y1;
  float length = sqrtf(dx * dx + dy * dy);
  float nx;
  float ny;
  float angle;

  if (radius <= 0.0f) {
    return true;
  }

  if (length <= 0.0001f) {
    return AP_RenderCircle(x1, y1, radius);
  }

  nx = -dy / length;
  ny = dx / length;
  angle = atan2f(dy, dx) / AP_DEG2RAD;

  if (!AP_RenderLine(x1 + nx * radius, y1 + ny * radius, x2 + nx * radius,
                     y2 + ny * radius)) {
    return false;
  }

  if (!AP_RenderLine(x1 - nx * radius, y1 - ny * radius, x2 - nx * radius,
                     y2 - ny * radius)) {
    return false;
  }

  if (!AP_RenderArc(x1, y1, radius, angle + 90.0f, angle + 270.0f)) {
    return false;
  }

  return AP_RenderArc(x2, y2, radius, angle - 90.0f, angle + 90.0f);
}

bool AP_DrawCross(int cx, int cy, int size) {
  if (size <= 0) {
    return true;
  }

  if (!AP_DrawLine(cx - size, cy, cx + size, cy)) {
    return false;
  }

  return AP_DrawLine(cx, cy - size, cx, cy + size);
}

bool AP_DrawGrid(int x, int y, int width, int height, int columns, int rows) {
  AP_Renderer *renderer = AP_RendererActive();
  int i;
  float fx;
  float fy;
  float fw;
  float fh;

  if (renderer == NULL) {
    return false;
  }

  if (width <= 0 || height <= 0 || columns <= 0 || rows <= 0) {
    AP_SET_ERROR(AP_ERROR_INVALID_ARGUMENT, "Invalid grid dimensions");
    return false;
  }

  fx = (float)x;
  fy = (float)y;
  fw = (float)width;
  fh = (float)height;
  AP_RendererApplyCentered(renderer, &fx, &fy, fw, fh);

  for (i = 0; i <= columns; ++i) {
    float gx = fx + fw * (float)i / (float)columns;
    if (!AP_RenderLine(gx, fy, gx, fy + fh)) {
      return false;
    }
  }

  for (i = 0; i <= rows; ++i) {
    float gy = fy + fh * (float)i / (float)rows;
    if (!AP_RenderLine(fx, gy, fx + fw, gy)) {
      return false;
    }
  }

  return true;
}

/* =========================================================
 * Mesh
 * ========================================================= */

static bool AP_RendererPushMappedVertex(AP_Renderer *renderer,
                                        const AP_Vertex *vertex) {
  float x;
  float y;
  AP_Color color;

  AP_RendererMapPoint(renderer, vertex->position.x, vertex->position.y, &x, &y);
  color = vertex->color;
  AP_RendererPushVertexUV(renderer, x, y, vertex->tex_coord.x,
                          vertex->tex_coord.y, color);
  return true;
}

static bool AP_RendererEmitIndexedTriangle(AP_Renderer *renderer,
                                           const AP_Vertex *vertices,
                                           int vertex_count, int i0, int i1,
                                           int i2) {
  if (i0 < 0 || i1 < 0 || i2 < 0 || i0 >= vertex_count || i1 >= vertex_count ||
      i2 >= vertex_count) {
    AP_SET_ERROR(AP_ERROR_INVALID_ARGUMENT, "Mesh index out of range");
    return false;
  }

  if (!AP_RendererReserve(renderer, 3)) {
    return false;
  }

  AP_RendererPushMappedVertex(renderer, &vertices[i0]);
  AP_RendererPushMappedVertex(renderer, &vertices[i1]);
  AP_RendererPushMappedVertex(renderer, &vertices[i2]);
  return true;
}

static bool AP_RendererEmitPrimitive(AP_Renderer *renderer,
                                     const AP_Vertex *vertices, int count,
                                     const AP_U16 *indices, int index_count,
                                     AP_Primitive primitive) {
  int i;
  int total = indices != NULL ? index_count : count;

  switch (primitive) {
  case AP_PRIM_POINTS:
    for (i = 0; i < total; ++i) {
      int index = indices != NULL ? (int)indices[i] : i;
      if (index < 0 || index >= count) {
        AP_SET_ERROR(AP_ERROR_INVALID_ARGUMENT, "Mesh index out of range");
        return false;
      }
      if (!AP_RenderPoint(vertices[index].position.x,
                          vertices[index].position.y)) {
        return false;
      }
    }
    return true;

  case AP_PRIM_LINES:
    if (total < 2) {
      return true;
    }
    for (i = 0; i + 1 < total; i += 2) {
      int a = indices != NULL ? (int)indices[i] : i;
      int b = indices != NULL ? (int)indices[i + 1] : i + 1;
      if (a < 0 || b < 0 || a >= count || b >= count) {
        AP_SET_ERROR(AP_ERROR_INVALID_ARGUMENT, "Mesh index out of range");
        return false;
      }
      if (!AP_RenderLine(vertices[a].position.x, vertices[a].position.y,
                         vertices[b].position.x, vertices[b].position.y)) {
        return false;
      }
    }
    return true;

  case AP_PRIM_LINE_STRIP:
  case AP_PRIM_LINE_LOOP:
    if (total < 2) {
      return true;
    }
    for (i = 0; i < total - 1; ++i) {
      int a = indices != NULL ? (int)indices[i] : i;
      int b = indices != NULL ? (int)indices[i + 1] : i + 1;
      if (a < 0 || b < 0 || a >= count || b >= count) {
        AP_SET_ERROR(AP_ERROR_INVALID_ARGUMENT, "Mesh index out of range");
        return false;
      }
      if (!AP_RenderLine(vertices[a].position.x, vertices[a].position.y,
                         vertices[b].position.x, vertices[b].position.y)) {
        return false;
      }
    }
    if (primitive == AP_PRIM_LINE_LOOP) {
      int a = indices != NULL ? (int)indices[total - 1] : total - 1;
      int b = indices != NULL ? (int)indices[0] : 0;
      return AP_RenderLine(vertices[a].position.x, vertices[a].position.y,
                           vertices[b].position.x, vertices[b].position.y);
    }
    return true;

  case AP_PRIM_TRIANGLES:
    for (i = 0; i + 2 < total; i += 3) {
      int a = indices != NULL ? (int)indices[i] : i;
      int b = indices != NULL ? (int)indices[i + 1] : i + 1;
      int c = indices != NULL ? (int)indices[i + 2] : i + 2;
      if (!AP_RendererEmitIndexedTriangle(renderer, vertices, count, a, b, c)) {
        return false;
      }
    }
    return true;

  case AP_PRIM_TRIANGLE_STRIP:
    for (i = 0; i + 2 < total; ++i) {
      int a = indices != NULL ? (int)indices[i] : i;
      int b = indices != NULL ? (int)indices[i + 1] : i + 1;
      int c = indices != NULL ? (int)indices[i + 2] : i + 2;
      if ((i & 1) != 0) {
        int tmp = a;
        a = b;
        b = tmp;
      }
      if (!AP_RendererEmitIndexedTriangle(renderer, vertices, count, a, b, c)) {
        return false;
      }
    }
    return true;

  case AP_PRIM_TRIANGLE_FAN:
    if (total < 3) {
      return true;
    }
    {
      int origin = indices != NULL ? (int)indices[0] : 0;
      for (i = 1; i + 1 < total; ++i) {
        int b = indices != NULL ? (int)indices[i] : i;
        int c = indices != NULL ? (int)indices[i + 1] : i + 1;
        if (!AP_RendererEmitIndexedTriangle(renderer, vertices, count, origin,
                                            b, c)) {
          return false;
        }
      }
    }
    return true;

  default:
    AP_SET_ERROR(AP_ERROR_INVALID_ARGUMENT, "Unknown mesh primitive");
    return false;
  }
}

bool AP_RenderPrimitives(const AP_Vertex *vertices, int count,
                         AP_Primitive primitive) {
  AP_Renderer *renderer;

  if (vertices == NULL || count < 0) {
    AP_SET_ERROR(AP_ERROR_INVALID_ARGUMENT, "Invalid mesh");
    return false;
  }

  renderer = AP_RendererActive();
  if (renderer == NULL) {
    return false;
  }

  AP_RendererUseTexture(renderer, renderer->white_texture,
                        renderer->blend_mode);
  return AP_RendererEmitPrimitive(renderer, vertices, count, NULL, 0,
                                  primitive);
}

bool AP_DrawMeshIndexed(const AP_Vertex *vertices, int vertex_count,
                        const AP_U16 *indices, int index_count,
                        AP_Primitive primitive) {
  AP_Renderer *renderer;

  if (vertices == NULL || indices == NULL || vertex_count < 0 ||
      index_count < 0) {
    AP_SET_ERROR(AP_ERROR_INVALID_ARGUMENT, "Invalid indexed mesh");
    return false;
  }

  renderer = AP_RendererActive();
  if (renderer == NULL) {
    return false;
  }

  AP_RendererUseTexture(renderer, renderer->white_texture,
                        renderer->blend_mode);
  return AP_RendererEmitPrimitive(renderer, vertices, vertex_count, indices,
                                  index_count, primitive);
}

bool AP_RenderGeometryRaw(const AP_Vec2 *positions, const AP_Color *colors,
                          int count) {
  AP_Renderer *renderer;
  int i;

  if (positions == NULL || count < 0 || (count % 3) != 0) {
    AP_SET_ERROR(AP_ERROR_INVALID_ARGUMENT,
                 "Geometry requires a triangle vertex count");
    return false;
  }

  renderer = AP_RendererActive();
  if (renderer == NULL) {
    return false;
  }

  if (!AP_RendererReserve(renderer, count)) {
    return false;
  }

  for (i = 0; i < count; ++i) {
    float x;
    float y;
    AP_Color color = renderer->draw_color;

    if (colors != NULL) {
      color = colors[i];
    }

    AP_RendererMapPoint(renderer, positions[i].x, positions[i].y, &x, &y);
    AP_RendererPushVertexColor(renderer, x, y, color);
  }

  return true;
}

/* =========================================================
 * Transform
 * ========================================================= */

bool AP_SetRenderRotation(AP_F32 degrees) {
  AP_Renderer *renderer = AP_RendererActive();

  if (renderer == NULL) {
    return false;
  }

  renderer->transform.rotation = degrees;
  return true;
}

AP_F32 AP_GetRenderRotation(void) {
  return g_renderer != NULL ? g_renderer->transform.rotation : 0.0f;
}

bool AP_SetRenderRotationOrigin(AP_F32 x, AP_F32 y) {
  AP_Renderer *renderer = AP_RendererActive();

  if (renderer == NULL) {
    return false;
  }

  renderer->transform.origin_x = x;
  renderer->transform.origin_y = y;
  return true;
}

bool AP_GetRenderRotationOrigin(AP_F32 *x, AP_F32 *y) {
  AP_Renderer *renderer = AP_RendererActive();

  if (renderer == NULL) {
    return false;
  }

  if (x != NULL) {
    *x = renderer->transform.origin_x;
  }

  if (y != NULL) {
    *y = renderer->transform.origin_y;
  }

  return true;
}

bool AP_SetRenderTranslation(AP_F32 x, AP_F32 y) {
  AP_Renderer *renderer = AP_RendererActive();

  if (renderer == NULL) {
    return false;
  }

  renderer->transform.translate_x = x;
  renderer->transform.translate_y = y;
  return true;
}

bool AP_RenderTranslate(AP_F32 x, AP_F32 y) {
  AP_Renderer *renderer = AP_RendererActive();

  if (renderer == NULL) {
    return false;
  }

  renderer->transform.translate_x += x;
  renderer->transform.translate_y += y;
  return true;
}

bool AP_GetRenderTranslation(AP_F32 *x, AP_F32 *y) {
  AP_Renderer *renderer = AP_RendererActive();

  if (renderer == NULL) {
    return false;
  }

  if (x != NULL) {
    *x = renderer->transform.translate_x;
  }

  if (y != NULL) {
    *y = renderer->transform.translate_y;
  }

  return true;
}

bool AP_SetRenderTransform(const AP_Transform *transform) {
  AP_Renderer *renderer = AP_RendererActive();

  if (renderer == NULL) {
    return false;
  }

  if (transform == NULL) {
    AP_SET_ERROR(AP_ERROR_INVALID_ARGUMENT, "Transform cannot be NULL");
    return false;
  }

  renderer->transform = *transform;
  return true;
}

bool AP_GetRenderTransform(AP_Transform *transform) {
  AP_Renderer *renderer = AP_RendererActive();

  if (renderer == NULL) {
    return false;
  }

  if (transform == NULL) {
    AP_SET_ERROR(AP_ERROR_INVALID_ARGUMENT, "Transform cannot be NULL");
    return false;
  }

  *transform = renderer->transform;
  return true;
}

bool AP_ResetRenderTransform(void) {
  AP_Renderer *renderer = AP_RendererActive();

  if (renderer == NULL) {
    return false;
  }

  renderer->transform = AP_TransformIdentity();
  return true;
}

bool AP_PushRenderTransform(void) {
  AP_Renderer *renderer = AP_RendererActive();

  if (renderer == NULL) {
    return false;
  }

  if (renderer->transform_depth >= AP_RENDERER_TRANSFORM_STACK) {
    AP_SET_ERROR(AP_ERROR_OPERATION_FAILED, "Transform stack overflow");
    return false;
  }

  renderer->transform_stack[renderer->transform_depth++] = renderer->transform;
  return true;
}

bool AP_PopRenderTransform(void) {
  AP_Renderer *renderer = AP_RendererActive();

  if (renderer == NULL) {
    return false;
  }

  if (renderer->transform_depth <= 0) {
    AP_SET_ERROR(AP_ERROR_OPERATION_FAILED, "Transform stack underflow");
    return false;
  }

  renderer->transform = renderer->transform_stack[--renderer->transform_depth];
  return true;
}

/* =========================================================
 * Viewport / clip / blend / scale
 * ========================================================= */

bool AP_SetViewport(int x, int y, int width, int height) {
  AP_Renderer *renderer = AP_RendererActive();

  if (renderer == NULL) {
    return false;
  }

  if (width < 0 || height < 0) {
    AP_SET_ERROR(AP_ERROR_INVALID_ARGUMENT,
                 "Viewport dimensions cannot be negative");
    return false;
  }

  AP_RendererFlushInternal(renderer);
  renderer->viewport.x = x;
  renderer->viewport.y = y;
  renderer->viewport.w = width;
  renderer->viewport.h = height;
  renderer->viewport_locked = true;
  AP_RendererApplyScissor(renderer);
  return true;
}

bool AP_ResetViewport(void) {
  AP_Renderer *renderer = AP_RendererActive();

  if (renderer == NULL) {
    return false;
  }

  AP_RendererFlushInternal(renderer);
  renderer->viewport.x = 0;
  renderer->viewport.y = 0;
  renderer->viewport.w = renderer->fb_width;
  renderer->viewport.h = renderer->fb_height;
  renderer->viewport_locked = false;
  AP_RendererApplyScissor(renderer);
  return true;
}

bool AP_GetViewport(int *x, int *y, int *width, int *height) {
  AP_Renderer *renderer = AP_RendererActive();

  if (renderer == NULL) {
    return false;
  }

  if (x != NULL) {
    *x = renderer->viewport.x;
  }

  if (y != NULL) {
    *y = renderer->viewport.y;
  }

  if (width != NULL) {
    *width = renderer->viewport.w;
  }

  if (height != NULL) {
    *height = renderer->viewport.h;
  }

  return true;
}

bool AP_SetClipRect(int x, int y, int width, int height) {
  AP_Renderer *renderer = AP_RendererActive();

  if (renderer == NULL) {
    return false;
  }

  if (width < 0 || height < 0) {
    AP_SET_ERROR(AP_ERROR_INVALID_ARGUMENT,
                 "Clip dimensions cannot be negative");
    return false;
  }

  AP_RendererFlushInternal(renderer);
  renderer->clip.x = x;
  renderer->clip.y = y;
  renderer->clip.w = width;
  renderer->clip.h = height;
  renderer->clip_enabled = true;
  AP_RendererApplyScissor(renderer);
  return true;
}

bool AP_SetClipRectR(const AP_Rect *rect) {
  if (rect == NULL) {
    AP_SET_ERROR(AP_ERROR_INVALID_ARGUMENT, "Clip rectangle cannot be NULL");
    return false;
  }

  return AP_SetClipRect(rect->x, rect->y, rect->w, rect->h);
}

bool AP_DisableClipRect(void) {
  AP_Renderer *renderer = AP_RendererActive();

  if (renderer == NULL) {
    return false;
  }

  AP_RendererFlushInternal(renderer);
  renderer->clip_enabled = false;
  AP_RendererApplyScissor(renderer);
  return true;
}

bool AP_GetClipRect(int *x, int *y, int *width, int *height) {
  AP_Renderer *renderer = AP_RendererActive();

  if (renderer == NULL) {
    return false;
  }

  if (x != NULL) {
    *x = renderer->clip.x;
  }

  if (y != NULL) {
    *y = renderer->clip.y;
  }

  if (width != NULL) {
    *width = renderer->clip.w;
  }

  if (height != NULL) {
    *height = renderer->clip.h;
  }

  return true;
}

bool AP_RenderClipEnabled(void) {
  return g_renderer != NULL && g_renderer->clip_enabled;
}

bool AP_SetRenderDrawBlendMode(AP_BlendMode mode) {
  AP_Renderer *renderer = AP_RendererActive();

  if (renderer == NULL) {
    return false;
  }

  AP_RendererFlushInternal(renderer);
  renderer->blend_mode = mode;
  renderer->batch_blend = mode;
  AP_RendererApplyBlend(renderer);
  return true;
}

AP_BlendMode AP_GetRenderDrawBlendMode(void) {
  return g_renderer != NULL ? g_renderer->blend_mode : AP_BLEND_ALPHA;
}

bool AP_SetRenderScale(AP_F32 scale_x, AP_F32 scale_y) {
  AP_Renderer *renderer = AP_RendererActive();

  if (renderer == NULL) {
    return false;
  }

  renderer->transform.scale_x = scale_x;
  renderer->transform.scale_y = scale_y;
  return true;
}

bool AP_GetRenderScale(AP_F32 *scale_x, AP_F32 *scale_y) {
  AP_Renderer *renderer = AP_RendererActive();

  if (renderer == NULL) {
    return false;
  }

  if (scale_x != NULL) {
    *scale_x = renderer->transform.scale_x;
  }

  if (scale_y != NULL) {
    *scale_y = renderer->transform.scale_y;
  }

  return true;
}

bool AP_GetRenderDrawColor(AP_U8 *r, AP_U8 *g, AP_U8 *b, AP_U8 *a) {
  float red;
  float green;
  float blue;
  float alpha;

  if (!AP_GetRenderDrawColorFloat(&red, &green, &blue, &alpha)) {
    return false;
  }

  if (r != NULL) {
    *r = (AP_U8)(red * 255.0f + 0.5f);
  }

  if (g != NULL) {
    *g = (AP_U8)(green * 255.0f + 0.5f);
  }

  if (b != NULL) {
    *b = (AP_U8)(blue * 255.0f + 0.5f);
  }

  if (a != NULL) {
    *a = (AP_U8)(alpha * 255.0f + 0.5f);
  }

  return true;
}

static AP_FRect AP_RendererFullFRect(const AP_Renderer *renderer) {
  AP_FRect rect;
  rect.x = 0.0f;
  rect.y = 0.0f;
  rect.w = (float)renderer->viewport.w;
  rect.h = (float)renderer->viewport.h;
  return rect;
}

bool AP_RenderFillRect(const AP_FRect *rect) {
  AP_Renderer *renderer = AP_RendererActive();
  AP_FRect area;

  if (renderer == NULL) {
    return false;
  }

  if (rect == NULL) {
    area = AP_RendererFullFRect(renderer);
    rect = &area;
  }

  return AP_FillRectF(rect->x, rect->y, rect->w, rect->h);
}

bool AP_RenderRect(const AP_FRect *rect) {
  AP_Renderer *renderer = AP_RendererActive();
  AP_FRect area;

  if (renderer == NULL) {
    return false;
  }

  if (rect == NULL) {
    area = AP_RendererFullFRect(renderer);
    rect = &area;
  }

  return AP_DrawRectF(rect->x, rect->y, rect->w, rect->h);
}

bool AP_RenderFillRects(const AP_FRect *rects, int count) {
  int i;

  if (rects == NULL || count < 0) {
    AP_SET_ERROR(AP_ERROR_INVALID_ARGUMENT, "Invalid fill rect list");
    return false;
  }

  for (i = 0; i < count; ++i) {
    if (!AP_RenderFillRect(&rects[i])) {
      return false;
    }
  }

  return true;
}

bool AP_RenderRects(const AP_FRect *rects, int count) {
  int i;

  if (rects == NULL || count < 0) {
    AP_SET_ERROR(AP_ERROR_INVALID_ARGUMENT, "Invalid rect list");
    return false;
  }

  for (i = 0; i < count; ++i) {
    if (!AP_RenderRect(&rects[i])) {
      return false;
    }
  }

  return true;
}

bool AP_RenderFillRectGradient(const AP_FRect *rect, AP_FColor top_left,
                               AP_FColor top_right, AP_FColor bottom_right,
                               AP_FColor bottom_left) {
  if (rect == NULL) {
    AP_SET_ERROR(AP_ERROR_INVALID_ARGUMENT, "Rectangle cannot be NULL");
    return false;
  }

  return AP_FillRectGradient(rect->x, rect->y, rect->w, rect->h, top_left,
                             top_right, bottom_right, bottom_left);
}

bool AP_RenderFillRoundedRect(const AP_FRect *rect, float radius) {
  if (rect == NULL) {
    AP_SET_ERROR(AP_ERROR_INVALID_ARGUMENT, "Rectangle cannot be NULL");
    return false;
  }

  return AP_FillRoundedRectF(rect->x, rect->y, rect->w, rect->h, radius);
}

bool AP_RenderRoundedRect(const AP_FRect *rect, float radius) {
  if (rect == NULL) {
    AP_SET_ERROR(AP_ERROR_INVALID_ARGUMENT, "Rectangle cannot be NULL");
    return false;
  }

  return AP_DrawRoundedRectF(rect->x, rect->y, rect->w, rect->h, radius);
}

bool AP_RenderRing(float x, float y, float inner_radius, float outer_radius) {
  if (!AP_RenderCircle(x, y, outer_radius)) {
    return false;
  }

  if (inner_radius > 0.0f) {
    return AP_RenderCircle(x, y, inner_radius);
  }

  return true;
}

bool AP_RenderNGon(float x, float y, float radius, int sides) {
  return AP_DrawNGon((int)(x + 0.5f), (int)(y + 0.5f), (int)(radius + 0.5f),
                     sides);
}

bool AP_RenderFillNGon(float x, float y, float radius, int sides) {
  return AP_FillNGon((int)(x + 0.5f), (int)(y + 0.5f), (int)(radius + 0.5f),
                     sides);
}

bool AP_RenderStar(float x, float y, float outer_radius, float inner_radius,
                   int points) {
  return AP_DrawStar((int)(x + 0.5f), (int)(y + 0.5f),
                     (int)(outer_radius + 0.5f), (int)(inner_radius + 0.5f),
                     points);
}

bool AP_RenderFillStar(float x, float y, float outer_radius, float inner_radius,
                       int points) {
  return AP_FillStar((int)(x + 0.5f), (int)(y + 0.5f),
                     (int)(outer_radius + 0.5f), (int)(inner_radius + 0.5f),
                     points);
}

bool AP_RenderCross(float x, float y, float size) {
  return AP_DrawCross((int)(x + 0.5f), (int)(y + 0.5f), (int)(size + 0.5f));
}

bool AP_RenderGrid(const AP_FRect *rect, int columns, int rows) {
  if (rect == NULL) {
    AP_SET_ERROR(AP_ERROR_INVALID_ARGUMENT, "Grid rectangle cannot be NULL");
    return false;
  }

  return AP_DrawGrid((int)rect->x, (int)rect->y, (int)rect->w, (int)rect->h,
                     columns, rows);
}

bool AP_RenderGeometry(const AP_Vertex *vertices, int num_vertices,
                       const int *indices, int num_indices) {
  AP_U16 *packed;
  int i;
  bool result;

  if (indices == NULL) {
    return AP_RenderPrimitives(vertices, num_vertices, AP_PRIM_TRIANGLES);
  }

  if (num_indices < 0) {
    AP_SET_ERROR(AP_ERROR_INVALID_ARGUMENT, "Invalid geometry index count");
    return false;
  }

  packed = (AP_U16 *)malloc((size_t)num_indices * sizeof(AP_U16));
  if (packed == NULL) {
    AP_SET_ERROR(AP_ERROR_OUT_OF_MEMORY, "Failed to pack geometry indices");
    return false;
  }

  for (i = 0; i < num_indices; ++i) {
    packed[i] = (AP_U16)indices[i];
  }

  result = AP_DrawMeshIndexed(vertices, num_vertices, packed, num_indices,
                              AP_PRIM_TRIANGLES);
  free(packed);
  return result;
}

bool AP_SetRenderViewport(const AP_Rect *rect) {
  if (rect == NULL) {
    return AP_ResetViewport();
  }

  return AP_SetViewport(rect->x, rect->y, rect->w, rect->h);
}

bool AP_GetRenderViewport(AP_Rect *rect) {
  AP_Renderer *renderer = AP_RendererActive();

  if (renderer == NULL) {
    return false;
  }

  if (rect != NULL) {
    *rect = renderer->viewport;
  }

  return true;
}

bool AP_RenderViewportSet(void) {
  AP_Renderer *renderer = AP_RendererActive();
  return renderer != NULL && renderer->viewport_locked;
}

bool AP_SetRenderClipRect(const AP_Rect *rect) {
  if (rect == NULL) {
    return AP_DisableClipRect();
  }

  return AP_SetClipRect(rect->x, rect->y, rect->w, rect->h);
}

bool AP_GetRenderClipRect(AP_Rect *rect) {
  AP_Renderer *renderer = AP_RendererActive();

  if (renderer == NULL) {
    return false;
  }

  if (rect != NULL) {
    *rect = renderer->clip;
  }

  return true;
}

bool AP_RendererSubmitTexturedQuad(AP_UInt texture, AP_BlendMode blend,
                                   const AP_FPoint corners[4],
                                   const AP_FPoint uvs[4], AP_Color tint) {
  AP_Renderer *renderer = AP_RendererActive();
  float x0;
  float y0;
  float x1;
  float y1;
  float x2;
  float y2;
  float x3;
  float y3;

  if (renderer == NULL) {
    return false;
  }

  if (corners == NULL || uvs == NULL) {
    AP_SET_ERROR(AP_ERROR_INVALID_ARGUMENT,
                 "Textured quad corners cannot be NULL");
    return false;
  }

  AP_RendererUseTexture(renderer, (GLuint)texture, blend);

  if (!AP_RendererReserve(renderer, 6)) {
    return false;
  }

  AP_RendererMapPoint(renderer, corners[0].x, corners[0].y, &x0, &y0);
  AP_RendererMapPoint(renderer, corners[1].x, corners[1].y, &x1, &y1);
  AP_RendererMapPoint(renderer, corners[2].x, corners[2].y, &x2, &y2);
  AP_RendererMapPoint(renderer, corners[3].x, corners[3].y, &x3, &y3);

  AP_RendererPushVertexUV(renderer, x0, y0, uvs[0].x, uvs[0].y, tint);
  AP_RendererPushVertexUV(renderer, x1, y1, uvs[1].x, uvs[1].y, tint);
  AP_RendererPushVertexUV(renderer, x2, y2, uvs[2].x, uvs[2].y, tint);
  AP_RendererPushVertexUV(renderer, x0, y0, uvs[0].x, uvs[0].y, tint);
  AP_RendererPushVertexUV(renderer, x2, y2, uvs[2].x, uvs[2].y, tint);
  AP_RendererPushVertexUV(renderer, x3, y3, uvs[3].x, uvs[3].y, tint);
  return true;
}

bool AP_RendererBindTarget(AP_UInt fbo, int width, int height) {
  AP_Renderer *renderer = AP_RendererActive();

  if (renderer == NULL) {
    return false;
  }

  AP_RendererFlushInternal(renderer);
  renderer->target_fbo = (GLuint)fbo;
  glBindFramebuffer(GL_FRAMEBUFFER, (GLuint)fbo);

  if (fbo == 0) {
    renderer->fb_width = renderer->window_fb_width;
    renderer->fb_height = renderer->window_fb_height;
  } else {
    renderer->fb_width = width;
    renderer->fb_height = height;
  }

  if (!renderer->viewport_locked) {
    renderer->viewport.x = 0;
    renderer->viewport.y = 0;
    renderer->viewport.w = renderer->fb_width;
    renderer->viewport.h = renderer->fb_height;
  }

  AP_OpenGLSetViewport(0, 0, renderer->fb_width, renderer->fb_height);
  AP_RendererApplyScissor(renderer);
  return true;
}

bool AP_RendererDrawMesh(AP_UInt texture, AP_BlendMode blend,
                         const AP_Vertex *vertices, int vertex_count,
                         const AP_U16 *indices, int index_count,
                         AP_Primitive primitive) {
  AP_Renderer *renderer = AP_RendererActive();

  if (renderer == NULL) {
    return false;
  }

  if (vertices == NULL || vertex_count < 0) {
    AP_SET_ERROR(AP_ERROR_INVALID_ARGUMENT, "Invalid mesh");
    return false;
  }

  AP_RendererUseTexture(renderer, (GLuint)texture, blend);
  return AP_RendererEmitPrimitive(renderer, vertices, vertex_count, indices,
                                  index_count, primitive);
}
