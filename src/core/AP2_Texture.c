/*
 * AP2 — Application Primitives
 * Copyright (c) 2024-2026 Jack Waechter
 *
 * Licensed under the MIT License.
 * See LICENSE in the project root for full terms.
 */

#include "AP2/AP2_Texture.h"

#include "AP2_Internal.h"

#include "AP2/AP2_Error.h"
#include "AP2/AP2_Image.h"
#include "AP2/AP2_Opengl.h"

#define GLFW_INCLUDE_NONE
#include <glad/gl.h>

#include <math.h>
#include <stdlib.h>
#include <string.h>

#define AP_TEXTURE_DEG2RAD (3.14159265358979323846f / 180.0f)

struct AP_Texture {
  GLuint id;
  GLuint fbo;
  int width;
  int height;
  AP_TextureAccess access;
  AP_BlendMode blend_mode;
  AP_ScaleMode scale_mode;
  AP_TextureAddressMode address_mode;
  AP_Color color_mod;
  bool invert_v;
  unsigned char *pixels;
  bool locked;
  AP_Rect lock_rect;
};

static AP_Texture *g_render_target = NULL;

static float AP_Clampf01(float value) {
  if (value < 0.0f) {
    return 0.0f;
  }
  if (value > 1.0f) {
    return 1.0f;
  }
  return value;
}

static AP_U8 AP_FloatToByte(float value) {
  return (AP_U8)(AP_Clampf01(value) * 255.0f + 0.5f);
}

static GLint AP_TextureWrapEnum(AP_TextureAddressMode mode) {
  if (mode == AP_TEXTUREADDRESS_WRAP) {
    return GL_REPEAT;
  }
  if (mode == AP_TEXTUREADDRESS_MIRROR) {
    return GL_MIRRORED_REPEAT;
  }
  return GL_CLAMP_TO_EDGE;
}

static void AP_TextureBindSampler(AP_Texture *texture) {
  GLint min_filter = GL_NEAREST;
  GLint mag_filter = GL_NEAREST;
  GLint wrap = AP_TextureWrapEnum(texture->address_mode);

  if (texture->scale_mode == AP_SCALEMODE_LINEAR) {
    min_filter = GL_LINEAR;
    mag_filter = GL_LINEAR;
  } else if (texture->scale_mode == AP_SCALEMODE_MIPMAP) {
    min_filter = GL_LINEAR_MIPMAP_LINEAR;
    mag_filter = GL_LINEAR;
  }

  glBindTexture(GL_TEXTURE_2D, texture->id);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, min_filter);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, mag_filter);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, wrap);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, wrap);
  glBindTexture(GL_TEXTURE_2D, 0);
}

static bool AP_TextureHasContext(void) {
  if (!AP_OpenGLHasContext()) {
    AP_SET_ERROR(AP_ERROR_NOT_INITIALIZED, "No active window for textures");
    return false;
  }

  return true;
}

static bool AP_TextureEnsureFbo(AP_Texture *texture) {
  if (texture->fbo != 0) {
    return true;
  }

  glGenFramebuffers(1, &texture->fbo);
  glBindFramebuffer(GL_FRAMEBUFFER, texture->fbo);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
                         texture->id, 0);
  if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    AP_SET_ERROR(AP_ERROR_OPERATION_FAILED, "Texture framebuffer is incomplete");
    return false;
  }

  glBindFramebuffer(GL_FRAMEBUFFER, 0);
  texture->invert_v = true;
  return true;
}

static AP_Texture *AP_TextureCreateEmpty(int width, int height,
                                         AP_TextureAccess access) {
  AP_Texture *texture;

  if (width <= 0 || height <= 0) {
    AP_SET_ERROR(AP_ERROR_INVALID_ARGUMENT, "Texture size must be positive");
    return NULL;
  }

  if (!AP_TextureHasContext()) {
    return NULL;
  }

  texture = (AP_Texture *)calloc(1, sizeof(AP_Texture));
  if (texture == NULL) {
    AP_SET_ERROR(AP_ERROR_OUT_OF_MEMORY, "Failed to allocate texture");
    return NULL;
  }

  texture->width = width;
  texture->height = height;
  texture->access = access;
  texture->blend_mode = AP_BLENDMODE_BLEND;
  texture->scale_mode = AP_SCALEMODE_LINEAR;
  texture->address_mode = AP_TEXTUREADDRESS_CLAMP;
  texture->color_mod.r = 1.0f;
  texture->color_mod.g = 1.0f;
  texture->color_mod.b = 1.0f;
  texture->color_mod.a = 1.0f;

  glGenTextures(1, &texture->id);
  if (texture->id == 0) {
    free(texture);
    AP_SET_ERROR(AP_ERROR_OPERATION_FAILED, "Failed to create GL texture");
    return NULL;
  }

  glBindTexture(GL_TEXTURE_2D, texture->id);
  glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA,
               GL_UNSIGNED_BYTE, NULL);
  glBindTexture(GL_TEXTURE_2D, 0);
  AP_TextureBindSampler(texture);

  if (access == AP_TEXTUREACCESS_TARGET && !AP_TextureEnsureFbo(texture)) {
    AP_DestroyTexture(texture);
    return NULL;
  }

  return texture;
}

static bool AP_TextureUpload(AP_Texture *texture, int x, int y, int width,
                             int height, const void *pixels, int pitch) {
  const unsigned char *src;
  unsigned char *row;
  int dst_pitch;
  int dest_y;
  int row_index;

  if (pixels == NULL) {
    AP_SET_ERROR(AP_ERROR_INVALID_ARGUMENT, "Texture pixels cannot be NULL");
    return false;
  }

  dst_pitch = width * 4;
  if (pitch <= 0) {
    pitch = dst_pitch;
  }

  dest_y = texture->invert_v ? texture->height - y - height : y;

  glBindTexture(GL_TEXTURE_2D, texture->id);
  glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

  if (!texture->invert_v && pitch == dst_pitch) {
    glTexSubImage2D(GL_TEXTURE_2D, 0, x, dest_y, width, height, GL_RGBA,
                    GL_UNSIGNED_BYTE, pixels);
    glBindTexture(GL_TEXTURE_2D, 0);
    return true;
  }

  src = (const unsigned char *)pixels;
  row = (unsigned char *)malloc((size_t)dst_pitch);
  if (row == NULL) {
    glBindTexture(GL_TEXTURE_2D, 0);
    AP_SET_ERROR(AP_ERROR_OUT_OF_MEMORY, "Failed to allocate texture row");
    return false;
  }

  for (row_index = 0; row_index < height; ++row_index) {
    int src_row = texture->invert_v ? height - 1 - row_index : row_index;
    memcpy(row, src + (size_t)src_row * (size_t)pitch, (size_t)dst_pitch);
    glTexSubImage2D(GL_TEXTURE_2D, 0, x, dest_y + row_index, width, 1, GL_RGBA,
                    GL_UNSIGNED_BYTE, row);
  }

  free(row);
  glBindTexture(GL_TEXTURE_2D, 0);
  return true;
}

static void AP_RotatePoint(float x, float y, float cx, float cy, float cosine,
                           float sine, float *out_x, float *out_y) {
  float dx = x - cx;
  float dy = y - cy;
  *out_x = dx * cosine - dy * sine + cx;
  *out_y = dx * sine + dy * cosine + cy;
}

static bool AP_TextureFillSourceDest(AP_Texture *texture, const AP_FRect *src,
                                     const AP_FRect *dst, AP_FRect *source,
                                     AP_FRect *dest) {
  AP_Rect viewport;

  if (!AP_TextureIsValid(texture)) {
    AP_SET_ERROR(AP_ERROR_INVALID_ARGUMENT, "Invalid texture");
    return false;
  }

  source->x = 0.0f;
  source->y = 0.0f;
  source->w = (float)texture->width;
  source->h = (float)texture->height;
  if (src != NULL) {
    *source = *src;
  }

  if (!AP_GetRenderViewport(&viewport)) {
    return false;
  }

  dest->x = (float)viewport.x;
  dest->y = (float)viewport.y;
  dest->w = (float)viewport.w;
  dest->h = (float)viewport.h;
  if (dst != NULL) {
    *dest = *dst;
  }

  return true;
}

static void AP_TextureBuildUVs(const AP_Texture *texture, const AP_FRect *source,
                               AP_FlipMode flip, AP_FPoint uvs[4]) {
  float u0 = source->x / (float)texture->width;
  float v0 = source->y / (float)texture->height;
  float u1 = (source->x + source->w) / (float)texture->width;
  float v1 = (source->y + source->h) / (float)texture->height;

  if (texture->invert_v) {
    float tmp = v0;
    v0 = 1.0f - v1;
    v1 = 1.0f - tmp;
  }

  if ((flip & AP_FLIP_HORIZONTAL) != 0) {
    float tmp = u0;
    u0 = u1;
    u1 = tmp;
  }

  if ((flip & AP_FLIP_VERTICAL) != 0) {
    float tmp = v0;
    v0 = v1;
    v1 = tmp;
  }

  uvs[0].x = u0;
  uvs[0].y = v0;
  uvs[1].x = u1;
  uvs[1].y = v0;
  uvs[2].x = u1;
  uvs[2].y = v1;
  uvs[3].x = u0;
  uvs[3].y = v1;
}

static bool AP_TextureDraw(AP_Texture *texture, const AP_FRect *src,
                           const AP_FPoint corners[4], AP_FlipMode flip,
                           AP_Color tint) {
  AP_FRect source;
  AP_FRect dest;
  AP_FPoint uvs[4];

  if (!AP_TextureIsValid(texture) || corners == NULL) {
    AP_SET_ERROR(AP_ERROR_INVALID_ARGUMENT, "Invalid textured draw");
    return false;
  }

  if (!AP_TextureFillSourceDest(texture, src, NULL, &source, &dest)) {
    return false;
  }

  AP_TextureBuildUVs(texture, &source, flip, uvs);
  return AP_RendererSubmitTexturedQuad((AP_UInt)texture->id, texture->blend_mode,
                                       corners, uvs, tint);
}

static bool AP_TextureDrawRect(AP_Texture *texture, const AP_FRect *src,
                               const AP_FRect *dst, float angle,
                               const AP_FPoint *center, AP_FlipMode flip,
                               AP_Color tint) {
  AP_FRect source;
  AP_FRect dest;
  AP_FPoint corners[4];
  AP_FPoint uvs[4];
  AP_FPoint pivot;
  float cosine;
  float sine;

  if (!AP_TextureIsValid(texture)) {
    AP_SET_ERROR(AP_ERROR_INVALID_ARGUMENT, "Invalid texture");
    return false;
  }

  if (!AP_TextureFillSourceDest(texture, src, dst, &source, &dest)) {
    return false;
  }

  AP_TextureBuildUVs(texture, &source, flip, uvs);

  corners[0].x = dest.x;
  corners[0].y = dest.y;
  corners[1].x = dest.x + dest.w;
  corners[1].y = dest.y;
  corners[2].x = dest.x + dest.w;
  corners[2].y = dest.y + dest.h;
  corners[3].x = dest.x;
  corners[3].y = dest.y + dest.h;

  if (center != NULL) {
    pivot = *center;
  } else {
    pivot.x = dest.x + dest.w * 0.5f;
    pivot.y = dest.y + dest.h * 0.5f;
  }

  cosine = cosf(angle * AP_TEXTURE_DEG2RAD);
  sine = sinf(angle * AP_TEXTURE_DEG2RAD);
  AP_RotatePoint(corners[0].x, corners[0].y, pivot.x, pivot.y, cosine, sine,
                 &corners[0].x, &corners[0].y);
  AP_RotatePoint(corners[1].x, corners[1].y, pivot.x, pivot.y, cosine, sine,
                 &corners[1].x, &corners[1].y);
  AP_RotatePoint(corners[2].x, corners[2].y, pivot.x, pivot.y, cosine, sine,
                 &corners[2].x, &corners[2].y);
  AP_RotatePoint(corners[3].x, corners[3].y, pivot.x, pivot.y, cosine, sine,
                 &corners[3].x, &corners[3].y);

  return AP_RendererSubmitTexturedQuad((AP_UInt)texture->id, texture->blend_mode,
                                       corners, uvs, tint);
}

bool AP_TextureRenderRotatedTinted(AP_Texture *texture, const AP_FRect *src,
                                   const AP_FRect *dst, float angle,
                                   const AP_FPoint *center, AP_FlipMode flip,
                                   AP_Color tint) {
  return AP_TextureDrawRect(texture, src, dst, angle, center, flip, tint);
}

AP_Texture *AP_CreateTexture(int width, int height) {
  return AP_TextureCreateEmpty(width, height, AP_TEXTUREACCESS_STATIC);
}

AP_Texture *AP_CreateTextureWithAccess(int width, int height,
                                       AP_TextureAccess access) {
  return AP_TextureCreateEmpty(width, height, access);
}

AP_Texture *AP_CreateTextureFromPixels(int width, int height,
                                       const void *pixels, int pitch) {
  AP_Texture *texture =
      AP_TextureCreateEmpty(width, height, AP_TEXTUREACCESS_STATIC);

  if (texture == NULL) {
    return NULL;
  }

  if (!AP_TextureUpload(texture, 0, 0, width, height, pixels, pitch)) {
    AP_DestroyTexture(texture);
    return NULL;
  }

  return texture;
}

AP_Texture *AP_CreateTextureFromImage(const AP_Image *image) {
  if (!AP_ImageIsValid(image)) {
    AP_SET_ERROR(AP_ERROR_INVALID_ARGUMENT, "Invalid image");
    return NULL;
  }

  return AP_CreateTextureFromPixels(AP_GetImageWidth(image),
                                    AP_GetImageHeight(image),
                                    AP_GetImagePixels(image),
                                    AP_GetImagePitch(image));
}

bool AP_UpdateTextureFromImage(AP_Texture *texture, const AP_Image *image) {
  int width;
  int height;

  if (!AP_TextureIsValid(texture) || !AP_ImageIsValid(image)) {
    AP_SET_ERROR(AP_ERROR_INVALID_ARGUMENT, "Invalid texture or image");
    return false;
  }

  width = AP_GetImageWidth(image);
  height = AP_GetImageHeight(image);
  if (width != texture->width || height != texture->height) {
    AP_SET_ERROR(AP_ERROR_INVALID_ARGUMENT,
                 "Image size does not match texture");
    return false;
  }

  return AP_UpdateTexture(texture, NULL, AP_GetImagePixels(image),
                          AP_GetImagePitch(image));
}

AP_Image *AP_CreateImageFromTexture(AP_Texture *texture) {
  int pitch;
  unsigned char *pixels;
  AP_Image *image;

  if (!AP_TextureIsValid(texture)) {
    AP_SET_ERROR(AP_ERROR_INVALID_ARGUMENT, "Invalid texture");
    return NULL;
  }

  pitch = texture->width * 4;
  pixels = (unsigned char *)malloc((size_t)pitch * (size_t)texture->height);
  if (pixels == NULL) {
    AP_SET_ERROR(AP_ERROR_OUT_OF_MEMORY, "Failed to read texture into image");
    return NULL;
  }

  if (!AP_ReadTexturePixels(texture, NULL, pixels, pitch)) {
    free(pixels);
    return NULL;
  }

  image = AP_CreateImageFromPixels(texture->width, texture->height, pixels,
                                   pitch);
  free(pixels);
  return image;
}

bool AP_SaveTexture(AP_Texture *texture, const char *path) {
  AP_Image *image;
  bool ok;

  image = AP_CreateImageFromTexture(texture);
  if (image == NULL) {
    return false;
  }

  ok = AP_SaveImage(image, path);
  AP_DestroyImage(image);
  return ok;
}

AP_Texture *AP_LoadTexture(const char *path) {
  AP_Image *image;
  AP_Texture *texture;

  image = AP_LoadImage(path);
  if (image == NULL) {
    return NULL;
  }

  texture = AP_CreateTextureFromImage(image);
  AP_DestroyImage(image);
  return texture;
}

AP_Texture *AP_LoadTextureFromMemory(const void *data, int size) {
  AP_Image *image;
  AP_Texture *texture;

  image = AP_LoadImageFromMemory(data, size);
  if (image == NULL) {
    return NULL;
  }

  texture = AP_CreateTextureFromImage(image);
  AP_DestroyImage(image);
  return texture;
}

AP_Texture *AP_CopyTexture(AP_Texture *texture) {
  int pitch;
  unsigned char *pixels;
  AP_Texture *copy;

  if (!AP_TextureIsValid(texture)) {
    AP_SET_ERROR(AP_ERROR_INVALID_ARGUMENT, "Invalid texture");
    return NULL;
  }

  pitch = texture->width * 4;
  pixels = (unsigned char *)malloc((size_t)pitch * (size_t)texture->height);
  if (pixels == NULL) {
    AP_SET_ERROR(AP_ERROR_OUT_OF_MEMORY, "Failed to copy texture");
    return NULL;
  }

  if (!AP_ReadTexturePixels(texture, NULL, pixels, pitch)) {
    free(pixels);
    return NULL;
  }

  copy = AP_CreateTextureFromPixels(texture->width, texture->height, pixels,
                                    pitch);
  free(pixels);
  if (copy != NULL) {
    copy->blend_mode = texture->blend_mode;
    copy->scale_mode = texture->scale_mode;
    copy->address_mode = texture->address_mode;
    copy->color_mod = texture->color_mod;
    AP_TextureBindSampler(copy);
  }
  return copy;
}

void AP_DestroyTexture(AP_Texture *texture) {
  if (texture == NULL) {
    return;
  }

  if (g_render_target == texture) {
    AP_SetRenderTarget(NULL);
  }

  if (AP_OpenGLHasContext()) {
    if (texture->fbo != 0) {
      glDeleteFramebuffers(1, &texture->fbo);
    }
    if (texture->id != 0) {
      glDeleteTextures(1, &texture->id);
    }
  }

  free(texture->pixels);
  free(texture);
}

bool AP_TextureIsValid(const AP_Texture *texture) {
  return texture != NULL && texture->id != 0 && texture->width > 0 &&
         texture->height > 0;
}

AP_UInt AP_TextureNativeId(const AP_Texture *texture) {
  return AP_TextureIsValid(texture) ? (AP_UInt)texture->id : 0;
}

AP_TextureAccess AP_GetTextureAccess(const AP_Texture *texture) {
  return AP_TextureIsValid(texture) ? texture->access : AP_TEXTUREACCESS_STATIC;
}

bool AP_GetTextureSize(AP_Texture *texture, int *w, int *h) {
  if (!AP_TextureIsValid(texture)) {
    AP_SET_ERROR(AP_ERROR_INVALID_ARGUMENT, "Invalid texture");
    return false;
  }

  if (w != NULL) {
    *w = texture->width;
  }
  if (h != NULL) {
    *h = texture->height;
  }
  return true;
}

int AP_GetTextureWidth(AP_Texture *texture) {
  return AP_TextureIsValid(texture) ? texture->width : 0;
}

int AP_GetTextureHeight(AP_Texture *texture) {
  return AP_TextureIsValid(texture) ? texture->height : 0;
}

bool AP_UpdateTexture(AP_Texture *texture, const AP_Rect *rect,
                      const void *pixels, int pitch) {
  int x = 0;
  int y = 0;
  int width;
  int height;

  if (!AP_TextureIsValid(texture)) {
    AP_SET_ERROR(AP_ERROR_INVALID_ARGUMENT, "Invalid texture");
    return false;
  }

  if (!AP_TextureHasContext()) {
    return false;
  }

  width = texture->width;
  height = texture->height;
  if (rect != NULL) {
    x = rect->x;
    y = rect->y;
    width = rect->w;
    height = rect->h;
  }

  if (x < 0 || y < 0 || width <= 0 || height <= 0 ||
      x + width > texture->width || y + height > texture->height) {
    AP_SET_ERROR(AP_ERROR_INVALID_ARGUMENT, "Texture update rect is out of bounds");
    return false;
  }

  return AP_TextureUpload(texture, x, y, width, height, pixels, pitch);
}

bool AP_LockTexture(AP_Texture *texture, const AP_Rect *rect, void **pixels,
                    int *pitch) {
  size_t bytes;

  if (!AP_TextureIsValid(texture) || pixels == NULL || pitch == NULL) {
    AP_SET_ERROR(AP_ERROR_INVALID_ARGUMENT, "Invalid texture lock");
    return false;
  }

  if (texture->locked) {
    AP_SET_ERROR(AP_ERROR_INVALID_STATE, "Texture is already locked");
    return false;
  }

  bytes = (size_t)texture->width * (size_t)texture->height * 4u;
  if (texture->pixels == NULL) {
    texture->pixels = (unsigned char *)calloc(1, bytes);
    if (texture->pixels == NULL) {
      AP_SET_ERROR(AP_ERROR_OUT_OF_MEMORY, "Failed to lock texture");
      return false;
    }
  }

  if (!AP_ReadTexturePixels(texture, NULL, texture->pixels, texture->width * 4)) {
    return false;
  }

  texture->lock_rect.x = 0;
  texture->lock_rect.y = 0;
  texture->lock_rect.w = texture->width;
  texture->lock_rect.h = texture->height;
  if (rect != NULL) {
    texture->lock_rect = *rect;
  }

  if (texture->lock_rect.x < 0 || texture->lock_rect.y < 0 ||
      texture->lock_rect.w <= 0 || texture->lock_rect.h <= 0 ||
      texture->lock_rect.x + texture->lock_rect.w > texture->width ||
      texture->lock_rect.y + texture->lock_rect.h > texture->height) {
    AP_SET_ERROR(AP_ERROR_INVALID_ARGUMENT, "Texture lock rect is out of bounds");
    return false;
  }

  texture->locked = true;
  *pitch = texture->width * 4;
  *pixels = texture->pixels +
            ((size_t)texture->lock_rect.y * (size_t)texture->width +
             (size_t)texture->lock_rect.x) *
                4u;
  return true;
}

bool AP_UnlockTexture(AP_Texture *texture) {
  if (!AP_TextureIsValid(texture) || !texture->locked) {
    AP_SET_ERROR(AP_ERROR_INVALID_STATE, "Texture is not locked");
    return false;
  }

  texture->locked = false;
  return AP_TextureUpload(
      texture, texture->lock_rect.x, texture->lock_rect.y, texture->lock_rect.w,
      texture->lock_rect.h,
      texture->pixels +
          ((size_t)texture->lock_rect.y * (size_t)texture->width +
           (size_t)texture->lock_rect.x) *
              4u,
      texture->width * 4);
}

bool AP_ReadTexturePixels(AP_Texture *texture, const AP_Rect *rect, void *pixels,
                          int pitch) {
  unsigned char *full;
  int full_pitch;
  int x = 0;
  int y = 0;
  int width;
  int height;
  int row;

  if (!AP_TextureIsValid(texture) || pixels == NULL) {
    AP_SET_ERROR(AP_ERROR_INVALID_ARGUMENT, "Invalid texture read");
    return false;
  }

  if (!AP_TextureHasContext()) {
    return false;
  }

  width = texture->width;
  height = texture->height;
  if (rect != NULL) {
    x = rect->x;
    y = rect->y;
    width = rect->w;
    height = rect->h;
  }

  if (x < 0 || y < 0 || width <= 0 || height <= 0 ||
      x + width > texture->width || y + height > texture->height) {
    AP_SET_ERROR(AP_ERROR_INVALID_ARGUMENT, "Texture read rect is out of bounds");
    return false;
  }

  full_pitch = texture->width * 4;
  if (pitch <= 0) {
    pitch = width * 4;
  }

  full = (unsigned char *)malloc((size_t)full_pitch * (size_t)texture->height);
  if (full == NULL) {
    AP_SET_ERROR(AP_ERROR_OUT_OF_MEMORY, "Failed to read texture");
    return false;
  }

  AP_FlushRenderer();
  glBindTexture(GL_TEXTURE_2D, texture->id);
  glPixelStorei(GL_PACK_ALIGNMENT, 1);
  glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_BYTE, full);
  glBindTexture(GL_TEXTURE_2D, 0);

  for (row = 0; row < height; ++row) {
    int src_row = texture->invert_v ? texture->height - 1 - (y + row) : y + row;
    memcpy((unsigned char *)pixels + (size_t)row * (size_t)pitch,
           full + (size_t)src_row * (size_t)full_pitch + (size_t)x * 4u,
           (size_t)width * 4u);
  }

  free(full);
  return true;
}

bool AP_GenerateTextureMipmaps(AP_Texture *texture) {
  if (!AP_TextureIsValid(texture)) {
    AP_SET_ERROR(AP_ERROR_INVALID_ARGUMENT, "Invalid texture");
    return false;
  }

  if (!AP_TextureHasContext()) {
    return false;
  }

  AP_FlushRenderer();
  glBindTexture(GL_TEXTURE_2D, texture->id);
  glGenerateMipmap(GL_TEXTURE_2D);
  glBindTexture(GL_TEXTURE_2D, 0);
  texture->scale_mode = AP_SCALEMODE_MIPMAP;
  AP_TextureBindSampler(texture);
  return true;
}

bool AP_SetTextureColorMod(AP_Texture *texture, AP_U8 r, AP_U8 g, AP_U8 b) {
  return AP_SetTextureColorModFloat(texture, (float)r / 255.0f,
                                    (float)g / 255.0f, (float)b / 255.0f);
}

bool AP_SetTextureColorModFloat(AP_Texture *texture, float r, float g,
                                float b) {
  if (!AP_TextureIsValid(texture)) {
    AP_SET_ERROR(AP_ERROR_INVALID_ARGUMENT, "Invalid texture");
    return false;
  }

  texture->color_mod.r = AP_Clampf01(r);
  texture->color_mod.g = AP_Clampf01(g);
  texture->color_mod.b = AP_Clampf01(b);
  return true;
}

bool AP_GetTextureColorMod(AP_Texture *texture, AP_U8 *r, AP_U8 *g, AP_U8 *b) {
  float red;
  float green;
  float blue;

  if (!AP_GetTextureColorModFloat(texture, &red, &green, &blue)) {
    return false;
  }

  if (r != NULL) {
    *r = AP_FloatToByte(red);
  }
  if (g != NULL) {
    *g = AP_FloatToByte(green);
  }
  if (b != NULL) {
    *b = AP_FloatToByte(blue);
  }
  return true;
}

bool AP_GetTextureColorModFloat(AP_Texture *texture, float *r, float *g,
                                float *b) {
  if (!AP_TextureIsValid(texture)) {
    AP_SET_ERROR(AP_ERROR_INVALID_ARGUMENT, "Invalid texture");
    return false;
  }

  if (r != NULL) {
    *r = texture->color_mod.r;
  }
  if (g != NULL) {
    *g = texture->color_mod.g;
  }
  if (b != NULL) {
    *b = texture->color_mod.b;
  }
  return true;
}

bool AP_SetTextureAlphaMod(AP_Texture *texture, AP_U8 alpha) {
  return AP_SetTextureAlphaModFloat(texture, (float)alpha / 255.0f);
}

bool AP_SetTextureAlphaModFloat(AP_Texture *texture, float alpha) {
  if (!AP_TextureIsValid(texture)) {
    AP_SET_ERROR(AP_ERROR_INVALID_ARGUMENT, "Invalid texture");
    return false;
  }

  texture->color_mod.a = AP_Clampf01(alpha);
  return true;
}

bool AP_GetTextureAlphaMod(AP_Texture *texture, AP_U8 *alpha) {
  float value;

  if (!AP_GetTextureAlphaModFloat(texture, &value)) {
    return false;
  }

  if (alpha != NULL) {
    *alpha = AP_FloatToByte(value);
  }
  return true;
}

bool AP_GetTextureAlphaModFloat(AP_Texture *texture, float *alpha) {
  if (!AP_TextureIsValid(texture)) {
    AP_SET_ERROR(AP_ERROR_INVALID_ARGUMENT, "Invalid texture");
    return false;
  }

  if (alpha != NULL) {
    *alpha = texture->color_mod.a;
  }
  return true;
}

bool AP_SetTextureBlendMode(AP_Texture *texture, AP_BlendMode mode) {
  if (!AP_TextureIsValid(texture)) {
    AP_SET_ERROR(AP_ERROR_INVALID_ARGUMENT, "Invalid texture");
    return false;
  }

  texture->blend_mode = mode;
  return true;
}

AP_BlendMode AP_GetTextureBlendMode(AP_Texture *texture) {
  return AP_TextureIsValid(texture) ? texture->blend_mode : AP_BLENDMODE_BLEND;
}

bool AP_SetTextureScaleMode(AP_Texture *texture, AP_ScaleMode mode) {
  if (!AP_TextureIsValid(texture) || !AP_TextureHasContext()) {
    return false;
  }

  texture->scale_mode = mode;
  AP_TextureBindSampler(texture);
  return true;
}

AP_ScaleMode AP_GetTextureScaleMode(AP_Texture *texture) {
  return AP_TextureIsValid(texture) ? texture->scale_mode : AP_SCALEMODE_LINEAR;
}

bool AP_SetTextureAddressMode(AP_Texture *texture, AP_TextureAddressMode mode) {
  if (!AP_TextureIsValid(texture) || !AP_TextureHasContext()) {
    return false;
  }

  texture->address_mode = mode;
  AP_TextureBindSampler(texture);
  return true;
}

AP_TextureAddressMode AP_GetTextureAddressMode(AP_Texture *texture) {
  return AP_TextureIsValid(texture) ? texture->address_mode
                                    : AP_TEXTUREADDRESS_CLAMP;
}

bool AP_SetRenderTarget(AP_Texture *texture) {
  if (texture == NULL) {
    g_render_target = NULL;
    return AP_RendererBindTarget(0, 0, 0);
  }

  if (!AP_TextureIsValid(texture)) {
    AP_SET_ERROR(AP_ERROR_INVALID_ARGUMENT, "Invalid render target");
    return false;
  }

  if (!AP_TextureEnsureFbo(texture)) {
    return false;
  }

  g_render_target = texture;
  return AP_RendererBindTarget((AP_UInt)texture->fbo, texture->width,
                               texture->height);
}

AP_Texture *AP_GetRenderTarget(void) { return g_render_target; }

bool AP_RenderTexture(AP_Texture *texture, const AP_FRect *src,
                      const AP_FRect *dst) {
  return AP_TextureDrawRect(texture, src, dst, 0.0f, NULL, AP_FLIP_NONE,
                            texture->color_mod);
}

bool AP_RenderTextureRotated(AP_Texture *texture, const AP_FRect *src,
                             const AP_FRect *dst, float angle,
                             const AP_FPoint *center, AP_FlipMode flip) {
  return AP_TextureDrawRect(texture, src, dst, angle, center, flip,
                            texture->color_mod);
}

bool AP_RenderTextureTiled(AP_Texture *texture, const AP_FRect *src,
                           float scale, const AP_FRect *dst) {
  AP_FRect source;
  AP_FRect dest;
  float tile_w;
  float tile_h;
  float y;

  if (scale <= 0.0f) {
    scale = 1.0f;
  }

  if (!AP_TextureFillSourceDest(texture, src, dst, &source, &dest)) {
    return false;
  }

  tile_w = source.w * scale;
  tile_h = source.h * scale;
  if (tile_w <= 0.0f || tile_h <= 0.0f) {
    return true;
  }

  for (y = dest.y; y < dest.y + dest.h; y += tile_h) {
    float x;
    float slice_h = tile_h;
    if (y + slice_h > dest.y + dest.h) {
      slice_h = dest.y + dest.h - y;
    }

    for (x = dest.x; x < dest.x + dest.w; x += tile_w) {
      AP_FRect tile_src = source;
      AP_FRect tile_dst;
      float slice_w = tile_w;

      if (x + slice_w > dest.x + dest.w) {
        slice_w = dest.x + dest.w - x;
      }

      tile_src.w = source.w * (slice_w / tile_w);
      tile_src.h = source.h * (slice_h / tile_h);
      tile_dst.x = x;
      tile_dst.y = y;
      tile_dst.w = slice_w;
      tile_dst.h = slice_h;
      if (!AP_TextureDrawRect(texture, &tile_src, &tile_dst, 0.0f, NULL,
                              AP_FLIP_NONE, texture->color_mod)) {
        return false;
      }
    }
  }

  return true;
}

bool AP_RenderTexture9Grid(AP_Texture *texture, const AP_FRect *src, float left,
                           float right, float top, float bottom, float scale,
                           const AP_FRect *dst) {
  AP_FRect source;
  AP_FRect dest;
  float cx;
  float cy;
  float dl;
  float dr;
  float dt;
  float db;
  AP_FRect s;
  AP_FRect d;

  if (scale <= 0.0f) {
    scale = 1.0f;
  }

  if (!AP_TextureFillSourceDest(texture, src, dst, &source, &dest)) {
    return false;
  }

  if (left < 0.0f || right < 0.0f || top < 0.0f || bottom < 0.0f ||
      left + right > source.w || top + bottom > source.h) {
    AP_SET_ERROR(AP_ERROR_INVALID_ARGUMENT, "Invalid 9-grid borders");
    return false;
  }

  cx = source.w - left - right;
  cy = source.h - top - bottom;
  dl = left * scale;
  dr = right * scale;
  dt = top * scale;
  db = bottom * scale;

  if (dl + dr > dest.w && dl + dr > 0.0f) {
    float fit = dest.w / (dl + dr);
    dl *= fit;
    dr *= fit;
  }
  if (dt + db > dest.h && dt + db > 0.0f) {
    float fit = dest.h / (dt + db);
    dt *= fit;
    db *= fit;
  }

  s.x = source.x;
  s.y = source.y;
  s.w = left;
  s.h = top;
  d.x = dest.x;
  d.y = dest.y;
  d.w = dl;
  d.h = dt;
  if (!AP_TextureDrawRect(texture, &s, &d, 0.0f, NULL, AP_FLIP_NONE,
                          texture->color_mod)) {
    return false;
  }

  s.x = source.x + left;
  s.w = cx;
  d.x = dest.x + dl;
  d.w = dest.w - dl - dr;
  if (!AP_TextureDrawRect(texture, &s, &d, 0.0f, NULL, AP_FLIP_NONE,
                          texture->color_mod)) {
    return false;
  }

  s.x = source.x + left + cx;
  s.w = right;
  d.x = dest.x + dest.w - dr;
  d.w = dr;
  if (!AP_TextureDrawRect(texture, &s, &d, 0.0f, NULL, AP_FLIP_NONE,
                          texture->color_mod)) {
    return false;
  }

  s.x = source.x;
  s.y = source.y + top;
  s.w = left;
  s.h = cy;
  d.x = dest.x;
  d.y = dest.y + dt;
  d.w = dl;
  d.h = dest.h - dt - db;
  if (!AP_TextureDrawRect(texture, &s, &d, 0.0f, NULL, AP_FLIP_NONE,
                          texture->color_mod)) {
    return false;
  }

  s.x = source.x + left;
  s.w = cx;
  d.x = dest.x + dl;
  d.w = dest.w - dl - dr;
  if (!AP_TextureDrawRect(texture, &s, &d, 0.0f, NULL, AP_FLIP_NONE,
                          texture->color_mod)) {
    return false;
  }

  s.x = source.x + left + cx;
  s.w = right;
  d.x = dest.x + dest.w - dr;
  d.w = dr;
  if (!AP_TextureDrawRect(texture, &s, &d, 0.0f, NULL, AP_FLIP_NONE,
                          texture->color_mod)) {
    return false;
  }

  s.x = source.x;
  s.y = source.y + top + cy;
  s.w = left;
  s.h = bottom;
  d.x = dest.x;
  d.y = dest.y + dest.h - db;
  d.w = dl;
  d.h = db;
  if (!AP_TextureDrawRect(texture, &s, &d, 0.0f, NULL, AP_FLIP_NONE,
                          texture->color_mod)) {
    return false;
  }

  s.x = source.x + left;
  s.w = cx;
  d.x = dest.x + dl;
  d.w = dest.w - dl - dr;
  if (!AP_TextureDrawRect(texture, &s, &d, 0.0f, NULL, AP_FLIP_NONE,
                          texture->color_mod)) {
    return false;
  }

  s.x = source.x + left + cx;
  s.w = right;
  d.x = dest.x + dest.w - dr;
  d.w = dr;
  return AP_TextureDrawRect(texture, &s, &d, 0.0f, NULL, AP_FLIP_NONE,
                            texture->color_mod);
}

bool AP_RenderTextureAffine(AP_Texture *texture, const AP_FRect *src,
                            const AP_FPoint *origin, const AP_FPoint *right,
                            const AP_FPoint *down) {
  AP_FPoint corners[4];

  if (origin == NULL || right == NULL || down == NULL) {
    AP_SET_ERROR(AP_ERROR_INVALID_ARGUMENT, "Affine points cannot be NULL");
    return false;
  }

  corners[0] = *origin;
  corners[1] = *right;
  corners[2].x = right->x + (down->x - origin->x);
  corners[2].y = right->y + (down->y - origin->y);
  corners[3] = *down;
  return AP_TextureDraw(texture, src, corners, AP_FLIP_NONE, texture->color_mod);
}

bool AP_RenderTextureQuad(AP_Texture *texture, const AP_FRect *src,
                          const AP_FPoint corners[4]) {
  return AP_TextureDraw(texture, src, corners, AP_FLIP_NONE, texture->color_mod);
}

bool AP_RenderTextureGeometry(AP_Texture *texture, const AP_Vertex *vertices,
                              int num_vertices, const int *indices,
                              int num_indices) {
  AP_U16 *packed;
  int i;
  bool result;

  if (!AP_TextureIsValid(texture)) {
    AP_SET_ERROR(AP_ERROR_INVALID_ARGUMENT, "Invalid texture");
    return false;
  }

  if (indices == NULL) {
    return AP_RendererDrawMesh((AP_UInt)texture->id, texture->blend_mode,
                               vertices, num_vertices, NULL, 0,
                               AP_PRIM_TRIANGLES);
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

  result = AP_RendererDrawMesh((AP_UInt)texture->id, texture->blend_mode,
                               vertices, num_vertices, packed, num_indices,
                               AP_PRIM_TRIANGLES);
  free(packed);
  return result;
}
