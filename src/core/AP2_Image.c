/*
 * AP2 — Application Primitives
 * Copyright (c) 2024-2026 Jack Waechter
 *
 * Licensed under the MIT License.
 * See LICENSE in the project root for full terms.
 */

#include "AP2/AP2_Image.h"

#include "AP2/AP2_Error.h"

#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-function"
#pragma GCC diagnostic ignored "-Wunused-parameter"
#pragma GCC diagnostic ignored "-Wsign-compare"
#endif
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

#if defined (_WIN32) || defined (_WIN64)
#include <direct.h>
#define GETCWD(buffer, size) _getcwd(buffer, size)
#else 
#include <unistd.h>
#define GETCWD(buffer, size) getcwd(buffer, size)
#endif 

#include <stdlib.h>
#include <string.h>

struct AP_Image {
  int width;
  int height;
  int pitch;
  unsigned char *pixels;
};

static AP_U8 AP_ImageClampByte(int value) {
  if (value < 0) {
    return 0;
  }
  if (value > 255) {
    return 255;
  }
  return (AP_U8)value;
}

static AP_U8 AP_ImageFloatToByte(float value) {
  if (value < 0.0f) {
    return 0;
  }
  if (value > 1.0f) {
    return 255;
  }
  return (AP_U8)(value * 255.0f + 0.5f);
}

static unsigned char *AP_ImageAt(AP_Image *image, int x, int y) {
  return image->pixels + (size_t)y * (size_t)image->pitch + (size_t)x * 4u;
}

static const unsigned char *AP_ImageAtConst(const AP_Image *image, int x,
                                            int y) {
  return image->pixels + (size_t)y * (size_t)image->pitch + (size_t)x * 4u;
}

static AP_Image *AP_ImageAlloc(int width, int height) {
  AP_Image *image;

  if (width <= 0 || height <= 0) {
    AP_SET_ERROR(AP_ERROR_INVALID_ARGUMENT, "Image size must be positive");
    return NULL;
  }

  image = (AP_Image *)calloc(1, sizeof(AP_Image));
  if (image == NULL) {
    AP_SET_ERROR(AP_ERROR_OUT_OF_MEMORY, "Failed to allocate image");
    return NULL;
  }

  image->width = width;
  image->height = height;
  image->pitch = width * 4;
  image->pixels = (unsigned char *)calloc((size_t)image->pitch * (size_t)height,
                                          1u);
  if (image->pixels == NULL) {
    free(image);
    AP_SET_ERROR(AP_ERROR_OUT_OF_MEMORY, "Failed to allocate image pixels");
    return NULL;
  }

  return image;
}

static bool AP_ImageCopyRect(const unsigned char *src, int src_pitch, int src_x,
                             int src_y, unsigned char *dst, int dst_pitch,
                             int dst_x, int dst_y, int width, int height) {
  int row;
  size_t bytes;

  if (width <= 0 || height <= 0) {
    return true;
  }

  bytes = (size_t)width * 4u;
  for (row = 0; row < height; ++row) {
    memcpy(dst + (size_t)(dst_y + row) * (size_t)dst_pitch + (size_t)dst_x * 4u,
           src + (size_t)(src_y + row) * (size_t)src_pitch + (size_t)src_x * 4u,
           bytes);
  }
  return true;
}

static bool AP_ImageClipBlit(const AP_Image *src, const AP_Rect *src_rect,
                             const AP_Image *dst, int *dst_x, int *dst_y,
                             int *src_x, int *src_y, int *width, int *height) {
  int sx = 0;
  int sy = 0;
  int sw;
  int sh;
  int dx;
  int dy;

  sw = src->width;
  sh = src->height;
  if (src_rect != NULL) {
    sx = src_rect->x;
    sy = src_rect->y;
    sw = src_rect->w;
    sh = src_rect->h;
  }

  if (sx < 0) {
    sw += sx;
    sx = 0;
  }
  if (sy < 0) {
    sh += sy;
    sy = 0;
  }
  if (sx + sw > src->width) {
    sw = src->width - sx;
  }
  if (sy + sh > src->height) {
    sh = src->height - sy;
  }

  dx = *dst_x;
  dy = *dst_y;
  if (dx < 0) {
    sx -= dx;
    sw += dx;
    dx = 0;
  }
  if (dy < 0) {
    sy -= dy;
    sh += dy;
    dy = 0;
  }
  if (dx + sw > dst->width) {
    sw = dst->width - dx;
  }
  if (dy + sh > dst->height) {
    sh = dst->height - dy;
  }

  if (sw <= 0 || sh <= 0) {
    *width = 0;
    *height = 0;
    return true;
  }

  *dst_x = dx;
  *dst_y = dy;
  *src_x = sx;
  *src_y = sy;
  *width = sw;
  *height = sh;
  return true;
}

static void AP_ImageBlendPixel(unsigned char *dst, const unsigned char *src,
                               AP_BlendMode blend) {
  unsigned r;
  unsigned g;
  unsigned b;
  unsigned a;
  unsigned sa;
  unsigned inv;

  switch (blend) {
  case AP_BLENDMODE_NONE:
    dst[0] = src[0];
    dst[1] = src[1];
    dst[2] = src[2];
    dst[3] = src[3];
    return;
  case AP_BLENDMODE_ADD:
    dst[0] = AP_ImageClampByte((int)dst[0] + (int)src[0]);
    dst[1] = AP_ImageClampByte((int)dst[1] + (int)src[1]);
    dst[2] = AP_ImageClampByte((int)dst[2] + (int)src[2]);
    dst[3] = AP_ImageClampByte((int)dst[3] + (int)src[3]);
    return;
  case AP_BLENDMODE_MOD:
  case AP_BLENDMODE_MUL:
    dst[0] = (AP_U8)(((unsigned)dst[0] * (unsigned)src[0]) / 255u);
    dst[1] = (AP_U8)(((unsigned)dst[1] * (unsigned)src[1]) / 255u);
    dst[2] = (AP_U8)(((unsigned)dst[2] * (unsigned)src[2]) / 255u);
    dst[3] = (AP_U8)(((unsigned)dst[3] * (unsigned)src[3]) / 255u);
    return;
  case AP_BLENDMODE_SCREEN:
    dst[0] = (AP_U8)(255u - ((255u - dst[0]) * (255u - src[0])) / 255u);
    dst[1] = (AP_U8)(255u - ((255u - dst[1]) * (255u - src[1])) / 255u);
    dst[2] = (AP_U8)(255u - ((255u - dst[2]) * (255u - src[2])) / 255u);
    dst[3] = AP_ImageClampByte((int)dst[3] + (int)src[3]);
    return;
  case AP_BLENDMODE_PREMULTIPLIED:
    sa = src[3];
    inv = 255u - sa;
    dst[0] = AP_ImageClampByte((int)(src[0] + (dst[0] * inv) / 255u));
    dst[1] = AP_ImageClampByte((int)(src[1] + (dst[1] * inv) / 255u));
    dst[2] = AP_ImageClampByte((int)(src[2] + (dst[2] * inv) / 255u));
    dst[3] = AP_ImageClampByte((int)(sa + (dst[3] * inv) / 255u));
    return;
  case AP_BLENDMODE_BLEND:
  default:
    sa = src[3];
    inv = 255u - sa;
    r = (src[0] * sa + dst[0] * inv) / 255u;
    g = (src[1] * sa + dst[1] * inv) / 255u;
    b = (src[2] * sa + dst[2] * inv) / 255u;
    a = sa + (dst[3] * inv) / 255u;
    dst[0] = AP_ImageClampByte((int)r);
    dst[1] = AP_ImageClampByte((int)g);
    dst[2] = AP_ImageClampByte((int)b);
    dst[3] = AP_ImageClampByte((int)a);
    return;
  }
}

static void AP_ImageSample(const AP_Image *image, float u, float v,
                           AP_ImageFilter filter, unsigned char out[4]) {
  float x;
  float y;
  int x0;
  int y0;
  int x1;
  int y1;
  float fx;
  float fy;
  const unsigned char *c00;
  const unsigned char *c10;
  const unsigned char *c01;
  const unsigned char *c11;
  int channel;

  x = u * (float)image->width - 0.5f;
  y = v * (float)image->height - 0.5f;
  if (x < 0.0f) {
    x = 0.0f;
  }
  if (y < 0.0f) {
    y = 0.0f;
  }
  if (x > (float)(image->width - 1)) {
    x = (float)(image->width - 1);
  }
  if (y > (float)(image->height - 1)) {
    y = (float)(image->height - 1);
  }

  x0 = (int)x;
  y0 = (int)y;
  if (filter == AP_IMAGE_NEAREST) {
    memcpy(out, AP_ImageAtConst(image, x0, y0), 4u);
    return;
  }

  x1 = x0 + 1;
  y1 = y0 + 1;
  if (x1 >= image->width) {
    x1 = image->width - 1;
  }
  if (y1 >= image->height) {
    y1 = image->height - 1;
  }

  fx = x - (float)x0;
  fy = y - (float)y0;
  c00 = AP_ImageAtConst(image, x0, y0);
  c10 = AP_ImageAtConst(image, x1, y0);
  c01 = AP_ImageAtConst(image, x0, y1);
  c11 = AP_ImageAtConst(image, x1, y1);
  for (channel = 0; channel < 4; ++channel) {
    float top = (float)c00[channel] + ((float)c10[channel] - (float)c00[channel]) * fx;
    float bot = (float)c01[channel] + ((float)c11[channel] - (float)c01[channel]) * fx;
    out[channel] = AP_ImageClampByte((int)(top + (bot - top) * fy + 0.5f));
  }
}

static const char *AP_ImageExtension(const char *path) {
  const char *dot = NULL;
  const char *cursor;

  if (path == NULL) {
    return NULL;
  }

  for (cursor = path; *cursor != '\0'; ++cursor) {
    if (*cursor == '.' ) {
      dot = cursor;
    } else if (*cursor == '/' || *cursor == '\\') {
      dot = NULL;
    }
  }
  return dot;
}

static bool AP_ImageExtEquals(const char *ext, const char *wanted) {
  size_t index;

  if (ext == NULL || wanted == NULL) {
    return false;
  }

  for (index = 0; wanted[index] != '\0'; ++index) {
    char a = ext[index];
    char b = wanted[index];
    if (a >= 'A' && a <= 'Z') {
      a = (char)(a - 'A' + 'a');
    }
    if (a != b) {
      return false;
    }
  }
  return ext[index] == '\0';
}

AP_Image *AP_CreateImage(int width, int height) {
  return AP_ImageAlloc(width, height);
}

AP_Image *AP_CreateImageFromPixels(int width, int height, const void *pixels,
                                   int pitch) {
  AP_Image *image;

  if (pixels == NULL) {
    AP_SET_ERROR(AP_ERROR_INVALID_ARGUMENT, "Image pixels cannot be NULL");
    return NULL;
  }

  image = AP_ImageAlloc(width, height);
  if (image == NULL) {
    return NULL;
  }

  if (pitch <= 0) {
    pitch = width * 4;
  }

  AP_ImageCopyRect((const unsigned char *)pixels, pitch, 0, 0, image->pixels,
                   image->pitch, 0, 0, width, height);
  return image;
}

AP_Image *AP_LoadImage(const char *path) {
  int width = 0;
  int height = 0;
  int channels = 0;
  unsigned char *pixels;
  AP_Image *image;

  if (path == NULL) {
    AP_SET_ERROR(AP_ERROR_INVALID_ARGUMENT, "Image path cannot be NULL");
    return NULL;
  }

  char cwd[1024];
  GETCWD(cwd, 1024);
  pixels = stbi_load(path, &width, &height, &channels, 4);
  if (pixels == NULL) {
    AP_SET_ERROR(AP_ERROR_NOT_FOUND, "Failed to load image file");
    return NULL;
  }

  image = AP_CreateImageFromPixels(width, height, pixels, width * 4);
  stbi_image_free(pixels);
  return image;
}

AP_Image *AP_LoadImageFromMemory(const void *data, int size) {
  int width = 0;
  int height = 0;
  int channels = 0;
  unsigned char *pixels;
  AP_Image *image;

  if (data == NULL || size <= 0) {
    AP_SET_ERROR(AP_ERROR_INVALID_ARGUMENT, "Invalid image memory");
    return NULL;
  }

  pixels = stbi_load_from_memory((const stbi_uc *)data, size, &width, &height,
                                 &channels, 4);
  if (pixels == NULL) {
    AP_SET_ERROR(AP_ERROR_NOT_FOUND, "Failed to decode image memory");
    return NULL;
  }

  image = AP_CreateImageFromPixels(width, height, pixels, width * 4);
  stbi_image_free(pixels);
  return image;
}

AP_Image *AP_CopyImage(const AP_Image *image) {
  if (!AP_ImageIsValid(image)) {
    AP_SET_ERROR(AP_ERROR_INVALID_ARGUMENT, "Invalid image");
    return NULL;
  }

  return AP_CreateImageFromPixels(image->width, image->height, image->pixels,
                                  image->pitch);
}

void AP_DestroyImage(AP_Image *image) {
  if (image == NULL) {
    return;
  }

  free(image->pixels);
  free(image);
}

bool AP_ImageIsValid(const AP_Image *image) {
  return image != NULL && image->pixels != NULL && image->width > 0 &&
         image->height > 0;
}

bool AP_GetImageSize(const AP_Image *image, int *w, int *h) {
  if (!AP_ImageIsValid(image)) {
    AP_SET_ERROR(AP_ERROR_INVALID_ARGUMENT, "Invalid image");
    return false;
  }

  if (w != NULL) {
    *w = image->width;
  }
  if (h != NULL) {
    *h = image->height;
  }
  return true;
}

int AP_GetImageWidth(const AP_Image *image) {
  return AP_ImageIsValid(image) ? image->width : 0;
}

int AP_GetImageHeight(const AP_Image *image) {
  return AP_ImageIsValid(image) ? image->height : 0;
}

int AP_GetImagePitch(const AP_Image *image) {
  return AP_ImageIsValid(image) ? image->pitch : 0;
}

const void *AP_GetImagePixels(const AP_Image *image) {
  return AP_ImageIsValid(image) ? image->pixels : NULL;
}

void *AP_GetImagePixelsMutable(AP_Image *image) {
  return AP_ImageIsValid(image) ? image->pixels : NULL;
}

bool AP_GetImagePixel(const AP_Image *image, int x, int y, AP_Color8 *color) {
  const unsigned char *pixel;

  if (!AP_ImageIsValid(image) || color == NULL) {
    AP_SET_ERROR(AP_ERROR_INVALID_ARGUMENT, "Invalid image pixel read");
    return false;
  }

  if (x < 0 || y < 0 || x >= image->width || y >= image->height) {
    AP_SET_ERROR(AP_ERROR_INVALID_ARGUMENT, "Image pixel is out of bounds");
    return false;
  }

  pixel = AP_ImageAtConst(image, x, y);
  color->r = pixel[0];
  color->g = pixel[1];
  color->b = pixel[2];
  color->a = pixel[3];
  return true;
}

bool AP_SetImagePixel(AP_Image *image, int x, int y, AP_Color8 color) {
  unsigned char *pixel;

  if (!AP_ImageIsValid(image)) {
    AP_SET_ERROR(AP_ERROR_INVALID_ARGUMENT, "Invalid image");
    return false;
  }

  if (x < 0 || y < 0 || x >= image->width || y >= image->height) {
    AP_SET_ERROR(AP_ERROR_INVALID_ARGUMENT, "Image pixel is out of bounds");
    return false;
  }

  pixel = AP_ImageAt(image, x, y);
  pixel[0] = color.r;
  pixel[1] = color.g;
  pixel[2] = color.b;
  pixel[3] = color.a;
  return true;
}

bool AP_GetImagePixelFloat(const AP_Image *image, int x, int y,
                           AP_Color *color) {
  AP_Color8 byte_color;

  if (color == NULL) {
    AP_SET_ERROR(AP_ERROR_INVALID_ARGUMENT, "Image color cannot be NULL");
    return false;
  }

  if (!AP_GetImagePixel(image, x, y, &byte_color)) {
    return false;
  }

  color->r = (float)byte_color.r / 255.0f;
  color->g = (float)byte_color.g / 255.0f;
  color->b = (float)byte_color.b / 255.0f;
  color->a = (float)byte_color.a / 255.0f;
  return true;
}

bool AP_SetImagePixelFloat(AP_Image *image, int x, int y, AP_Color color) {
  AP_Color8 byte_color;

  byte_color.r = AP_ImageFloatToByte(color.r);
  byte_color.g = AP_ImageFloatToByte(color.g);
  byte_color.b = AP_ImageFloatToByte(color.b);
  byte_color.a = AP_ImageFloatToByte(color.a);
  return AP_SetImagePixel(image, x, y, byte_color);
}

bool AP_FillImage(AP_Image *image, AP_Color8 color) {
  return AP_FillImageRect(image, NULL, color);
}

bool AP_FillImageRect(AP_Image *image, const AP_Rect *rect, AP_Color8 color) {
  int x = 0;
  int y = 0;
  int width;
  int height;
  int row;
  int col;
  unsigned char *pixel;

  if (!AP_ImageIsValid(image)) {
    AP_SET_ERROR(AP_ERROR_INVALID_ARGUMENT, "Invalid image");
    return false;
  }

  width = image->width;
  height = image->height;
  if (rect != NULL) {
    x = rect->x;
    y = rect->y;
    width = rect->w;
    height = rect->h;
  }

  if (x < 0) {
    width += x;
    x = 0;
  }
  if (y < 0) {
    height += y;
    y = 0;
  }
  if (x + width > image->width) {
    width = image->width - x;
  }
  if (y + height > image->height) {
    height = image->height - y;
  }
  if (width <= 0 || height <= 0) {
    return true;
  }

  for (row = 0; row < height; ++row) {
    pixel = AP_ImageAt(image, x, y + row);
    for (col = 0; col < width; ++col) {
      pixel[0] = color.r;
      pixel[1] = color.g;
      pixel[2] = color.b;
      pixel[3] = color.a;
      pixel += 4;
    }
  }
  return true;
}

bool AP_BlitImage(const AP_Image *src, const AP_Rect *src_rect, AP_Image *dst,
                  int dst_x, int dst_y) {
  int src_x = 0;
  int src_y = 0;
  int width = 0;
  int height = 0;
  unsigned char *temp = NULL;
  const unsigned char *from;
  int from_pitch;

  if (!AP_ImageIsValid(src) || !AP_ImageIsValid(dst)) {
    AP_SET_ERROR(AP_ERROR_INVALID_ARGUMENT, "Invalid image blit");
    return false;
  }

  if (!AP_ImageClipBlit(src, src_rect, dst, &dst_x, &dst_y, &src_x, &src_y,
                        &width, &height)) {
    return false;
  }
  if (width <= 0 || height <= 0) {
    return true;
  }

  from = src->pixels;
  from_pitch = src->pitch;
  if (src == dst) {
    temp = (unsigned char *)malloc((size_t)src->pitch * (size_t)src->height);
    if (temp == NULL) {
      AP_SET_ERROR(AP_ERROR_OUT_OF_MEMORY, "Failed to blit overlapping image");
      return false;
    }
    memcpy(temp, src->pixels, (size_t)src->pitch * (size_t)src->height);
    from = temp;
  }

  AP_ImageCopyRect(from, from_pitch, src_x, src_y, dst->pixels, dst->pitch,
                   dst_x, dst_y, width, height);
  free(temp);
  return true;
}

bool AP_BlitImageBlend(const AP_Image *src, const AP_Rect *src_rect,
                       AP_Image *dst, int dst_x, int dst_y, AP_BlendMode blend) {
  int src_x = 0;
  int src_y = 0;
  int width = 0;
  int height = 0;
  int row;
  int col;
  unsigned char *temp = NULL;
  const unsigned char *from;
  int from_pitch;

  if (!AP_ImageIsValid(src) || !AP_ImageIsValid(dst)) {
    AP_SET_ERROR(AP_ERROR_INVALID_ARGUMENT, "Invalid image blit");
    return false;
  }

  if (blend == AP_BLENDMODE_NONE) {
    return AP_BlitImage(src, src_rect, dst, dst_x, dst_y);
  }

  if (!AP_ImageClipBlit(src, src_rect, dst, &dst_x, &dst_y, &src_x, &src_y,
                        &width, &height)) {
    return false;
  }
  if (width <= 0 || height <= 0) {
    return true;
  }

  from = src->pixels;
  from_pitch = src->pitch;
  if (src == dst) {
    temp = (unsigned char *)malloc((size_t)src->pitch * (size_t)src->height);
    if (temp == NULL) {
      AP_SET_ERROR(AP_ERROR_OUT_OF_MEMORY, "Failed to blit overlapping image");
      return false;
    }
    memcpy(temp, src->pixels, (size_t)src->pitch * (size_t)src->height);
    from = temp;
  }

  for (row = 0; row < height; ++row) {
    const unsigned char *src_row =
        from + (size_t)(src_y + row) * (size_t)from_pitch + (size_t)src_x * 4u;
    unsigned char *dst_row = AP_ImageAt(dst, dst_x, dst_y + row);
    for (col = 0; col < width; ++col) {
      AP_ImageBlendPixel(dst_row + col * 4, src_row + col * 4, blend);
    }
  }

  free(temp);
  return true;
}

bool AP_BlitImageScaled(const AP_Image *src, const AP_Rect *src_rect,
                        AP_Image *dst, const AP_Rect *dst_rect,
                        AP_ImageFilter filter) {
  AP_Rect source;
  AP_Rect dest;
  int x;
  int y;
  unsigned char sample[4];
  unsigned char *pixel;
  float u0;
  float v0;
  float u_span;
  float v_span;

  if (!AP_ImageIsValid(src) || !AP_ImageIsValid(dst)) {
    AP_SET_ERROR(AP_ERROR_INVALID_ARGUMENT, "Invalid scaled blit");
    return false;
  }

  source.x = 0;
  source.y = 0;
  source.w = src->width;
  source.h = src->height;
  if (src_rect != NULL) {
    source = *src_rect;
  }

  dest.x = 0;
  dest.y = 0;
  dest.w = dst->width;
  dest.h = dst->height;
  if (dst_rect != NULL) {
    dest = *dst_rect;
  }

  if (source.w <= 0 || source.h <= 0 || dest.w <= 0 || dest.h <= 0) {
    return true;
  }

  u0 = (float)source.x / (float)src->width;
  v0 = (float)source.y / (float)src->height;
  u_span = (float)source.w / (float)src->width;
  v_span = (float)source.h / (float)src->height;

  for (y = 0; y < dest.h; ++y) {
    int dy = dest.y + y;
    float v;
    if (dy < 0 || dy >= dst->height) {
      continue;
    }
    v = v0 + (((float)y + 0.5f) / (float)dest.h) * v_span;
    pixel = AP_ImageAt(dst, 0, dy);
    for (x = 0; x < dest.w; ++x) {
      int dx = dest.x + x;
      float u;
      if (dx < 0 || dx >= dst->width) {
        continue;
      }
      u = u0 + (((float)x + 0.5f) / (float)dest.w) * u_span;
      AP_ImageSample(src, u, v, filter, sample);
      pixel[dx * 4 + 0] = sample[0];
      pixel[dx * 4 + 1] = sample[1];
      pixel[dx * 4 + 2] = sample[2];
      pixel[dx * 4 + 3] = sample[3];
    }
  }
  return true;
}

bool AP_FlipImage(AP_Image *image, bool horizontal, bool vertical) {
  int y;
  unsigned char *row;
  unsigned char *temp_row;

  if (!AP_ImageIsValid(image)) {
    AP_SET_ERROR(AP_ERROR_INVALID_ARGUMENT, "Invalid image");
    return false;
  }

  if (!horizontal && !vertical) {
    return true;
  }

  temp_row = (unsigned char *)malloc((size_t)image->pitch);
  if (temp_row == NULL) {
    AP_SET_ERROR(AP_ERROR_OUT_OF_MEMORY, "Failed to flip image");
    return false;
  }

  if (vertical) {
    for (y = 0; y < image->height / 2; ++y) {
      unsigned char *top = AP_ImageAt(image, 0, y);
      unsigned char *bot = AP_ImageAt(image, 0, image->height - 1 - y);
      memcpy(temp_row, top, (size_t)image->pitch);
      memcpy(top, bot, (size_t)image->pitch);
      memcpy(bot, temp_row, (size_t)image->pitch);
    }
  }

  if (horizontal) {
    for (y = 0; y < image->height; ++y) {
      int x;
      row = AP_ImageAt(image, 0, y);
      for (x = 0; x < image->width / 2; ++x) {
        unsigned char *left = row + x * 4;
        unsigned char *right = row + (image->width - 1 - x) * 4;
        unsigned char tmp[4];
        memcpy(tmp, left, 4u);
        memcpy(left, right, 4u);
        memcpy(right, tmp, 4u);
      }
    }
  }

  free(temp_row);
  return true;
}

AP_Image *AP_RotateImage(const AP_Image *image, int degrees) {
  AP_Image *out;
  int x;
  int y;
  int turns;

  if (!AP_ImageIsValid(image)) {
    AP_SET_ERROR(AP_ERROR_INVALID_ARGUMENT, "Invalid image");
    return NULL;
  }

  turns = degrees % 360;
  if (turns < 0) {
    turns += 360;
  }
  if (turns % 90 != 0) {
    AP_SET_ERROR(AP_ERROR_INVALID_ARGUMENT,
                 "Image rotation must be a 90-degree step");
    return NULL;
  }
  if (turns == 0) {
    return AP_CopyImage(image);
  }

  if (turns == 180) {
    out = AP_CopyImage(image);
    if (out == NULL) {
      return NULL;
    }
    AP_FlipImage(out, true, true);
    return out;
  }

  out = AP_ImageAlloc(image->height, image->width);
  if (out == NULL) {
    return NULL;
  }

  for (y = 0; y < image->height; ++y) {
    for (x = 0; x < image->width; ++x) {
      const unsigned char *src = AP_ImageAtConst(image, x, y);
      unsigned char *dst;
      if (turns == 90) {
        dst = AP_ImageAt(out, image->height - 1 - y, x);
      } else {
        dst = AP_ImageAt(out, y, image->width - 1 - x);
      }
      memcpy(dst, src, 4u);
    }
  }
  return out;
}

AP_Image *AP_ScaleImage(const AP_Image *image, int width, int height,
                        AP_ImageFilter filter) {
  AP_Image *out;

  if (!AP_ImageIsValid(image)) {
    AP_SET_ERROR(AP_ERROR_INVALID_ARGUMENT, "Invalid image");
    return NULL;
  }

  out = AP_ImageAlloc(width, height);
  if (out == NULL) {
    return NULL;
  }

  if (!AP_BlitImageScaled(image, NULL, out, NULL, filter)) {
    AP_DestroyImage(out);
    return NULL;
  }
  return out;
}

AP_Image *AP_CropImage(const AP_Image *image, const AP_Rect *rect) {
  AP_Rect crop;
  AP_Image *out;

  if (!AP_ImageIsValid(image) || rect == NULL) {
    AP_SET_ERROR(AP_ERROR_INVALID_ARGUMENT, "Invalid image crop");
    return NULL;
  }

  crop = *rect;
  if (crop.x < 0) {
    crop.w += crop.x;
    crop.x = 0;
  }
  if (crop.y < 0) {
    crop.h += crop.y;
    crop.y = 0;
  }
  if (crop.x + crop.w > image->width) {
    crop.w = image->width - crop.x;
  }
  if (crop.y + crop.h > image->height) {
    crop.h = image->height - crop.y;
  }
  if (crop.w <= 0 || crop.h <= 0) {
    AP_SET_ERROR(AP_ERROR_INVALID_ARGUMENT, "Image crop rect is empty");
    return NULL;
  }

  out = AP_ImageAlloc(crop.w, crop.h);
  if (out == NULL) {
    return NULL;
  }

  AP_ImageCopyRect(image->pixels, image->pitch, crop.x, crop.y, out->pixels,
                   out->pitch, 0, 0, crop.w, crop.h);
  return out;
}

bool AP_ModulateImage(AP_Image *image, AP_Color8 color) {
  int i;
  int count;
  unsigned char *pixel;

  if (!AP_ImageIsValid(image)) {
    AP_SET_ERROR(AP_ERROR_INVALID_ARGUMENT, "Invalid image");
    return false;
  }

  count = image->width * image->height;
  pixel = image->pixels;
  for (i = 0; i < count; ++i) {
    pixel[0] = (AP_U8)(((unsigned)pixel[0] * (unsigned)color.r) / 255u);
    pixel[1] = (AP_U8)(((unsigned)pixel[1] * (unsigned)color.g) / 255u);
    pixel[2] = (AP_U8)(((unsigned)pixel[2] * (unsigned)color.b) / 255u);
    pixel[3] = (AP_U8)(((unsigned)pixel[3] * (unsigned)color.a) / 255u);
    pixel += 4;
  }
  return true;
}

bool AP_ImageGrayscale(AP_Image *image) {
  int i;
  int count;
  unsigned char *pixel;

  if (!AP_ImageIsValid(image)) {
    AP_SET_ERROR(AP_ERROR_INVALID_ARGUMENT, "Invalid image");
    return false;
  }

  count = image->width * image->height;
  pixel = image->pixels;
  for (i = 0; i < count; ++i) {
    unsigned luma =
        (77u * pixel[0] + 150u * pixel[1] + 29u * pixel[2]) / 256u;
    AP_U8 gray = AP_ImageClampByte((int)luma);
    pixel[0] = gray;
    pixel[1] = gray;
    pixel[2] = gray;
    pixel += 4;
  }
  return true;
}

bool AP_ImageInvert(AP_Image *image) {
  int i;
  int count;
  unsigned char *pixel;

  if (!AP_ImageIsValid(image)) {
    AP_SET_ERROR(AP_ERROR_INVALID_ARGUMENT, "Invalid image");
    return false;
  }

  count = image->width * image->height;
  pixel = image->pixels;
  for (i = 0; i < count; ++i) {
    pixel[0] = (AP_U8)(255u - pixel[0]);
    pixel[1] = (AP_U8)(255u - pixel[1]);
    pixel[2] = (AP_U8)(255u - pixel[2]);
    pixel += 4;
  }
  return true;
}

bool AP_ImagePremultiply(AP_Image *image) {
  int i;
  int count;
  unsigned char *pixel;

  if (!AP_ImageIsValid(image)) {
    AP_SET_ERROR(AP_ERROR_INVALID_ARGUMENT, "Invalid image");
    return false;
  }

  count = image->width * image->height;
  pixel = image->pixels;
  for (i = 0; i < count; ++i) {
    unsigned a = pixel[3];
    pixel[0] = (AP_U8)(((unsigned)pixel[0] * a) / 255u);
    pixel[1] = (AP_U8)(((unsigned)pixel[1] * a) / 255u);
    pixel[2] = (AP_U8)(((unsigned)pixel[2] * a) / 255u);
    pixel += 4;
  }
  return true;
}

bool AP_ReplaceImageColor(AP_Image *image, AP_Color8 from, AP_Color8 to) {
  int i;
  int count;
  unsigned char *pixel;

  if (!AP_ImageIsValid(image)) {
    AP_SET_ERROR(AP_ERROR_INVALID_ARGUMENT, "Invalid image");
    return false;
  }

  count = image->width * image->height;
  pixel = image->pixels;
  for (i = 0; i < count; ++i) {
    if (pixel[0] == from.r && pixel[1] == from.g && pixel[2] == from.b &&
        pixel[3] == from.a) {
      pixel[0] = to.r;
      pixel[1] = to.g;
      pixel[2] = to.b;
      pixel[3] = to.a;
    }
    pixel += 4;
  }
  return true;
}

bool AP_MapImageColorKey(AP_Image *image, AP_Color8 key) {
  int i;
  int count;
  unsigned char *pixel;

  if (!AP_ImageIsValid(image)) {
    AP_SET_ERROR(AP_ERROR_INVALID_ARGUMENT, "Invalid image");
    return false;
  }

  count = image->width * image->height;
  pixel = image->pixels;
  for (i = 0; i < count; ++i) {
    if (pixel[0] == key.r && pixel[1] == key.g && pixel[2] == key.b) {
      pixel[3] = 0;
    }
    pixel += 4;
  }
  return true;
}

bool AP_SaveImagePNG(const AP_Image *image, const char *path) {
  if (!AP_ImageIsValid(image) || path == NULL) {
    AP_SET_ERROR(AP_ERROR_INVALID_ARGUMENT, "Invalid image save");
    return false;
  }

  if (stbi_write_png(path, image->width, image->height, 4, image->pixels,
                     image->pitch) == 0) {
    AP_SET_ERROR(AP_ERROR_OPERATION_FAILED, "Failed to write PNG image");
    return false;
  }
  return true;
}

bool AP_SaveImageBMP(const AP_Image *image, const char *path) {
  if (!AP_ImageIsValid(image) || path == NULL) {
    AP_SET_ERROR(AP_ERROR_INVALID_ARGUMENT, "Invalid image save");
    return false;
  }

  if (stbi_write_bmp(path, image->width, image->height, 4, image->pixels) ==
      0) {
    AP_SET_ERROR(AP_ERROR_OPERATION_FAILED, "Failed to write BMP image");
    return false;
  }
  return true;
}

bool AP_SaveImageJPG(const AP_Image *image, const char *path, int quality) {
  if (!AP_ImageIsValid(image) || path == NULL) {
    AP_SET_ERROR(AP_ERROR_INVALID_ARGUMENT, "Invalid image save");
    return false;
  }

  if (quality < 1) {
    quality = 1;
  }
  if (quality > 100) {
    quality = 100;
  }

  if (stbi_write_jpg(path, image->width, image->height, 4, image->pixels,
                     quality) == 0) {
    AP_SET_ERROR(AP_ERROR_OPERATION_FAILED, "Failed to write JPEG image");
    return false;
  }
  return true;
}

bool AP_SaveImage(const AP_Image *image, const char *path) {
  const char *ext;

  if (!AP_ImageIsValid(image) || path == NULL) {
    AP_SET_ERROR(AP_ERROR_INVALID_ARGUMENT, "Invalid image save");
    return false;
  }

  ext = AP_ImageExtension(path);
  if (AP_ImageExtEquals(ext, ".bmp")) {
    return AP_SaveImageBMP(image, path);
  }
  if (AP_ImageExtEquals(ext, ".jpg") || AP_ImageExtEquals(ext, ".jpeg")) {
    return AP_SaveImageJPG(image, path, 90);
  }
  return AP_SaveImagePNG(image, path);
}
