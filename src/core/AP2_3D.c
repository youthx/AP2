/*
 * AP2 — Application Primitives
 * Copyright (c) 2026 Jack Waechter
 *
 * Licensed under the MIT License.
 * See LICENSE in the project root for full terms.
 */

#include "AP2/AP2_3D.h"

#include "AP2_Internal.h"

#include "AP2/AP2_Error.h"
#include "AP2/AP2_Logger.h"
#include "AP2/AP2_Opengl.h"
#include "AP2/AP2_Shader.h"
#include "AP2/AP2_Texture.h"
#include "AP2/AP2_Window.h"

#define GLFW_INCLUDE_NONE
#include <glad/gl.h>

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define AP_3D_LINE_MAX 256

#if AP_3D_LIGHT_MAX != 16
#error AP_3D_LIGHT_MAX must match the 3D fragment shader array size
#endif

struct AP_Mesh {
  GLuint vao;
  GLuint vbo;
  GLuint ibo;
  int vertex_count;
  int index_count;
};

typedef struct AP_3DState {
  bool initialized;
  bool active;

  AP_Shader *shader;
  GLuint white_texture;
  GLuint line_vao;
  GLuint line_vbo;

  AP_Mesh *cube;
  AP_Mesh *plane;
  AP_Mesh *sphere;

  AP_Mat4 view;
  AP_Mat4 proj;
  AP_Mat4 model;
  AP_Vec3 camera_pos;
  AP_Light lights[AP_3D_LIGHT_MAX];
  AP_Color ambient;
  AP_Color tint;
  AP_F32 shininess;
  AP_F32 specular;
  GLuint texture;
  bool depth_test;
  bool cull;
  bool lights_ready;
  bool specular_ready;
} AP_3DState;

static AP_3DState g_3d;

static const char *AP_3D_VERTEX_SHADER =
    "#version 330 core\n"
    "layout(location = 0) in vec3 a_position;\n"
    "layout(location = 1) in vec3 a_normal;\n"
    "layout(location = 2) in vec2 a_uv;\n"
    "layout(location = 3) in vec4 a_color;\n"
    "uniform mat4 u_mvp;\n"
    "uniform mat4 u_model;\n"
    "uniform mat3 u_normal_matrix;\n"
    "out vec3 v_world_pos;\n"
    "out vec3 v_normal;\n"
    "out vec2 v_uv;\n"
    "out vec4 v_color;\n"
    "void main() {\n"
    "  vec4 world = u_model * vec4(a_position, 1.0);\n"
    "  v_world_pos = world.xyz;\n"
    "  v_normal = u_normal_matrix * a_normal;\n"
    "  v_uv = a_uv;\n"
    "  v_color = a_color;\n"
    "  gl_Position = u_mvp * vec4(a_position, 1.0);\n"
    "}\n";

static const char *AP_3D_FRAGMENT_SHADER =
    "#version 330 core\n"
    "#define AP_LIGHT_MAX 16\n"
    "in vec3 v_world_pos;\n"
    "in vec3 v_normal;\n"
    "in vec2 v_uv;\n"
    "in vec4 v_color;\n"
    "struct Light {\n"
    "  int type;\n"
    "  vec3 position;\n"
    "  vec3 direction;\n"
    "  vec3 color;\n"
    "  float intensity;\n"
    "  float range;\n"
    "  float inner_cone;\n"
    "  float outer_cone;\n"
    "};\n"
    "uniform sampler2D u_texture;\n"
    "uniform Light u_lights[AP_LIGHT_MAX];\n"
    "uniform int u_light_count;\n"
    "uniform vec3 u_ambient_color;\n"
    "uniform vec3 u_camera_pos;\n"
    "uniform vec4 u_tint;\n"
    "uniform float u_lit;\n"
    "uniform float u_shininess;\n"
    "uniform float u_specular;\n"
    "out vec4 frag_color;\n"
    "vec3 evaluate_light(Light light, vec3 n, vec3 world_pos, vec3 view_dir) {\n"
    "  if (light.type == 0 || light.intensity <= 0.0) {\n"
    "    return vec3(0.0);\n"
    "  }\n"
    "  if (light.type == 4) {\n"
    "    return light.color * light.intensity;\n"
    "  }\n"
    "  vec3 L;\n"
    "  float atten = 1.0;\n"
    "  if (light.type == 1) {\n"
    "    L = normalize(-light.direction);\n"
    "  } else {\n"
    "    vec3 to_light = light.position - world_pos;\n"
    "    float dist = length(to_light);\n"
    "    L = to_light / max(dist, 1e-5);\n"
    "    if (light.range > 0.0) {\n"
    "      float x = clamp(dist / light.range, 0.0, 1.0);\n"
    "      atten = (1.0 - x) * (1.0 - x);\n"
    "    }\n"
    "    if (light.type == 3) {\n"
    "      float theta = dot(-L, normalize(light.direction));\n"
    "      float inner = cos(radians(light.inner_cone));\n"
    "      float outer = cos(radians(light.outer_cone));\n"
    "      atten *= clamp((theta - outer) / max(inner - outer, 1e-5), 0.0, 1.0);\n"
    "    }\n"
    "  }\n"
    "  float ndotl = max(dot(n, L), 0.0);\n"
    "  vec3 h = normalize(L + view_dir);\n"
    "  float spec = pow(max(dot(n, h), 0.0), max(u_shininess, 1.0)) * u_specular;\n"
    "  return light.color * light.intensity * atten * (ndotl + spec);\n"
    "}\n"
    "void main() {\n"
    "  vec4 albedo = texture(u_texture, v_uv) * v_color * u_tint;\n"
    "  if (u_lit < 0.5) {\n"
    "    frag_color = albedo;\n"
    "    return;\n"
    "  }\n"
    "  vec3 n = normalize(v_normal);\n"
    "  vec3 view_dir = normalize(u_camera_pos - v_world_pos);\n"
    "  vec3 light = u_ambient_color;\n"
    "  int count = min(u_light_count, AP_LIGHT_MAX);\n"
    "  for (int i = 0; i < AP_LIGHT_MAX; ++i) {\n"
    "    if (i >= count) {\n"
    "      break;\n"
    "    }\n"
    "    light += evaluate_light(u_lights[i], n, v_world_pos, view_dir);\n"
    "  }\n"
    "  frag_color = vec4(albedo.rgb * light, albedo.a);\n"
    "}\n";

/* =========================================================
 * Mesh helpers
 * ========================================================= */

static AP_Vertex3 AP_3DMakeVertex(AP_Vec3 position, AP_Vec3 normal, AP_Vec2 uv,
                                  AP_Color color) {
  AP_Vertex3 v;
  v.position = position;
  v.normal = normal;
  v.uv = uv;
  v.color = color;
  return v;
}

static void AP_3DBindMeshLayout(void) {
  glEnableVertexAttribArray(0);
  glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(AP_Vertex3),
                        (const void *)offsetof(AP_Vertex3, position));
  glEnableVertexAttribArray(1);
  glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(AP_Vertex3),
                        (const void *)offsetof(AP_Vertex3, normal));
  glEnableVertexAttribArray(2);
  glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(AP_Vertex3),
                        (const void *)offsetof(AP_Vertex3, uv));
  glEnableVertexAttribArray(3);
  glVertexAttribPointer(3, 4, GL_FLOAT, GL_FALSE, sizeof(AP_Vertex3),
                        (const void *)offsetof(AP_Vertex3, color));
}

static AP_Mesh *AP_3DUploadMesh(const AP_Vertex3 *vertices, int vertex_count,
                                const AP_U32 *indices, int index_count) {
  AP_Mesh *mesh;

  if (vertices == NULL || vertex_count <= 0) {
    AP_SET_ERROR(AP_ERROR_INVALID_ARGUMENT, "Mesh requires vertices");
    return NULL;
  }

  mesh = (AP_Mesh *)calloc(1, sizeof(AP_Mesh));
  if (mesh == NULL) {
    AP_SET_ERROR(AP_ERROR_OUT_OF_MEMORY, "Failed to allocate mesh");
    return NULL;
  }

  mesh->vertex_count = vertex_count;
  mesh->index_count = index_count;

  glGenVertexArrays(1, &mesh->vao);
  glGenBuffers(1, &mesh->vbo);
  glBindVertexArray(mesh->vao);
  glBindBuffer(GL_ARRAY_BUFFER, mesh->vbo);
  glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)vertex_count * (GLsizeiptr)sizeof(AP_Vertex3),
               vertices, GL_STATIC_DRAW);
  AP_3DBindMeshLayout();

  if (indices != NULL && index_count > 0) {
    glGenBuffers(1, &mesh->ibo);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mesh->ibo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER,
                 (GLsizeiptr)index_count * (GLsizeiptr)sizeof(AP_U32), indices,
                 GL_STATIC_DRAW);
  }

  glBindVertexArray(0);
  return mesh;
}

static void AP_3DPushFace(AP_Vertex3 *vertices, AP_U32 *indices, int *vcount,
                          int *icount, AP_Vec3 a, AP_Vec3 b, AP_Vec3 c,
                          AP_Vec3 d, AP_Vec3 normal) {
  AP_Color white = AP_C4(1.0f, 1.0f, 1.0f, 1.0f);
  int base = *vcount;

  vertices[base + 0] = AP_3DMakeVertex(a, normal, AP_V2(0.0f, 0.0f), white);
  vertices[base + 1] = AP_3DMakeVertex(b, normal, AP_V2(1.0f, 0.0f), white);
  vertices[base + 2] = AP_3DMakeVertex(c, normal, AP_V2(1.0f, 1.0f), white);
  vertices[base + 3] = AP_3DMakeVertex(d, normal, AP_V2(0.0f, 1.0f), white);

  indices[*icount + 0] = (AP_U32)base + 0;
  indices[*icount + 1] = (AP_U32)base + 1;
  indices[*icount + 2] = (AP_U32)base + 2;
  indices[*icount + 3] = (AP_U32)base + 0;
  indices[*icount + 4] = (AP_U32)base + 2;
  indices[*icount + 5] = (AP_U32)base + 3;

  *vcount += 4;
  *icount += 6;
}

static AP_Light AP_LightSanitize(AP_Light light) {
  if (light.type < AP_LIGHT_DISABLED || light.type > AP_LIGHT_AMBIENT) {
    light.type = AP_LIGHT_DISABLED;
  }

  if (light.type == AP_LIGHT_DISABLED) {
    light.enabled = false;
    return light;
  }

  if (AP_Vec3LengthSq(light.direction) > AP_EPSILON) {
    light.direction = AP_Vec3Normalize(light.direction);
  } else {
    light.direction = AP_V3(0.0f, -1.0f, 0.0f);
  }

  light.intensity = AP_Maxf(light.intensity, 0.0f);
  light.range = AP_Maxf(light.range, 0.0f);
  light.inner_cone = AP_Clampf(light.inner_cone, 0.0f, 89.0f);
  light.outer_cone = AP_Clampf(light.outer_cone, light.inner_cone + 0.01f, 90.0f);
  return light;
}

static void AP_3DApplyDefaultLights(void) {
  memset(g_3d.lights, 0, sizeof(g_3d.lights));
  g_3d.lights[0] = AP_LightSanitize(AP_LightDirectional(
      AP_V3(-0.4f, -1.0f, -0.3f), AP_C4(1.0f, 1.0f, 1.0f, 1.0f), 1.0f));
  g_3d.ambient = AP_C4(0.25f, 0.25f, 0.25f, 1.0f);
}

static void AP_3DPrepareLights(bool customize) {
  if (!g_3d.lights_ready) {
    if (customize) {
      memset(g_3d.lights, 0, sizeof(g_3d.lights));
      g_3d.ambient = AP_C4(0.25f, 0.25f, 0.25f, 1.0f);
    } else {
      AP_3DApplyDefaultLights();
    }
    if (g_3d.shininess <= 0.0f) {
      g_3d.shininess = 32.0f;
    }
    if (!g_3d.specular_ready) {
      g_3d.specular = 0.35f;
    }
    g_3d.lights_ready = true;
  }
}

static int AP_3DFindLightType(AP_LightType type) {
  int i;

  for (i = 0; i < AP_3D_LIGHT_MAX; ++i) {
    if (g_3d.lights[i].type == type) {
      return i;
    }
  }
  return -1;
}

static int AP_3DFindFreeLight(void) {
  int i;

  for (i = 0; i < AP_3D_LIGHT_MAX; ++i) {
    if (g_3d.lights[i].type == AP_LIGHT_DISABLED) {
      return i;
    }
  }
  return -1;
}

static void AP_3DUploadLights(void) {
  char name[64];
  int packed = 0;
  int i;

  AP_ShaderSetUniformVec3(g_3d.shader, "u_ambient_color",
                          AP_V3(g_3d.ambient.r, g_3d.ambient.g, g_3d.ambient.b));
  AP_ShaderSetUniformVec3(g_3d.shader, "u_camera_pos", g_3d.camera_pos);
  AP_ShaderSetUniformF(g_3d.shader, "u_shininess", g_3d.shininess);
  AP_ShaderSetUniformF(g_3d.shader, "u_specular", g_3d.specular);

  for (i = 0; i < AP_3D_LIGHT_MAX; ++i) {
    const AP_Light *light = &g_3d.lights[i];

    if (!light->enabled || light->type == AP_LIGHT_DISABLED) {
      continue;
    }

    snprintf(name, sizeof(name), "u_lights[%d].type", packed);
    AP_ShaderSetUniformI(g_3d.shader, name, (AP_Int)light->type);
    snprintf(name, sizeof(name), "u_lights[%d].position", packed);
    AP_ShaderSetUniformVec3(g_3d.shader, name, light->position);
    snprintf(name, sizeof(name), "u_lights[%d].direction", packed);
    AP_ShaderSetUniformVec3(g_3d.shader, name, light->direction);
    snprintf(name, sizeof(name), "u_lights[%d].color", packed);
    AP_ShaderSetUniformVec3(g_3d.shader, name,
                            AP_V3(light->color.r, light->color.g, light->color.b));
    snprintf(name, sizeof(name), "u_lights[%d].intensity", packed);
    AP_ShaderSetUniformF(g_3d.shader, name, light->intensity);
    snprintf(name, sizeof(name), "u_lights[%d].range", packed);
    AP_ShaderSetUniformF(g_3d.shader, name, light->range);
    snprintf(name, sizeof(name), "u_lights[%d].inner_cone", packed);
    AP_ShaderSetUniformF(g_3d.shader, name, light->inner_cone);
    snprintf(name, sizeof(name), "u_lights[%d].outer_cone", packed);
    AP_ShaderSetUniformF(g_3d.shader, name, light->outer_cone);
    packed++;
  }

  AP_ShaderSetUniformI(g_3d.shader, "u_light_count", packed);
}

static bool AP_3DEnsure(void) {
  unsigned char white[4] = {255, 255, 255, 255};

  if (g_3d.initialized) {
    return true;
  }

  if (!AP_OpenGLIsInitialized()) {
    AP_SET_ERROR(AP_ERROR_NOT_INITIALIZED, "3D requires an active window");
    return false;
  }

  g_3d.shader = AP_CreateShader(AP_3D_VERTEX_SHADER, AP_3D_FRAGMENT_SHADER);
  if (g_3d.shader == NULL) {
    return false;
  }

  glGenTextures(1, &g_3d.white_texture);
  glBindTexture(GL_TEXTURE_2D, g_3d.white_texture);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE,
               white);

  glGenVertexArrays(1, &g_3d.line_vao);
  glGenBuffers(1, &g_3d.line_vbo);
  glBindVertexArray(g_3d.line_vao);
  glBindBuffer(GL_ARRAY_BUFFER, g_3d.line_vbo);
  glBufferData(GL_ARRAY_BUFFER,
               (GLsizeiptr)AP_3D_LINE_MAX * (GLsizeiptr)sizeof(AP_Vertex3),
               NULL, GL_DYNAMIC_DRAW);
  AP_3DBindMeshLayout();
  glBindVertexArray(0);

  g_3d.cube = AP_CreateMeshCube(1.0f);
  g_3d.plane = AP_CreateMeshPlane(1.0f, 1.0f);
  g_3d.sphere = AP_CreateMeshSphere(1.0f, 16, 12);
  if (g_3d.cube == NULL || g_3d.plane == NULL || g_3d.sphere == NULL) {
    AP_3DShutdown();
    return false;
  }

  g_3d.model = AP_Mat4Identity();
  g_3d.view = AP_Mat4Identity();
  g_3d.proj = AP_Mat4Identity();
  g_3d.camera_pos = AP_V3(0.0f, 4.0f, 8.0f);
  g_3d.tint = AP_C4(1.0f, 1.0f, 1.0f, 1.0f);
  g_3d.texture = g_3d.white_texture;
  g_3d.depth_test = true;
  g_3d.cull = true;
  AP_3DPrepareLights(false);
  g_3d.initialized = true;

  AP_INFO("3D initialized");
  return true;
}

static AP_F32 AP_3DAspect(void) {
  int width = AP_GetWindowPixelWidth();
  int height = AP_GetWindowPixelHeight();
  if (width <= 0 || height <= 0) {
    return 16.0f / 9.0f;
  }
  return (AP_F32)width / (AP_F32)height;
}

static void AP_3DApplyRasterState(void) {
  if (g_3d.depth_test) {
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);
  } else {
    glDisable(GL_DEPTH_TEST);
  }

  if (g_3d.cull) {
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glFrontFace(GL_CCW);
  } else {
    glDisable(GL_CULL_FACE);
  }
}

static bool AP_3DSubmit(const AP_Mesh *mesh, const AP_Mat4 *model, AP_Color tint,
                        bool lit, GLenum mode) {
  AP_Mat4 used_model = model != NULL ? *model : g_3d.model;
  AP_Mat4 mvp = AP_Mat4Mul(g_3d.proj, AP_Mat4Mul(g_3d.view, used_model));
  AP_Mat3 normal = AP_Mat4NormalMatrix(used_model);

  if (!g_3d.active || mesh == NULL || mesh->vao == 0) {
    AP_SET_ERROR(AP_ERROR_INVALID_STATE, "3D pass is not active");
    return false;
  }

  AP_ShaderBind(g_3d.shader);
  AP_ShaderSetUniformMat4(g_3d.shader, "u_mvp", mvp.m, false);
  AP_ShaderSetUniformMat4(g_3d.shader, "u_model", used_model.m, false);
  AP_ShaderSetUniformMat3(g_3d.shader, "u_normal_matrix", normal.m, false);
  AP_3DUploadLights();
  AP_ShaderSetUniformColor(g_3d.shader, "u_tint", tint);
  AP_ShaderSetUniformF(g_3d.shader, "u_lit", lit ? 1.0f : 0.0f);

  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, g_3d.texture != 0 ? g_3d.texture : g_3d.white_texture);
  AP_ShaderSetUniformI(g_3d.shader, "u_texture", 0);

  glBindVertexArray(mesh->vao);
  if (mesh->index_count > 0) {
    glDrawElements(mode, mesh->index_count, GL_UNSIGNED_INT, NULL);
  } else {
    glDrawArrays(mode, 0, mesh->vertex_count);
  }
  glBindVertexArray(0);
  return true;
}

static bool AP_3DSubmitLines(const AP_Vertex3 *vertices, int count,
                             AP_Color tint) {
  AP_Mat4 mvp;
  AP_Mat3 normal;

  if (!g_3d.active || vertices == NULL || count < 2) {
    AP_SET_ERROR(AP_ERROR_INVALID_STATE, "3D line submit is invalid");
    return false;
  }

  if (count > AP_3D_LINE_MAX) {
    count = AP_3D_LINE_MAX;
  }

  mvp = AP_Mat4Mul(g_3d.proj, AP_Mat4Mul(g_3d.view, g_3d.model));
  normal = AP_Mat4NormalMatrix(g_3d.model);

  AP_ShaderBind(g_3d.shader);
  AP_ShaderSetUniformMat4(g_3d.shader, "u_mvp", mvp.m, false);
  AP_ShaderSetUniformMat4(g_3d.shader, "u_model", g_3d.model.m, false);
  AP_ShaderSetUniformMat3(g_3d.shader, "u_normal_matrix", normal.m, false);
  AP_ShaderSetUniformColor(g_3d.shader, "u_tint", tint);
  AP_ShaderSetUniformF(g_3d.shader, "u_lit", 0.0f);
  AP_ShaderSetUniformI(g_3d.shader, "u_texture", 0);

  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, g_3d.white_texture);
  glBindVertexArray(g_3d.line_vao);
  glBindBuffer(GL_ARRAY_BUFFER, g_3d.line_vbo);
  glBufferSubData(GL_ARRAY_BUFFER, 0,
                  (GLsizeiptr)count * (GLsizeiptr)sizeof(AP_Vertex3), vertices);
  glDrawArrays(GL_LINES, 0, count);
  glBindVertexArray(0);
  return true;
}

/* =========================================================
 * Camera
 * ========================================================= */

AP_Camera AP_CameraPerspective(AP_Vec3 position, AP_Vec3 target,
                               AP_F32 fov_degrees) {
  AP_Camera camera;
  camera.position = position;
  camera.target = target;
  camera.up = AP_Vec3Up();
  camera.fov_degrees = fov_degrees > 0.0f ? fov_degrees : 60.0f;
  camera.near_z = 0.1f;
  camera.far_z = 250.0f;
  camera.ortho_size = 10.0f;
  return camera;
}

AP_Camera AP_CameraOrtho(AP_Vec3 position, AP_Vec3 target, AP_F32 ortho_size) {
  AP_Camera camera = AP_CameraPerspective(position, target, 0.0f);
  camera.fov_degrees = 0.0f;
  camera.ortho_size = ortho_size > 0.0f ? ortho_size : 10.0f;
  return camera;
}

AP_Camera AP_CameraDefault(void) {
  return AP_CameraPerspective(AP_V3(0.0f, 4.0f, 8.0f), AP_Vec3Zero(), 50.0f);
}

AP_Mat4 AP_CameraView(const AP_Camera *camera) {
  if (camera == NULL) {
    return AP_Mat4Identity();
  }
  return AP_Mat4LookAt(camera->position, camera->target, camera->up);
}

AP_Mat4 AP_CameraProjection(const AP_Camera *camera, AP_F32 aspect) {
  if (camera == NULL) {
    return AP_Mat4Identity();
  }

  if (camera->fov_degrees <= 0.0f) {
    return AP_Mat4OrthoSize(camera->ortho_size, aspect, camera->near_z,
                            camera->far_z);
  }

  return AP_Mat4Perspective(camera->fov_degrees, aspect, camera->near_z,
                            camera->far_z);
}

AP_Mat4 AP_CameraViewProjection(const AP_Camera *camera, AP_F32 aspect) {
  return AP_Mat4Mul(AP_CameraProjection(camera, aspect), AP_CameraView(camera));
}

/* =========================================================
 * Mesh
 * ========================================================= */

AP_Mesh *AP_CreateMesh(const AP_Vertex3 *vertices, int vertex_count,
                       const AP_U32 *indices, int index_count) {
  if (!AP_3DEnsure()) {
    return NULL;
  }
  return AP_3DUploadMesh(vertices, vertex_count, indices, index_count);
}

AP_Mesh *AP_CreateMeshCube(AP_F32 size) {
  AP_Vertex3 vertices[24];
  AP_U32 indices[36];
  AP_F32 h = size * 0.5f;
  int vcount = 0;
  int icount = 0;

  AP_3DPushFace(vertices, indices, &vcount, &icount, AP_V3(-h, -h, h),
                AP_V3(h, -h, h), AP_V3(h, h, h), AP_V3(-h, h, h),
                AP_V3(0.0f, 0.0f, 1.0f));
  AP_3DPushFace(vertices, indices, &vcount, &icount, AP_V3(h, -h, -h),
                AP_V3(-h, -h, -h), AP_V3(-h, h, -h), AP_V3(h, h, -h),
                AP_V3(0.0f, 0.0f, -1.0f));
  AP_3DPushFace(vertices, indices, &vcount, &icount, AP_V3(-h, -h, -h),
                AP_V3(-h, -h, h), AP_V3(-h, h, h), AP_V3(-h, h, -h),
                AP_V3(-1.0f, 0.0f, 0.0f));
  AP_3DPushFace(vertices, indices, &vcount, &icount, AP_V3(h, -h, h),
                AP_V3(h, -h, -h), AP_V3(h, h, -h), AP_V3(h, h, h),
                AP_V3(1.0f, 0.0f, 0.0f));
  AP_3DPushFace(vertices, indices, &vcount, &icount, AP_V3(-h, h, h),
                AP_V3(h, h, h), AP_V3(h, h, -h), AP_V3(-h, h, -h),
                AP_V3(0.0f, 1.0f, 0.0f));
  AP_3DPushFace(vertices, indices, &vcount, &icount, AP_V3(-h, -h, -h),
                AP_V3(h, -h, -h), AP_V3(h, -h, h), AP_V3(-h, -h, h),
                AP_V3(0.0f, -1.0f, 0.0f));

  return AP_3DUploadMesh(vertices, vcount, indices, icount);
}

AP_Mesh *AP_CreateMeshPlane(AP_F32 width, AP_F32 depth) {
  AP_Vertex3 vertices[4];
  AP_U32 indices[6];
  AP_F32 hx = width * 0.5f;
  AP_F32 hz = depth * 0.5f;
  AP_Color white = AP_C4(1.0f, 1.0f, 1.0f, 1.0f);
  AP_Vec3 n = AP_Vec3Up();

  vertices[0] = AP_3DMakeVertex(AP_V3(-hx, 0.0f, -hz), n, AP_V2(0.0f, 0.0f), white);
  vertices[1] = AP_3DMakeVertex(AP_V3(hx, 0.0f, -hz), n, AP_V2(1.0f, 0.0f), white);
  vertices[2] = AP_3DMakeVertex(AP_V3(hx, 0.0f, hz), n, AP_V2(1.0f, 1.0f), white);
  vertices[3] = AP_3DMakeVertex(AP_V3(-hx, 0.0f, hz), n, AP_V2(0.0f, 1.0f), white);

  indices[0] = 0;
  indices[1] = 1;
  indices[2] = 2;
  indices[3] = 0;
  indices[4] = 2;
  indices[5] = 3;
  return AP_3DUploadMesh(vertices, 4, indices, 6);
}

AP_Mesh *AP_CreateMeshSphere(AP_F32 radius, int slices, int stacks) {
  AP_Vertex3 *vertices;
  AP_U32 *indices;
  AP_Mesh *mesh;
  AP_Color white = AP_C4(1.0f, 1.0f, 1.0f, 1.0f);
  int vertex_count;
  int index_count;
  int stack;
  int slice;
  int v = 0;
  int i = 0;

  if (slices < 3) {
    slices = 3;
  }
  if (stacks < 2) {
    stacks = 2;
  }
  if (radius <= 0.0f) {
    radius = 1.0f;
  }

  vertex_count = (stacks + 1) * (slices + 1);
  index_count = stacks * slices * 6;
  vertices = (AP_Vertex3 *)malloc((size_t)vertex_count * sizeof(AP_Vertex3));
  indices = (AP_U32 *)malloc((size_t)index_count * sizeof(AP_U32));
  if (vertices == NULL || indices == NULL) {
    free(vertices);
    free(indices);
    AP_SET_ERROR(AP_ERROR_OUT_OF_MEMORY, "Failed to allocate sphere mesh");
    return NULL;
  }

  for (stack = 0; stack <= stacks; ++stack) {
    AP_F32 v_coord = (AP_F32)stack / (AP_F32)stacks;
    AP_F32 phi = v_coord * AP_PI;
    AP_F32 y = cosf(phi);
    AP_F32 ring = sinf(phi);

    for (slice = 0; slice <= slices; ++slice) {
      AP_F32 u_coord = (AP_F32)slice / (AP_F32)slices;
      AP_F32 theta = u_coord * AP_TAU;
      AP_Vec3 n = AP_Vec3Normalize(AP_V3(ring * cosf(theta), y, ring * sinf(theta)));
      vertices[v++] =
          AP_3DMakeVertex(AP_Vec3Scale(n, radius), n, AP_V2(u_coord, 1.0f - v_coord),
                          white);
    }
  }

  for (stack = 0; stack < stacks; ++stack) {
    for (slice = 0; slice < slices; ++slice) {
      AP_U32 a = (AP_U32)(stack * (slices + 1) + slice);
      AP_U32 b = a + (AP_U32)(slices + 1);
      indices[i++] = a;
      indices[i++] = b;
      indices[i++] = a + 1;
      indices[i++] = a + 1;
      indices[i++] = b;
      indices[i++] = b + 1;
    }
  }

  mesh = AP_3DUploadMesh(vertices, vertex_count, indices, index_count);
  free(vertices);
  free(indices);
  return mesh;
}

void AP_DestroyMesh(AP_Mesh *mesh) {
  if (mesh == NULL) {
    return;
  }

  if (mesh == g_3d.cube || mesh == g_3d.plane || mesh == g_3d.sphere) {
    AP_SET_ERROR(AP_ERROR_INVALID_ARGUMENT, "Cannot destroy builtin 3D mesh");
    return;
  }

  if (mesh->vao != 0) {
    glDeleteVertexArrays(1, &mesh->vao);
  }
  if (mesh->vbo != 0) {
    glDeleteBuffers(1, &mesh->vbo);
  }
  if (mesh->ibo != 0) {
    glDeleteBuffers(1, &mesh->ibo);
  }

  free(mesh);
}

bool AP_MeshIsValid(const AP_Mesh *mesh) {
  return mesh != NULL && mesh->vao != 0 && mesh->vertex_count > 0;
}

int AP_MeshVertexCount(const AP_Mesh *mesh) {
  return mesh != NULL ? mesh->vertex_count : 0;
}

int AP_MeshIndexCount(const AP_Mesh *mesh) {
  return mesh != NULL ? mesh->index_count : 0;
}

/* =========================================================
 * Pass
 * ========================================================= */

bool AP_Begin3D(const AP_Camera *camera) {
  const AP_Camera *used = camera != NULL ? camera : NULL;
  AP_Camera fallback;

  AP_RendererFlushCurrent();

  if (!AP_3DEnsure()) {
    return false;
  }

  if (used == NULL) {
    fallback = AP_CameraDefault();
    used = &fallback;
  }

  g_3d.view = AP_CameraView(used);
  g_3d.proj = AP_CameraProjection(used, AP_3DAspect());
  g_3d.camera_pos = used->position;
  g_3d.model = AP_Mat4Identity();
  g_3d.tint = AP_C4(1.0f, 1.0f, 1.0f, 1.0f);
  g_3d.texture = g_3d.white_texture;
  g_3d.active = true;

  AP_3DApplyRasterState();
  AP_ShaderBind(g_3d.shader);
  return true;
}

void AP_End3D(void) {
  if (!g_3d.active) {
    return;
  }

  g_3d.active = false;
  glDisable(GL_DEPTH_TEST);
  glDisable(GL_CULL_FACE);
  glBindVertexArray(0);
  AP_ShaderUnbind();
}

bool AP_Is3D(void) { return g_3d.active; }

bool AP_Set3DModel(const AP_Mat4 *model) {
  if (model == NULL) {
    AP_SET_ERROR(AP_ERROR_INVALID_ARGUMENT, "3D model matrix is NULL");
    return false;
  }
  g_3d.model = *model;
  return true;
}

AP_Mat4 AP_Get3DModel(void) { return g_3d.model; }

void AP_Reset3DModel(void) { g_3d.model = AP_Mat4Identity(); }

bool AP_Set3DTexture(AP_Texture *texture) {
  AP_UInt id = AP_TextureNativeId(texture);
  g_3d.texture = id != 0 ? (GLuint)id : g_3d.white_texture;
  return true;
}

bool AP_Set3DTint(AP_Color tint) {
  g_3d.tint = tint;
  return true;
}

bool AP_Set3DShininess(AP_F32 shininess) {
  g_3d.shininess = AP_Maxf(shininess, 1.0f);
  return true;
}

bool AP_Set3DSpecular(AP_F32 strength) {
  g_3d.specular = AP_Maxf(strength, 0.0f);
  g_3d.specular_ready = true;
  return true;
}

/* =========================================================
 * Lights
 * ========================================================= */

AP_Light AP_LightDirectional(AP_Vec3 direction, AP_Color color,
                             AP_F32 intensity) {
  AP_Light light;

  memset(&light, 0, sizeof(light));
  light.type = AP_LIGHT_DIRECTIONAL;
  light.enabled = true;
  light.direction = direction;
  light.color = color;
  light.intensity = intensity;
  light.range = 0.0f;
  light.inner_cone = 0.0f;
  light.outer_cone = 0.0f;
  return AP_LightSanitize(light);
}

AP_Light AP_LightPoint(AP_Vec3 position, AP_Color color, AP_F32 intensity,
                       AP_F32 range) {
  AP_Light light;

  memset(&light, 0, sizeof(light));
  light.type = AP_LIGHT_POINT;
  light.enabled = true;
  light.position = position;
  light.direction = AP_V3(0.0f, -1.0f, 0.0f);
  light.color = color;
  light.intensity = intensity;
  light.range = range;
  light.inner_cone = 0.0f;
  light.outer_cone = 0.0f;
  return AP_LightSanitize(light);
}

AP_Light AP_LightSpot(AP_Vec3 position, AP_Vec3 direction, AP_Color color,
                      AP_F32 intensity, AP_F32 range, AP_F32 inner_cone,
                      AP_F32 outer_cone) {
  AP_Light light;

  memset(&light, 0, sizeof(light));
  light.type = AP_LIGHT_SPOT;
  light.enabled = true;
  light.position = position;
  light.direction = direction;
  light.color = color;
  light.intensity = intensity;
  light.range = range;
  light.inner_cone = inner_cone;
  light.outer_cone = outer_cone;
  return AP_LightSanitize(light);
}

AP_Light AP_LightAmbient(AP_Color color, AP_F32 intensity) {
  AP_Light light;

  memset(&light, 0, sizeof(light));
  light.type = AP_LIGHT_AMBIENT;
  light.enabled = true;
  light.direction = AP_V3(0.0f, -1.0f, 0.0f);
  light.color = color;
  light.intensity = intensity;
  return AP_LightSanitize(light);
}

int AP_AddLight(AP_Light light) {
  int index;

  AP_3DPrepareLights(true);
  light = AP_LightSanitize(light);
  if (light.type == AP_LIGHT_DISABLED) {
    AP_SET_ERROR(AP_ERROR_INVALID_ARGUMENT, "Cannot add a disabled light");
    return -1;
  }

  index = AP_3DFindFreeLight();
  if (index < 0) {
    AP_SET_ERROR(AP_ERROR_OUT_OF_MEMORY, "3D light list is full");
    return -1;
  }

  light.enabled = true;
  g_3d.lights[index] = light;
  return index;
}

bool AP_SetLight(int index, AP_Light light) {
  if (index < 0 || index >= AP_3D_LIGHT_MAX) {
    AP_SET_ERROR(AP_ERROR_INVALID_ARGUMENT, "Light index is out of range");
    return false;
  }

  AP_3DPrepareLights(true);
  g_3d.lights[index] = AP_LightSanitize(light);
  return true;
}

bool AP_GetLight(int index, AP_Light *out) {
  if (index < 0 || index >= AP_3D_LIGHT_MAX || out == NULL) {
    AP_SET_ERROR(AP_ERROR_INVALID_ARGUMENT, "Light get is invalid");
    return false;
  }

  AP_3DPrepareLights(false);
  *out = g_3d.lights[index];
  return true;
}

bool AP_EnableLight(int index, bool enabled) {
  if (index < 0 || index >= AP_3D_LIGHT_MAX) {
    AP_SET_ERROR(AP_ERROR_INVALID_ARGUMENT, "Light index is out of range");
    return false;
  }

  AP_3DPrepareLights(true);
  if (g_3d.lights[index].type == AP_LIGHT_DISABLED) {
    AP_SET_ERROR(AP_ERROR_INVALID_STATE, "Light slot is empty");
    return false;
  }

  g_3d.lights[index].enabled = enabled;
  return true;
}

bool AP_RemoveLight(int index) {
  if (index < 0 || index >= AP_3D_LIGHT_MAX) {
    AP_SET_ERROR(AP_ERROR_INVALID_ARGUMENT, "Light index is out of range");
    return false;
  }

  AP_3DPrepareLights(true);
  memset(&g_3d.lights[index], 0, sizeof(g_3d.lights[index]));
  g_3d.lights[index].type = AP_LIGHT_DISABLED;
  g_3d.lights[index].enabled = false;
  return true;
}

void AP_ClearLights(void) {
  AP_3DPrepareLights(true);
  memset(g_3d.lights, 0, sizeof(g_3d.lights));
}

int AP_GetLightCount(void) {
  int count = 0;
  int i;

  AP_3DPrepareLights(false);
  for (i = 0; i < AP_3D_LIGHT_MAX; ++i) {
    if (g_3d.lights[i].type != AP_LIGHT_DISABLED) {
      count++;
    }
  }
  return count;
}

bool AP_SetAmbientLight(AP_Color color) {
  AP_3DPrepareLights(true);
  g_3d.ambient = color;
  return true;
}

AP_Color AP_GetAmbientLight(void) {
  AP_3DPrepareLights(false);
  return g_3d.ambient;
}

bool AP_Set3DLight(AP_Vec3 direction, AP_Color color, AP_F32 ambient) {
  AP_Light light;
  int index;
  AP_F32 amount;

  if (AP_Vec3LengthSq(direction) <= AP_EPSILON) {
    AP_SET_ERROR(AP_ERROR_INVALID_ARGUMENT, "Light direction is zero");
    return false;
  }

  AP_3DPrepareLights(true);
  light = AP_LightDirectional(direction, color, 1.0f);
  index = AP_3DFindLightType(AP_LIGHT_DIRECTIONAL);
  if (index >= 0) {
    g_3d.lights[index] = light;
  } else if (AP_AddLight(light) < 0) {
    return false;
  }

  amount = AP_Saturate(ambient);
  g_3d.ambient = AP_C4(amount, amount, amount, 1.0f);
  return true;
}

bool AP_Set3DDepthTest(bool enabled) {
  g_3d.depth_test = enabled;
  if (g_3d.active) {
    AP_3DApplyRasterState();
  }
  return true;
}

bool AP_Set3DCullFace(bool enabled) {
  g_3d.cull = enabled;
  if (g_3d.active) {
    AP_3DApplyRasterState();
  }
  return true;
}

/* =========================================================
 * Draw
 * ========================================================= */

bool AP_DrawMesh(const AP_Mesh *mesh) {
  return AP_3DSubmit(mesh, NULL, g_3d.tint, true, GL_TRIANGLES);
}

bool AP_DrawMeshEx(const AP_Mesh *mesh, const AP_Mat4 *model, AP_Color tint) {
  return AP_3DSubmit(mesh, model, tint, true, GL_TRIANGLES);
}

bool AP_DrawCube(AP_Vec3 center, AP_Vec3 size, AP_Color color) {
  AP_Mat4 model =
      AP_Mat4TRS(center, AP_QuatIdentity(), size);
  return AP_3DSubmit(g_3d.cube, &model, color, true, GL_TRIANGLES);
}

bool AP_DrawPlane(AP_Vec3 center, AP_F32 width, AP_F32 depth, AP_Color color) {
  AP_Mat4 model =
      AP_Mat4TRS(center, AP_QuatIdentity(), AP_V3(width, 1.0f, depth));
  return AP_3DSubmit(g_3d.plane, &model, color, true, GL_TRIANGLES);
}

bool AP_DrawSphere(AP_Vec3 center, AP_F32 radius, AP_Color color) {
  AP_Mat4 model =
      AP_Mat4TRS(center, AP_QuatIdentity(), AP_V3(radius, radius, radius));
  return AP_3DSubmit(g_3d.sphere, &model, color, true, GL_TRIANGLES);
}

bool AP_DrawLine3D(AP_Vec3 a, AP_Vec3 b, AP_Color color) {
  AP_Vertex3 verts[2];
  AP_Vec3 n = AP_Vec3Up();
  verts[0] = AP_3DMakeVertex(a, n, AP_V2(0.0f, 0.0f), color);
  verts[1] = AP_3DMakeVertex(b, n, AP_V2(1.0f, 0.0f), color);
  return AP_3DSubmitLines(verts, 2, AP_C4(1.0f, 1.0f, 1.0f, 1.0f));
}

bool AP_DrawGrid3D(AP_F32 size, int divisions, AP_Color color) {
  AP_Vertex3 verts[AP_3D_LINE_MAX];
  AP_Vec3 n = AP_Vec3Up();
  AP_F32 half;
  AP_F32 step;
  int count = 0;
  int line;

  if (divisions < 1) {
    divisions = 1;
  }
  if (size <= 0.0f) {
    size = 10.0f;
  }

  half = size * 0.5f;
  step = size / (AP_F32)divisions;

  for (line = 0; line <= divisions && count + 4 <= AP_3D_LINE_MAX; ++line) {
    AP_F32 t = -half + step * (AP_F32)line;
    verts[count++] = AP_3DMakeVertex(AP_V3(-half, 0.0f, t), n, AP_V2(0.0f, 0.0f), color);
    verts[count++] = AP_3DMakeVertex(AP_V3(half, 0.0f, t), n, AP_V2(1.0f, 0.0f), color);
    verts[count++] = AP_3DMakeVertex(AP_V3(t, 0.0f, -half), n, AP_V2(0.0f, 0.0f), color);
    verts[count++] = AP_3DMakeVertex(AP_V3(t, 0.0f, half), n, AP_V2(1.0f, 0.0f), color);
  }

  return AP_3DSubmitLines(verts, count, AP_C4(1.0f, 1.0f, 1.0f, 1.0f));
}

void AP_3DShutdown(void) {
  g_3d.active = false;

  if (g_3d.cube != NULL) {
    if (g_3d.cube->vao != 0) {
      glDeleteVertexArrays(1, &g_3d.cube->vao);
    }
    if (g_3d.cube->vbo != 0) {
      glDeleteBuffers(1, &g_3d.cube->vbo);
    }
    if (g_3d.cube->ibo != 0) {
      glDeleteBuffers(1, &g_3d.cube->ibo);
    }
    free(g_3d.cube);
    g_3d.cube = NULL;
  }

  if (g_3d.plane != NULL) {
    if (g_3d.plane->vao != 0) {
      glDeleteVertexArrays(1, &g_3d.plane->vao);
    }
    if (g_3d.plane->vbo != 0) {
      glDeleteBuffers(1, &g_3d.plane->vbo);
    }
    if (g_3d.plane->ibo != 0) {
      glDeleteBuffers(1, &g_3d.plane->ibo);
    }
    free(g_3d.plane);
    g_3d.plane = NULL;
  }

  if (g_3d.sphere != NULL) {
    if (g_3d.sphere->vao != 0) {
      glDeleteVertexArrays(1, &g_3d.sphere->vao);
    }
    if (g_3d.sphere->vbo != 0) {
      glDeleteBuffers(1, &g_3d.sphere->vbo);
    }
    if (g_3d.sphere->ibo != 0) {
      glDeleteBuffers(1, &g_3d.sphere->ibo);
    }
    free(g_3d.sphere);
    g_3d.sphere = NULL;
  }

  if (g_3d.line_vao != 0) {
    glDeleteVertexArrays(1, &g_3d.line_vao);
    g_3d.line_vao = 0;
  }
  if (g_3d.line_vbo != 0) {
    glDeleteBuffers(1, &g_3d.line_vbo);
    g_3d.line_vbo = 0;
  }
  if (g_3d.white_texture != 0) {
    glDeleteTextures(1, &g_3d.white_texture);
    g_3d.white_texture = 0;
  }
  if (g_3d.shader != NULL) {
    AP_ShaderDestroyInternal(g_3d.shader);
    g_3d.shader = NULL;
  }

  memset(&g_3d, 0, sizeof(g_3d));
}
