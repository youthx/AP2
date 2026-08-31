/*
 * AP2 — Application Primitives
 * Copyright (c) 2024-2026 Jack Waechter
 *
 * Licensed under the MIT License.
 * See LICENSE in the project root for full terms.
 */

 #include "AP2/AP2_3D.h"

 #include "AP2_Internal.h"

 #include "AP2/AP2_Error.h"
 #include "AP2/AP2_Logger.h"
 #include "AP2/AP2_Material.h"
 #include "AP2/AP2_Opengl.h"
 #include "AP2/AP2_Renderer.h"
 #include "AP2/AP2_Shader.h"
 #include "AP2/AP2_Texture.h"
 #include "AP2/AP2_Window.h"

 #define GLFW_INCLUDE_NONE
 #include <glad/gl.h>

 #define CGLTF_IMPLEMENTATION
 #include "AP2/cgltf.h"

 #include <limits.h>
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

   AP_Material *material; /* owned by mesh if material_owned == true */
   bool material_owned;
 };

 struct AP_Model {
   AP_Mesh **meshes;
   AP_Mat4 *local_transforms; /* per-mesh local transform (mesh space → model) */

   int mesh_count;

   AP_Mat4 transform; /* root model transform (model → world) */

   AP_Material *materials; /* contiguous array owned by the model */
   int material_count;

   /* GPU textures created while loading (owned; destroyed with model) */
   AP_Texture **textures;
   int texture_count;
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
   const AP_Material *material; /* optional global override; not owned */
   GLuint depth_rbo;
   GLuint depth_fbo;
   int depth_w;
   int depth_h;
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
     "uniform float u_alpha_cutoff;\n"
     "uniform int u_alpha_mode;\n" /* 0=OPAQUE 1=MASK 2=BLEND */
     "out vec4 frag_color;\n"
     "vec3 evaluate_light(Light light, vec3 n, vec3 world_pos, vec3 view_dir) "
     "{\n"
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
     "      atten *= clamp((theta - outer) / max(inner - outer, 1e-5), 0.0, "
     "1.0);\n"
     "    }\n"
     "  }\n"
     "  float ndotl = max(dot(n, L), 0.0);\n"
     "  vec3 h = normalize(L + view_dir);\n"
     "  float spec = pow(max(dot(n, h), 0.0), max(u_shininess, 1.0)) * "
     "u_specular;\n"
     "  return light.color * light.intensity * atten * (ndotl + spec);\n"
     "}\n"
     "void main() {\n"
     "  vec4 albedo = texture(u_texture, v_uv) * v_color * u_tint;\n"
     "  if (u_alpha_mode == 1 && albedo.a < u_alpha_cutoff) {\n"
     "    discard;\n"
     "  }\n"
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
   mesh->material = NULL;
   mesh->material_owned = false;

   glGenVertexArrays(1, &mesh->vao);
   glGenBuffers(1, &mesh->vbo);
   glBindVertexArray(mesh->vao);
   glBindBuffer(GL_ARRAY_BUFFER, mesh->vbo);
   glBufferData(GL_ARRAY_BUFFER,
                (GLsizeiptr)vertex_count * (GLsizeiptr)sizeof(AP_Vertex3),
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

 static bool AP_3DReadPrimitive(const cgltf_primitive *primitive,
                                AP_Vertex3 **out_vertices, int *out_vertex_count,
                                AP_U32 **out_indices, int *out_index_count) {
   const cgltf_accessor *position = NULL;
   const cgltf_accessor *normal = NULL;
   const cgltf_accessor *texcoord = NULL;
   const cgltf_accessor *color = NULL;

   AP_Vertex3 *vertices = NULL;
   AP_U32 *indices = NULL;

   cgltf_size vertex_count;
   cgltf_size index_count;
   cgltf_size i;

   if (primitive == NULL || primitive->type != cgltf_primitive_type_triangles) {
     AP_SET_ERROR(AP_ERROR_OPERATION_FAILED,
                  "Invalid primitive. Only triangles are supported currently.");
     return false;
   }

   for (i = 0; i < primitive->attributes_count; ++i) {
     const cgltf_attribute *attr = &primitive->attributes[i];

     switch (attr->type) {
     case cgltf_attribute_type_position:
       position = attr->data;
       break;
     case cgltf_attribute_type_normal:
       normal = attr->data;
       break;
     case cgltf_attribute_type_texcoord:
       texcoord = attr->data;
       break;
     case cgltf_attribute_type_color:
       color = attr->data;
       break;
     default:
       AP_WARN("Unsupported attribute type: %d", attr->type);
       break;
     }
   }

   if (position == NULL) {
     AP_SET_ERROR(AP_ERROR_OPERATION_FAILED, "Missing position attribute.");
     return false;
   }

   vertex_count = position->count;
   if (vertex_count > INT_MAX) {
     AP_SET_ERROR(AP_ERROR_OPERATION_FAILED, "Vertex count is too large.");
     return false;
   }

   vertices = calloc((size_t)vertex_count, sizeof(AP_Vertex3));
   if (vertices == NULL) {
     AP_SET_ERROR(AP_ERROR_OUT_OF_MEMORY, "Failed to allocate vertices.");
     return false;
   }

   for (i = 0; i < vertex_count; ++i) {
     float p[4] = {0};
     float n[4] = {0};
     float uv[2] = {0};
     float c[4] = {1.0f, 1.0f, 1.0f, 1.0f};

     cgltf_accessor_read_float(position, i, p, 3);

     if (normal != NULL) {
       cgltf_accessor_read_float(normal, i, n, 3);
     } else {
       n[0] = 0.0f;
       n[1] = 1.0f;
       n[2] = 0.0f;
     }

     if (texcoord != NULL) {
       cgltf_accessor_read_float(texcoord, i, uv, 2);
     }

     if (color != NULL) {
       cgltf_accessor_read_float(color, i, c, 4);
     }

     vertices[i] =
         AP_3DMakeVertex(AP_V3(p[0], p[1], p[2]), AP_V3(n[0], n[1], n[2]),
                         AP_V2(uv[0], uv[1]), AP_C4(c[0], c[1], c[2], c[3]));
   }

   if (primitive->indices != NULL) {
     const cgltf_accessor *accessor = primitive->indices;

     index_count = accessor->count;

     if (index_count > INT_MAX) {
       free(vertices);
       AP_SET_ERROR(AP_ERROR_OPERATION_FAILED, "Index count is too large.");
       return false;
     }

     indices = malloc((size_t)index_count * sizeof(AP_U32));
     if (indices == NULL) {
       free(vertices);
       AP_SET_ERROR(AP_ERROR_OUT_OF_MEMORY, "Failed to allocate glTF indices.");
       return false;
     }

     for (i = 0; i < index_count; ++i) {
       indices[i] = (AP_U32)cgltf_accessor_read_index(accessor, i);
     }
   } else {
     index_count = 0;
   }

   *out_vertices = vertices;
   *out_vertex_count = (int)vertex_count;
   *out_indices = indices;
   *out_index_count = (int)index_count;
   return true;
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

 /* =========================================================
  * Material helpers
  * ========================================================= */

 static AP_Color AP_3DColorFromFloats(const float *f, int n) {
   if (f == NULL) {
     return AP_C4(1.0f, 1.0f, 1.0f, 1.0f);
   }
   if (n >= 4) {
     return AP_C4(f[0], f[1], f[2], f[3]);
   }
   if (n == 3) {
     return AP_C4(f[0], f[1], f[2], 1.0f);
   }
   return AP_C4(1.0f, 1.0f, 1.0f, 1.0f);
 }

 /*
  * Approximate PBR metallic-roughness with the existing Blinn-Phong path.
  * Higher metallic → lower diffuse contribution handled by albedo darkening
  * is left to the artist; we mainly map roughness → shininess/specular.
  */
 static void AP_3DMaterialToShading(const AP_Material *mat, AP_F32 *out_shininess,
                                    AP_F32 *out_specular) {
   AP_F32 roughness = 1.0f;
   AP_F32 metallic = 0.0f;

   if (mat != NULL) {
     if (mat->type == AP_MATERIAL_TYPE_PBR_METALLIC_ROUGHNESS) {
       roughness = AP_Clampf(mat->roughness, 0.04f, 1.0f);
       metallic = AP_Clampf(mat->metallic, 0.0f, 1.0f);
     } else if (mat->type == AP_MATERIAL_TYPE_PBR_SPECULAR_GLOSSINESS) {
       roughness = 1.0f - AP_Clampf(mat->glossiness_factor, 0.0f, 1.0f);
       metallic = 0.0f;
     } else if (mat->type == AP_MATERIAL_TYPE_UNLIT) {
       *out_shininess = 1.0f;
       *out_specular = 0.0f;
       return;
     }
   }

   /* shininess: map roughness 0→high, 1→low */
   *out_shininess = AP_Maxf(1.0f, (1.0f - roughness) * (1.0f - roughness) * 256.0f);
   /* specular strength rises with metallic and falls with roughness */
   *out_specular = AP_Clampf(0.04f + metallic * 0.96f, 0.0f, 1.0f) *
                   (1.0f - roughness * 0.5f);
 }

 static void AP_3DApplyMaterialRaster(const AP_Material *mat) {
   bool double_sided = false;
   bool needs_blend = false;

   if (mat != NULL) {
     double_sided = mat->double_sided;
     needs_blend = AP_MaterialNeedsAlphaBlend(mat);
   }

   if (double_sided || !g_3d.cull) {
     glDisable(GL_CULL_FACE);
   } else {
     glEnable(GL_CULL_FACE);
     glCullFace(GL_BACK);
     glFrontFace(GL_CCW);
   }

   if (needs_blend) {
     glEnable(GL_BLEND);
     glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE,
                         GL_ONE_MINUS_SRC_ALPHA);
     /* Transparent objects should not write depth in a simple forward path */
     glDepthMask(GL_FALSE);
   } else {
     glDisable(GL_BLEND);
     if (g_3d.depth_test) {
       glDepthMask(GL_TRUE);
     }
   }
 }

 static const AP_Material *AP_3DResolveMaterial(const AP_Mesh *mesh) {
   if (g_3d.material != NULL) {
     return g_3d.material;
   }
   if (mesh != NULL && mesh->material != NULL) {
     return mesh->material;
   }
   return NULL;
 }

 /* =========================================================
  * Lights (unchanged helpers)
  * ========================================================= */

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
   light.outer_cone =
       AP_Clampf(light.outer_cone, light.inner_cone + 0.01f, 90.0f);
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

   AP_ShaderSetUniformVec3(
       g_3d.shader, "u_ambient_color",
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
     AP_ShaderSetUniformVec3(
         g_3d.shader, name,
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

 /* =========================================================
  * Init / depth / raster
  * ========================================================= */

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
   g_3d.material = NULL;
   g_3d.depth_test = true;
   g_3d.cull = true;
   AP_3DPrepareLights(false);
   g_3d.initialized = true;

   AP_INFO("3D initialized");
   return true;
 }

 static void AP_3DEnsureDepthBuffer(void) {
   GLint fbo = 0;
   GLint attached = GL_NONE;
   GLint viewport[4];
   int width;
   int height;

   glGetIntegerv(GL_FRAMEBUFFER_BINDING, &fbo);
   glGetIntegerv(GL_VIEWPORT, viewport);
   width = viewport[2];
   height = viewport[3];
   if (width <= 0 || height <= 0) {
     return;
   }

   glGetFramebufferAttachmentParameteriv(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
                                         GL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE,
                                         &attached);

   if (attached != GL_NONE && (GLuint)fbo != g_3d.depth_fbo) {
     return;
   }

   if (g_3d.depth_rbo == 0) {
     glGenRenderbuffers(1, &g_3d.depth_rbo);
   }

   if (g_3d.depth_w != width || g_3d.depth_h != height) {
     glBindRenderbuffer(GL_RENDERBUFFER, g_3d.depth_rbo);
     glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, width, height);
     glBindRenderbuffer(GL_RENDERBUFFER, 0);
     g_3d.depth_w = width;
     g_3d.depth_h = height;
   }

   if (fbo != 0) {
     glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT,
                               GL_RENDERBUFFER, g_3d.depth_rbo);
     g_3d.depth_fbo = (GLuint)fbo;
   }
 }

 static AP_F32 AP_3DAspect(void) {
   AP_Rect viewport;
   int width;
   int height;

   if (AP_GetRenderViewport(&viewport) && viewport.w > 0 && viewport.h > 0) {
     return (AP_F32)viewport.w / (AP_F32)viewport.h;
   }

   width = AP_GetWindowPixelWidth();
   height = AP_GetWindowPixelHeight();
   if (width <= 0 || height <= 0) {
     return 16.0f / 9.0f;
   }
   return (AP_F32)width / (AP_F32)height;
 }

 static void AP_3DApplyRasterState(void) {
   glDisable(GL_SCISSOR_TEST);
   glDisable(GL_BLEND);
   glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);

   if (g_3d.depth_test) {
     glEnable(GL_DEPTH_TEST);
     glDepthFunc(GL_LESS);
     glDepthMask(GL_TRUE);
   } else {
     glDisable(GL_DEPTH_TEST);
     glDepthMask(GL_FALSE);
   }

   if (g_3d.cull) {
     glEnable(GL_CULL_FACE);
     glCullFace(GL_BACK);
     glFrontFace(GL_CCW);
   } else {
     glDisable(GL_CULL_FACE);
   }
 }

 static bool AP_3DSubmit(const AP_Mesh *mesh, const AP_Mat4 *model,
                         AP_Color tint, bool lit, GLenum mode) {
   AP_Mat4 used_model = model != NULL ? *model : g_3d.model;
   AP_Mat4 mvp = AP_Mat4Mul(g_3d.proj, AP_Mat4Mul(g_3d.view, used_model));
   AP_Mat3 normal = AP_Mat4NormalMatrix(used_model);
   const AP_Material *mat;
   AP_Color final_tint;
   GLuint tex_id;
   AP_F32 shininess;
   AP_F32 specular;
   int alpha_mode = 0;
   AP_F32 alpha_cutoff = 0.5f;
   bool unlit = !lit;

   if (!g_3d.active || mesh == NULL || mesh->vao == 0) {
     AP_SET_ERROR(AP_ERROR_INVALID_STATE, "3D pass is not active");
     return false;
   }

   mat = AP_3DResolveMaterial(mesh);

   /* Start from caller tint, then multiply by material base color if present */
   final_tint = tint;
   tex_id = g_3d.texture != 0 ? g_3d.texture : g_3d.white_texture;
   shininess = g_3d.shininess;
   specular = g_3d.specular;

   if (mat != NULL) {
     final_tint.r *= mat->base_color.r;
     final_tint.g *= mat->base_color.g;
     final_tint.b *= mat->base_color.b;
     final_tint.a *= mat->base_color.a;

     if (mat->has_base_color_texture && mat->base_color_texture != 0) {
       tex_id = (GLuint)mat->base_color_texture;
     }

     if (mat->type == AP_MATERIAL_TYPE_UNLIT) {
       unlit = true;
     }

     AP_3DMaterialToShading(mat, &shininess, &specular);

     switch (mat->alpha_mode) {
     case AP_ALPHA_MODE_MASK:
       alpha_mode = 1;
       alpha_cutoff = mat->alpha_cutoff;
       break;
     case AP_ALPHA_MODE_BLEND:
       alpha_mode = 2;
       break;
     default:
       alpha_mode = 0;
       break;
     }
   }

   AP_3DApplyRasterState();
   AP_3DApplyMaterialRaster(mat);

   AP_ShaderBind(g_3d.shader);
   AP_ShaderSetUniformMat4(g_3d.shader, "u_mvp", mvp.m, false);
   AP_ShaderSetUniformMat4(g_3d.shader, "u_model", used_model.m, false);
   AP_ShaderSetUniformMat3(g_3d.shader, "u_normal_matrix", normal.m, false);

   /* Temporarily override global shading for this draw */
   {
     AP_F32 saved_shininess = g_3d.shininess;
     AP_F32 saved_specular = g_3d.specular;
     g_3d.shininess = shininess;
     g_3d.specular = specular;
     AP_3DUploadLights();
     g_3d.shininess = saved_shininess;
     g_3d.specular = saved_specular;
   }

   AP_ShaderSetUniformColor(g_3d.shader, "u_tint", final_tint);
   AP_ShaderSetUniformF(g_3d.shader, "u_lit", unlit ? 0.0f : 1.0f);
   AP_ShaderSetUniformF(g_3d.shader, "u_alpha_cutoff", alpha_cutoff);
   AP_ShaderSetUniformI(g_3d.shader, "u_alpha_mode", alpha_mode);

   glActiveTexture(GL_TEXTURE0);
   glBindTexture(GL_TEXTURE_2D, tex_id);
   AP_ShaderSetUniformI(g_3d.shader, "u_texture", 0);

   glBindVertexArray(mesh->vao);
   if (mesh->index_count > 0) {
     glDrawElements(mode, mesh->index_count, GL_UNSIGNED_INT, NULL);
   } else {
     glDrawArrays(mode, 0, mesh->vertex_count);
   }
   glBindVertexArray(0);

   /* Restore default raster after possible transparent draw */
   if (mat != NULL && AP_MaterialNeedsAlphaBlend(mat)) {
     glDepthMask(GL_TRUE);
     glDisable(GL_BLEND);
   }

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

   AP_3DApplyRasterState();

   mvp = AP_Mat4Mul(g_3d.proj, AP_Mat4Mul(g_3d.view, g_3d.model));
   normal = AP_Mat4NormalMatrix(g_3d.model);

   AP_ShaderBind(g_3d.shader);
   AP_ShaderSetUniformMat4(g_3d.shader, "u_mvp", mvp.m, false);
   AP_ShaderSetUniformMat4(g_3d.shader, "u_model", g_3d.model.m, false);
   AP_ShaderSetUniformMat3(g_3d.shader, "u_normal_matrix", normal.m, false);
   AP_ShaderSetUniformColor(g_3d.shader, "u_tint", tint);
   AP_ShaderSetUniformF(g_3d.shader, "u_lit", 0.0f);
   AP_ShaderSetUniformF(g_3d.shader, "u_alpha_cutoff", 0.5f);
   AP_ShaderSetUniformI(g_3d.shader, "u_alpha_mode", 0);
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
  * Mesh
  * ========================================================= */

 AP_Mesh *AP_CreateMesh(const AP_Vertex3 *vertices, int vertex_count,
                        const AP_U32 *indices, int index_count) {
   if (!AP_3DEnsure()) {
     return NULL;
   }
   return AP_3DUploadMesh(vertices, vertex_count, indices, index_count);
 }

 AP_Mesh *AP_LoadMesh(const char *path) {
   cgltf_options options = {0};
   cgltf_data *data = NULL;
   cgltf_result result;
   cgltf_primitive *primitive;
   AP_Vertex3 *vertices = NULL;
   AP_U32 *indices = NULL;
   int vertex_count = 0;
   int index_count = 0;
   AP_Mesh *mesh = NULL;

   if (path == NULL) {
     AP_SET_ERROR(AP_ERROR_INVALID_ARGUMENT, "Path is NULL");
     return NULL;
   }

   if (!AP_3DEnsure()) {
     return NULL;
   }

   result = cgltf_parse_file(&options, path, &data);

   if (result != cgltf_result_success || data == NULL) {
     AP_SET_ERROR(AP_ERROR_OPERATION_FAILED, "Failed to parse mesh file");
     return NULL;
   }

   /*
    * cgltf_parse_file only parses the JSON/GLB structure — it does not
    * resolve the actual binary buffer data (external .bin files or
    * base64 blobs). AP_3DReadPrimitive below reads accessor data
    * directly, so the buffers must be loaded first or every accessor
    * read silently comes back empty/zeroed, producing a mesh with no
    * real geometry.
    */
   result = cgltf_load_buffers(&options, data, path);
   if (result != cgltf_result_success) {
     AP_SET_ERROR(AP_ERROR_OPERATION_FAILED, "Failed to load mesh buffers");
     cgltf_free(data);
     return NULL;
   }

   if (data->meshes_count == 0) {
     AP_SET_ERROR(AP_ERROR_OPERATION_FAILED, "Mesh file is invalid");
     cgltf_free(data);
     return NULL;
   }

   primitive = NULL;

   for (cgltf_size m = 0; m < data->meshes_count && primitive == NULL; ++m) {
     cgltf_mesh *gltf_mesh = &data->meshes[m];
     for (cgltf_size p = 0; p < gltf_mesh->primitives_count; ++p) {
       if (gltf_mesh->primitives[p].type == cgltf_primitive_type_triangles) {
         primitive = &gltf_mesh->primitives[p];
         break;
       }
     }
   }

   if (primitive == NULL) {
     AP_SET_ERROR(AP_ERROR_OPERATION_FAILED, "Mesh file is invalid");
     cgltf_free(data);
     return NULL;
   }

   if (!AP_3DReadPrimitive(primitive, &vertices, &vertex_count, &indices,
                           &index_count)) {
     cgltf_free(data);
     return NULL;
   }

   mesh = AP_3DUploadMesh(vertices, vertex_count, indices, index_count);

   free(vertices);
   free(indices);
   cgltf_free(data);
   return mesh;
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

   vertices[0] =
       AP_3DMakeVertex(AP_V3(-hx, 0.0f, -hz), n, AP_V2(0.0f, 0.0f), white);
   vertices[1] =
       AP_3DMakeVertex(AP_V3(hx, 0.0f, -hz), n, AP_V2(1.0f, 0.0f), white);
   vertices[2] =
       AP_3DMakeVertex(AP_V3(hx, 0.0f, hz), n, AP_V2(1.0f, 1.0f), white);
   vertices[3] =
       AP_3DMakeVertex(AP_V3(-hx, 0.0f, hz), n, AP_V2(0.0f, 1.0f), white);

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
       AP_Vec3 n =
           AP_Vec3Normalize(AP_V3(ring * cosf(theta), y, ring * sinf(theta)));
       vertices[v++] = AP_3DMakeVertex(AP_Vec3Scale(n, radius), n,
                                       AP_V2(u_coord, 1.0f - v_coord), white);
     }
   }

   for (stack = 0; stack < stacks; ++stack) {
     for (slice = 0; slice < slices; ++slice) {
       AP_U32 a = (AP_U32)(stack * (slices + 1) + slice);
       AP_U32 b = a + (AP_U32)(slices + 1);
       indices[i++] = a;
       indices[i++] = a + 1;
       indices[i++] = b;
       indices[i++] = a + 1;
       indices[i++] = b + 1;
       indices[i++] = b;
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

   if (mesh->material_owned && mesh->material != NULL) {
     AP_DestroyMaterial(mesh->material);
     mesh->material = NULL;
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

 bool AP_MeshSetMaterial(AP_Mesh *mesh, AP_Material *material) {
   if (mesh == NULL) {
     AP_SET_ERROR(AP_ERROR_INVALID_ARGUMENT, "Mesh is NULL");
     return false;
   }

   if (mesh->material_owned && mesh->material != NULL &&
       mesh->material != material) {
     AP_DestroyMaterial(mesh->material);
   }

   mesh->material = material;
   mesh->material_owned = false; /* caller retains ownership */
   return true;
 }

 AP_Material *AP_MeshGetMaterial(const AP_Mesh *mesh) {
   return mesh != NULL ? mesh->material : NULL;
 }

 /* =========================================================
  * glTF material / texture loading
  * ========================================================= */

 static AP_Texture *AP_3DLoadGltfTexture(const cgltf_texture *tex,
                                         const cgltf_data *data,
                                         const char *base_path) {
   const cgltf_image *image;
   AP_Texture *out = NULL;

   if (tex == NULL || tex->image == NULL) {
     return NULL;
   }

   image = tex->image;

   if (image->buffer_view != NULL && image->buffer_view->buffer != NULL &&
       image->buffer_view->buffer->data != NULL) {
     const uint8_t *bytes =
         (const uint8_t *)image->buffer_view->buffer->data +
         image->buffer_view->offset;
     int size = (int)image->buffer_view->size;
     out = AP_LoadTextureFromMemory(bytes, size);
   } else if (image->uri != NULL) {
     char path[1024];
     const char *uri = image->uri;

     /* Skip data: URIs for simplicity; buffer_view path covers embedded */
     if (strncmp(uri, "data:", 5) == 0) {
       return NULL;
     }

     if (base_path != NULL && base_path[0] != '\0') {
       const char *slash = strrchr(base_path, '/');
       const char *bslash = strrchr(base_path, '\\');
       const char *sep = slash;
       if (bslash != NULL && (sep == NULL || bslash > sep)) {
         sep = bslash;
       }
       if (sep != NULL) {
         size_t dir_len = (size_t)(sep - base_path + 1);
         if (dir_len + strlen(uri) + 1 < sizeof(path)) {
           memcpy(path, base_path, dir_len);
           path[dir_len] = '\0';
           strncat(path, uri, sizeof(path) - dir_len - 1);
           out = AP_LoadTexture(path);
         }
       } else {
         out = AP_LoadTexture(uri);
       }
     } else {
       out = AP_LoadTexture(uri);
     }
   }

   (void)data;
   return out;
 }

 static void AP_3DFillMaterialFromGltf(AP_Material *mat,
                                       const cgltf_material *src,
                                       AP_Texture **textures, int *texture_count,
                                       int texture_capacity,
                                       const cgltf_data *data,
                                       const char *base_path) {
   if (mat == NULL || src == NULL) {
     return;
   }

   if (src->name != NULL) {
     /* name already set by AP_MaterialCreate; leave as-is or replace */
   }

   mat->double_sided = src->double_sided != 0;
   mat->alpha_cutoff = src->alpha_cutoff;

   switch (src->alpha_mode) {
   case cgltf_alpha_mode_mask:
     mat->alpha_mode = AP_ALPHA_MODE_MASK;
     break;
   case cgltf_alpha_mode_blend:
     mat->alpha_mode = AP_ALPHA_MODE_BLEND;
     break;
   default:
     mat->alpha_mode = AP_ALPHA_MODE_OPAQUE;
     break;
   }

   if (src->has_pbr_metallic_roughness) {
     const cgltf_pbr_metallic_roughness *pbr = &src->pbr_metallic_roughness;
     mat->type = AP_MATERIAL_TYPE_PBR_METALLIC_ROUGHNESS;
     mat->base_color = AP_3DColorFromFloats(pbr->base_color_factor, 4);
     mat->metallic = pbr->metallic_factor;
     mat->roughness = pbr->roughness_factor;

     if (pbr->base_color_texture.texture != NULL &&
         *texture_count < texture_capacity) {
       AP_Texture *t = AP_3DLoadGltfTexture(pbr->base_color_texture.texture,
                                            data, base_path);
       if (t != NULL) {
         textures[*texture_count] = t;
         (*texture_count)++;
         AP_MaterialSetBaseColorTexture(mat, AP_TextureNativeId(t));
       }
     }

     if (pbr->metallic_roughness_texture.texture != NULL &&
         *texture_count < texture_capacity) {
       AP_Texture *t =
           AP_3DLoadGltfTexture(pbr->metallic_roughness_texture.texture, data,
                                base_path);
       if (t != NULL) {
         textures[*texture_count] = t;
         (*texture_count)++;
         AP_MaterialSetMetallicRoughnessTexture(mat, AP_TextureNativeId(t));
       }
     }
   } else if (src->has_pbr_specular_glossiness) {
     const cgltf_pbr_specular_glossiness *sg = &src->pbr_specular_glossiness;
     mat->type = AP_MATERIAL_TYPE_PBR_SPECULAR_GLOSSINESS;
     mat->base_color = AP_3DColorFromFloats(sg->diffuse_factor, 4);
     mat->specular_factor = AP_3DColorFromFloats(sg->specular_factor, 3);
     mat->glossiness_factor = sg->glossiness_factor;

     if (sg->diffuse_texture.texture != NULL &&
         *texture_count < texture_capacity) {
       AP_Texture *t =
           AP_3DLoadGltfTexture(sg->diffuse_texture.texture, data, base_path);
       if (t != NULL) {
         textures[*texture_count] = t;
         (*texture_count)++;
         AP_MaterialSetBaseColorTexture(mat, AP_TextureNativeId(t));
       }
     }
     if (sg->specular_glossiness_texture.texture != NULL &&
         *texture_count < texture_capacity) {
       AP_Texture *t = AP_3DLoadGltfTexture(
           sg->specular_glossiness_texture.texture, data, base_path);
       if (t != NULL) {
         textures[*texture_count] = t;
         (*texture_count)++;
         AP_MaterialSetSpecularGlossinessTexture(mat, AP_TextureNativeId(t));
       }
     }
   } else if (src->unlit) {
     mat->type = AP_MATERIAL_TYPE_UNLIT;
     mat->base_color = AP_C4(1.0f, 1.0f, 1.0f, 1.0f);
   }

   if (src->normal_texture.texture != NULL &&
       *texture_count < texture_capacity) {
     AP_Texture *t =
         AP_3DLoadGltfTexture(src->normal_texture.texture, data, base_path);
     if (t != NULL) {
       textures[*texture_count] = t;
       (*texture_count)++;
       AP_MaterialSetNormalTexture(mat, AP_TextureNativeId(t),
                                   src->normal_texture.scale);
     }
   }

   if (src->occlusion_texture.texture != NULL &&
       *texture_count < texture_capacity) {
     AP_Texture *t =
         AP_3DLoadGltfTexture(src->occlusion_texture.texture, data, base_path);
     if (t != NULL) {
       textures[*texture_count] = t;
       (*texture_count)++;
       AP_MaterialSetOcclusionTexture(mat, AP_TextureNativeId(t),
                                      src->occlusion_texture.scale);
     }
   }

   if (src->emissive_texture.texture != NULL &&
       *texture_count < texture_capacity) {
     AP_Texture *t =
         AP_3DLoadGltfTexture(src->emissive_texture.texture, data, base_path);
     if (t != NULL) {
       textures[*texture_count] = t;
       (*texture_count)++;
       AP_MaterialSetEmissiveTexture(mat, AP_TextureNativeId(t));
     }
   }

   mat->emissive_factor = AP_3DColorFromFloats(src->emissive_factor, 3);
 }

 /* =========================================================
  * Model
  * ========================================================= */

 AP_Model *AP_CreateModel(void) {
   AP_Model *model;

   if (!AP_3DEnsure()) {
     return NULL;
   }

   model = (AP_Model *)calloc(1, sizeof(AP_Model));
   if (model == NULL) {
     AP_SET_ERROR(AP_ERROR_OUT_OF_MEMORY, "Failed to allocate model");
     return NULL;
   }

   model->transform = AP_Mat4Identity();
   return model;
 }

 AP_Model *AP_LoadModel(const char *path) {
   cgltf_options options = {0};
   cgltf_data *data = NULL;
   cgltf_result result;
   AP_Model *model = NULL;
   int mesh_count = 0;
   int mesh_index = 0;
   int mat_count = 0;
   int tex_capacity = 0;
   cgltf_size m;
   cgltf_size p;
   int i;

   if (path == NULL) {
     AP_SET_ERROR(AP_ERROR_INVALID_ARGUMENT, "Path is NULL");
     return NULL;
   }

   if (!AP_3DEnsure()) {
     return NULL;
   }

   result = cgltf_parse_file(&options, path, &data);
   if (result != cgltf_result_success || data == NULL) {
     AP_SET_ERROR(AP_ERROR_OPERATION_FAILED, "Failed to parse model file");
     return NULL;
   }

   result = cgltf_load_buffers(&options, data, path);
   if (result != cgltf_result_success) {
     AP_SET_ERROR(AP_ERROR_OPERATION_FAILED, "Failed to load model buffers");
     cgltf_free(data);
     return NULL;
   }

   for (m = 0; m < data->meshes_count; ++m) {
     cgltf_mesh *gltf_mesh = &data->meshes[m];
     for (p = 0; p < gltf_mesh->primitives_count; ++p) {
       if (gltf_mesh->primitives[p].type == cgltf_primitive_type_triangles) {
         mesh_count++;
       }
     }
   }

   if (mesh_count == 0) {
     AP_SET_ERROR(AP_ERROR_OPERATION_FAILED, "Model has no triangle meshes");
     cgltf_free(data);
     return NULL;
   }

   model = AP_CreateModel();
   if (model == NULL) {
     cgltf_free(data);
     return NULL;
   }

   model->meshes = (AP_Mesh **)calloc((size_t)mesh_count, sizeof(AP_Mesh *));
   model->local_transforms =
       (AP_Mat4 *)calloc((size_t)mesh_count, sizeof(AP_Mat4));
   if (model->meshes == NULL || model->local_transforms == NULL) {
     free(model->meshes);
     free(model->local_transforms);
     free(model);
     cgltf_free(data);
     AP_SET_ERROR(AP_ERROR_OUT_OF_MEMORY, "Failed to allocate model meshes");
     return NULL;
   }

   model->mesh_count = mesh_count;

   /* Materials */
   mat_count = (int)data->materials_count;
   if (mat_count > 0) {
     model->materials =
         (AP_Material *)calloc((size_t)mat_count, sizeof(AP_Material));
     if (model->materials == NULL) {
       AP_DestroyModel(model);
       cgltf_free(data);
       AP_SET_ERROR(AP_ERROR_OUT_OF_MEMORY, "Failed to allocate materials");
       return NULL;
     }
     model->material_count = mat_count;

     /* Worst-case textures: several per material */
     tex_capacity = mat_count * 6 + 8;
     model->textures =
         (AP_Texture **)calloc((size_t)tex_capacity, sizeof(AP_Texture *));
     if (model->textures == NULL) {
       AP_DestroyModel(model);
       cgltf_free(data);
       AP_SET_ERROR(AP_ERROR_OUT_OF_MEMORY, "Failed to allocate texture list");
       return NULL;
     }

     for (i = 0; i < mat_count; ++i) {
       const cgltf_material *src = &data->materials[i];
       AP_Material *dst = &model->materials[i];
       const char *name = src->name != NULL ? src->name : "material";

       /* Placement: zero then init as if created */
       memset(dst, 0, sizeof(*dst));
       {
         AP_Material *tmp = AP_CreateMaterial(name, AP_MATERIAL_TYPE_PBR_METALLIC_ROUGHNESS);
         if (tmp != NULL) {
           *dst = *tmp;
           /* Steal name pointer; destroy shell without freeing name */
           tmp->name = NULL;
           AP_DestroyMaterial(tmp);
         }
       }

       AP_3DFillMaterialFromGltf(dst, src, model->textures, &model->texture_count,
                                 tex_capacity, data, path);
     }
   }

   for (m = 0; m < data->meshes_count; ++m) {
     cgltf_mesh *gltf_mesh = &data->meshes[m];
     for (p = 0; p < gltf_mesh->primitives_count; ++p) {
       cgltf_primitive *prim = &gltf_mesh->primitives[p];
       AP_Vertex3 *vertices = NULL;
       AP_U32 *indices = NULL;
       int vertex_count = 0;
       int index_count = 0;

       if (prim->type != cgltf_primitive_type_triangles) {
         continue;
       }

       if (!AP_3DReadPrimitive(prim, &vertices, &vertex_count, &indices,
                               &index_count)) {
         free(vertices);
         free(indices);
         AP_DestroyModel(model);
         cgltf_free(data);
         return NULL;
       }

       model->meshes[mesh_index] =
           AP_3DUploadMesh(vertices, vertex_count, indices, index_count);
       model->local_transforms[mesh_index] = AP_Mat4Identity();
       free(vertices);
       free(indices);

       if (model->meshes[mesh_index] == NULL) {
         AP_DestroyModel(model);
         cgltf_free(data);
         return NULL;
       }

       /* Bind material by index if present */
       if (prim->material != NULL && model->materials != NULL) {
         cgltf_size mi = (cgltf_size)(prim->material - data->materials);
         if (mi < (cgltf_size)model->material_count) {
           model->meshes[mesh_index]->material = &model->materials[mi];
           model->meshes[mesh_index]->material_owned = false;
         }
       }

       mesh_index++;
     }
   }

   if (data->nodes_count > 0) {
     cgltf_node *node = &data->nodes[0];
     AP_Vec3 t = AP_V3(0.0f, 0.0f, 0.0f);
     AP_Quat r = AP_QuatIdentity();
     AP_Vec3 s = AP_V3(1.0f, 1.0f, 1.0f);

     if (node->has_translation) {
       t = AP_V3(node->translation[0], node->translation[1],
                 node->translation[2]);
     }
     if (node->has_rotation) {
       r = AP_Q4(node->rotation[0], node->rotation[1], node->rotation[2],
                 node->rotation[3]);
     }
     if (node->has_scale) {
       s = AP_V3(node->scale[0], node->scale[1], node->scale[2]);
     }
     if (node->has_matrix) {
       memcpy(model->transform.m, node->matrix, sizeof(float) * 16);
     } else {
       model->transform = AP_Mat4TRS(t, r, s);
     }
   }

   cgltf_free(data);
   return model;
 }

 void AP_DestroyModel(AP_Model *model) {
   int i;

   if (model == NULL) {
     return;
   }

   if (model->meshes != NULL) {
     for (i = 0; i < model->mesh_count; ++i) {
       /* Materials live in model->materials; clear pointer so DestroyMesh
          does not free them */
       if (model->meshes[i] != NULL) {
         model->meshes[i]->material = NULL;
         model->meshes[i]->material_owned = false;
         AP_DestroyMesh(model->meshes[i]);
       }
     }
     free(model->meshes);
   }

   free(model->local_transforms);

   if (model->materials != NULL) {
     for (i = 0; i < model->material_count; ++i) {
       if (model->materials[i].name != NULL) {
         free((void *)model->materials[i].name);
         model->materials[i].name = NULL;
       }
     }
     free(model->materials);
   }

   if (model->textures != NULL) {
     for (i = 0; i < model->texture_count; ++i) {
       AP_DestroyTexture(model->textures[i]);
     }
     free(model->textures);
   }

   free(model);
 }

 bool AP_ModelIsValid(const AP_Model *model) {
   return model != NULL && model->mesh_count > 0 && model->meshes != NULL;
 }

 int AP_ModelMeshCount(const AP_Model *model) {
   return model != NULL ? model->mesh_count : 0;
 }

 AP_Mesh *AP_ModelGetMesh(const AP_Model *model, int index) {
   if (model == NULL || index < 0 || index >= model->mesh_count) {
     return NULL;
   }
   return model->meshes[index];
 }

 int AP_ModelMaterialCount(const AP_Model *model) {
   return model != NULL ? model->material_count : 0;
 }

 AP_Material *AP_ModelGetMaterial(const AP_Model *model, int index) {
   if (model == NULL || index < 0 || index >= model->material_count) {
     return NULL;
   }
   return &model->materials[index];
 }

 /* ---- Root transform ---- */

 bool AP_ModelSetTransform(AP_Model *model, const AP_Mat4 *transform) {
   if (model == NULL || transform == NULL) {
     AP_SET_ERROR(AP_ERROR_INVALID_ARGUMENT, "Model transform is invalid");
     return false;
   }
   model->transform = *transform;
   return true;
 }

 AP_Mat4 AP_ModelGetTransform(const AP_Model *model) {
   if (model == NULL) {
     return AP_Mat4Identity();
   }
   return model->transform;
 }

 bool AP_ModelSetTRS(AP_Model *model, AP_Vec3 position, AP_Quat rotation,
                     AP_Vec3 scale) {
   if (model == NULL) {
     AP_SET_ERROR(AP_ERROR_INVALID_ARGUMENT, "Model is NULL");
     return false;
   }
   model->transform = AP_Mat4TRS(position, rotation, scale);
   return true;
 }

 bool AP_ModelSetPosition(AP_Model *model, AP_Vec3 position) {
   if (model == NULL) {
     AP_SET_ERROR(AP_ERROR_INVALID_ARGUMENT, "Model is NULL");
     return false;
   }
   model->transform.m[12] = position.x;
   model->transform.m[13] = position.y;
   model->transform.m[14] = position.z;
   return true;
 }

 bool AP_ModelTranslate(AP_Model *model, AP_Vec3 delta) {
   if (model == NULL) {
     AP_SET_ERROR(AP_ERROR_INVALID_ARGUMENT, "Model is NULL");
     return false;
   }
   model->transform = AP_Mat4Mul(AP_Mat4Translate(delta), model->transform);
   return true;
 }

 bool AP_ModelRotate(AP_Model *model, AP_Vec3 axis, AP_F32 degrees) {
   if (model == NULL) {
     AP_SET_ERROR(AP_ERROR_INVALID_ARGUMENT, "Model is NULL");
     return false;
   }
   model->transform =
       AP_Mat4Mul(AP_Mat4RotateAxis(axis, degrees), model->transform);
   return true;
 }

 bool AP_ModelScale(AP_Model *model, AP_Vec3 scale) {
   if (model == NULL) {
     AP_SET_ERROR(AP_ERROR_INVALID_ARGUMENT, "Model is NULL");
     return false;
   }
   model->transform = AP_Mat4Mul(AP_Mat4Scale(scale), model->transform);
   return true;
 }

 bool AP_ModelResetTransform(AP_Model *model) {
   if (model == NULL) {
     AP_SET_ERROR(AP_ERROR_INVALID_ARGUMENT, "Model is NULL");
     return false;
   }
   model->transform = AP_Mat4Identity();
   return true;
 }

 bool AP_ModelSetMeshTransform(AP_Model *model, int mesh_index,
                               const AP_Mat4 *local) {
   if (model == NULL || local == NULL || mesh_index < 0 ||
       mesh_index >= model->mesh_count) {
     AP_SET_ERROR(AP_ERROR_INVALID_ARGUMENT, "Mesh transform is invalid");
     return false;
   }
   model->local_transforms[mesh_index] = *local;
   return true;
 }

 AP_Mat4 AP_ModelGetMeshTransform(const AP_Model *model, int mesh_index) {
   if (model == NULL || mesh_index < 0 || mesh_index >= model->mesh_count) {
     return AP_Mat4Identity();
   }
   return model->local_transforms[mesh_index];
 }

 bool AP_ModelSetMeshTRS(AP_Model *model, int mesh_index, AP_Vec3 position,
                         AP_Quat rotation, AP_Vec3 scale) {
   if (model == NULL || mesh_index < 0 || mesh_index >= model->mesh_count) {
     AP_SET_ERROR(AP_ERROR_INVALID_ARGUMENT, "Mesh TRS is invalid");
     return false;
   }
   model->local_transforms[mesh_index] = AP_Mat4TRS(position, rotation, scale);
   return true;
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
   g_3d.material = NULL;
   g_3d.active = true;

   AP_3DEnsureDepthBuffer();
   AP_3DApplyRasterState();
   glDepthMask(GL_TRUE);
   glClearDepth(1.0);
   glClear(GL_DEPTH_BUFFER_BIT);
   AP_ShaderBind(g_3d.shader);
   return true;
 }

 void AP_End3D(void) {
   if (!g_3d.active) {
     return;
   }

   g_3d.active = false;
   g_3d.material = NULL;
   glDisable(GL_DEPTH_TEST);
   glDepthMask(GL_TRUE);
   glDisable(GL_CULL_FACE);
   glEnable(GL_BLEND);
   glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE,
                       GL_ONE_MINUS_SRC_ALPHA);
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

 bool AP_Set3DTransform(AP_Vec3 position, AP_Quat rotation, AP_Vec3 scale) {
   g_3d.model = AP_Mat4TRS(position, rotation, scale);
   return true;
 }

 bool AP_Set3DPosition(AP_Vec3 position) {
   g_3d.model.m[12] = position.x;
   g_3d.model.m[13] = position.y;
   g_3d.model.m[14] = position.z;
   return true;
 }

 bool AP_Set3DRotation(AP_Quat rotation) {
   AP_Vec3 t = AP_Mat4GetTranslation(g_3d.model);
   g_3d.model = AP_Mat4TRS(t, rotation, AP_V3(1.0f, 1.0f, 1.0f));
   return true;
 }

 bool AP_Set3DScale(AP_Vec3 scale) {
   AP_Vec3 t = AP_Mat4GetTranslation(g_3d.model);
   AP_Quat r = AP_QuatFromMat4(g_3d.model);
   g_3d.model = AP_Mat4TRS(t, r, scale);
   return true;
 }

 bool AP_Translate3D(AP_Vec3 delta) {
   g_3d.model = AP_Mat4Mul(g_3d.model, AP_Mat4Translate(delta));
   return true;
 }

 bool AP_Rotate3D(AP_Vec3 axis, AP_F32 degrees) {
   g_3d.model = AP_Mat4Mul(g_3d.model, AP_Mat4RotateAxis(axis, degrees));
   return true;
 }

 bool AP_Scale3D(AP_Vec3 scale) {
   g_3d.model = AP_Mat4Mul(g_3d.model, AP_Mat4Scale(scale));
   return true;
 }

 bool AP_Set3DTexture(AP_Texture *texture) {
   AP_UInt id = AP_TextureNativeId(texture);
   g_3d.texture = id != 0 ? (GLuint)id : g_3d.white_texture;
   return true;
 }

 bool AP_Set3DTint(AP_Color tint) {
   g_3d.tint = tint;
   return true;
 }

 bool AP_Set3DMaterial(const AP_Material *material) {
   g_3d.material = material;
   return true;
 }

 const AP_Material *AP_Get3DMaterial(void) { return g_3d.material; }

 void AP_Clear3DMaterial(void) { g_3d.material = NULL; }

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
  * Lights (public API — same as before)
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

 bool AP_DrawMeshTRS(const AP_Mesh *mesh, AP_Vec3 position, AP_Quat rotation,
                     AP_Vec3 scale, AP_Color tint) {
   AP_Mat4 model = AP_Mat4TRS(position, rotation, scale);
   return AP_3DSubmit(mesh, &model, tint, true, GL_TRIANGLES);
 }

 bool AP_DrawModel(const AP_Model *model) {
   int i;
   AP_Mat4 world;

   if (model == NULL || model->mesh_count <= 0) {
     AP_SET_ERROR(AP_ERROR_INVALID_ARGUMENT, "Model is invalid");
     return false;
   }

   for (i = 0; i < model->mesh_count; ++i) {
     if (model->meshes[i] == NULL) {
       continue;
     }
     world = AP_Mat4Mul(model->transform, model->local_transforms[i]);
     if (!AP_3DSubmit(model->meshes[i], &world, g_3d.tint, true, GL_TRIANGLES)) {
       return false;
     }
   }
   return true;
 }

 bool AP_DrawModelEx(const AP_Model *model, const AP_Mat4 *world_override,
                     AP_Color tint) {
   int i;
   AP_Mat4 root;
   AP_Mat4 world;

   if (model == NULL || model->mesh_count <= 0) {
     AP_SET_ERROR(AP_ERROR_INVALID_ARGUMENT, "Model is invalid");
     return false;
   }

   root = world_override != NULL ? *world_override : model->transform;

   for (i = 0; i < model->mesh_count; ++i) {
     if (model->meshes[i] == NULL) {
       continue;
     }
     world = AP_Mat4Mul(root, model->local_transforms[i]);
     if (!AP_3DSubmit(model->meshes[i], &world, tint, true, GL_TRIANGLES)) {
       return false;
     }
   }
   return true;
 }

 bool AP_DrawModelTRS(const AP_Model *model, AP_Vec3 position, AP_Quat rotation,
                      AP_Vec3 scale, AP_Color tint) {
   AP_Mat4 world = AP_Mat4TRS(position, rotation, scale);
   return AP_DrawModelEx(model, &world, tint);
 }

 bool AP_DrawCube(AP_Vec3 center, AP_Vec3 size, AP_Color color) {
   AP_Mat4 model = AP_Mat4TRS(center, AP_QuatIdentity(), size);
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
     verts[count++] =
         AP_3DMakeVertex(AP_V3(-half, 0.0f, t), n, AP_V2(0.0f, 0.0f), color);
     verts[count++] =
         AP_3DMakeVertex(AP_V3(half, 0.0f, t), n, AP_V2(1.0f, 0.0f), color);
     verts[count++] =
         AP_3DMakeVertex(AP_V3(t, 0.0f, -half), n, AP_V2(0.0f, 0.0f), color);
     verts[count++] =
         AP_3DMakeVertex(AP_V3(t, 0.0f, half), n, AP_V2(1.0f, 0.0f), color);
   }

   return AP_3DSubmitLines(verts, count, AP_C4(1.0f, 1.0f, 1.0f, 1.0f));
 }

 void AP_3DShutdown(void) {
   g_3d.active = false;
   g_3d.material = NULL;

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
   if (g_3d.depth_rbo != 0) {
     glDeleteRenderbuffers(1, &g_3d.depth_rbo);
     g_3d.depth_rbo = 0;
   }
   if (g_3d.shader != NULL) {
     AP_ShaderDestroyInternal(g_3d.shader);
     g_3d.shader = NULL;
   }

   memset(&g_3d, 0, sizeof(g_3d));
 }