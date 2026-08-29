/*
 * AP2 — Application Primitives
 * Copyright (c) 2026 Jack Waechter
 *
 * Licensed under the MIT License.
 * See LICENSE in the project root for full terms.
 */

#include "AP2/AP2_Font.h"

#include "AP2_Internal.h"

#include "AP2/AP2_Error.h"
#include "AP2/AP2_Renderer.h"
#include "AP2/AP2_Texture.h"

#define STB_TRUETYPE_IMPLEMENTATION
#define STBTT_STATIC
#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-function"
#endif
#include "stb_truetype.h"
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define AP_FONT_FIRST 32
#define AP_FONT_COUNT 95
#define AP_FONT_LAST (AP_FONT_FIRST + AP_FONT_COUNT - 1)
#define AP_FONT_GLYPH 8
#define AP_FONT_PAD 1
#define AP_FONT_CELL (AP_FONT_GLYPH + AP_FONT_PAD * 2)
#define AP_FONT_COLS 16

typedef struct AP_FontGlyph {
  AP_FRect src;
  float x_advance;
  float x_off;
  float y_off;
  float width;
  float height;
} AP_FontGlyph;

struct AP_Font {
  AP_Texture *atlas;
  float size;
  float ascent;
  float descent;
  float line_height;
  bool builtin;
  AP_FontGlyph glyphs[AP_FONT_COUNT];
};

static AP_Font *g_default_font = NULL;
static AP_Font *g_current_font = NULL;

/*
 * Public-domain 8x8 VGA font (U+0020..U+007E).
 * Daniel Hepper / IBM VGA, font8x8.
 */
static const unsigned char AP_FONT_BITMAP[AP_FONT_COUNT][AP_FONT_GLYPH] = {
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, {0x18, 0x3C, 0x3C, 0x18, 0x18, 0x00, 0x18, 0x00},
    {0x36, 0x36, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, {0x36, 0x36, 0x7F, 0x36, 0x7F, 0x36, 0x36, 0x00},
    {0x0C, 0x3E, 0x03, 0x1E, 0x30, 0x1F, 0x0C, 0x00}, {0x00, 0x63, 0x33, 0x18, 0x0C, 0x66, 0x63, 0x00},
    {0x1C, 0x36, 0x1C, 0x6E, 0x3B, 0x33, 0x6E, 0x00}, {0x06, 0x06, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00},
    {0x18, 0x0C, 0x06, 0x06, 0x06, 0x0C, 0x18, 0x00}, {0x06, 0x0C, 0x18, 0x18, 0x18, 0x0C, 0x06, 0x00},
    {0x00, 0x66, 0x3C, 0xFF, 0x3C, 0x66, 0x00, 0x00}, {0x00, 0x0C, 0x0C, 0x3F, 0x0C, 0x0C, 0x00, 0x00},
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x0C, 0x0C, 0x06}, {0x00, 0x00, 0x00, 0x3F, 0x00, 0x00, 0x00, 0x00},
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x0C, 0x0C, 0x00}, {0x60, 0x30, 0x18, 0x0C, 0x06, 0x03, 0x01, 0x00},
    {0x3E, 0x63, 0x73, 0x7B, 0x6F, 0x67, 0x3E, 0x00}, {0x0C, 0x0E, 0x0C, 0x0C, 0x0C, 0x0C, 0x3F, 0x00},
    {0x1E, 0x33, 0x30, 0x1C, 0x06, 0x33, 0x3F, 0x00}, {0x1E, 0x33, 0x30, 0x1C, 0x30, 0x33, 0x1E, 0x00},
    {0x38, 0x3C, 0x36, 0x33, 0x7F, 0x30, 0x78, 0x00}, {0x3F, 0x03, 0x1F, 0x30, 0x30, 0x33, 0x1E, 0x00},
    {0x1C, 0x06, 0x03, 0x1F, 0x33, 0x33, 0x1E, 0x00}, {0x3F, 0x33, 0x30, 0x18, 0x0C, 0x0C, 0x0C, 0x00},
    {0x1E, 0x33, 0x33, 0x1E, 0x33, 0x33, 0x1E, 0x00}, {0x1E, 0x33, 0x33, 0x3E, 0x30, 0x18, 0x0E, 0x00},
    {0x00, 0x0C, 0x0C, 0x00, 0x00, 0x0C, 0x0C, 0x00}, {0x00, 0x0C, 0x0C, 0x00, 0x00, 0x0C, 0x0C, 0x06},
    {0x18, 0x0C, 0x06, 0x03, 0x06, 0x0C, 0x18, 0x00}, {0x00, 0x00, 0x3F, 0x00, 0x00, 0x3F, 0x00, 0x00},
    {0x06, 0x0C, 0x18, 0x30, 0x18, 0x0C, 0x06, 0x00}, {0x1E, 0x33, 0x30, 0x18, 0x0C, 0x00, 0x0C, 0x00},
    {0x3E, 0x63, 0x7B, 0x7B, 0x7B, 0x03, 0x1E, 0x00}, {0x0C, 0x1E, 0x33, 0x33, 0x3F, 0x33, 0x33, 0x00},
    {0x3F, 0x66, 0x66, 0x3E, 0x66, 0x66, 0x3F, 0x00}, {0x3C, 0x66, 0x03, 0x03, 0x03, 0x66, 0x3C, 0x00},
    {0x1F, 0x36, 0x66, 0x66, 0x66, 0x36, 0x1F, 0x00}, {0x7F, 0x46, 0x16, 0x1E, 0x16, 0x46, 0x7F, 0x00},
    {0x7F, 0x46, 0x16, 0x1E, 0x16, 0x06, 0x0F, 0x00}, {0x3C, 0x66, 0x03, 0x03, 0x73, 0x66, 0x7C, 0x00},
    {0x33, 0x33, 0x33, 0x3F, 0x33, 0x33, 0x33, 0x00}, {0x1E, 0x0C, 0x0C, 0x0C, 0x0C, 0x0C, 0x1E, 0x00},
    {0x78, 0x30, 0x30, 0x30, 0x33, 0x33, 0x1E, 0x00}, {0x67, 0x66, 0x36, 0x1E, 0x36, 0x66, 0x67, 0x00},
    {0x0F, 0x06, 0x06, 0x06, 0x46, 0x66, 0x7F, 0x00}, {0x63, 0x77, 0x7F, 0x7F, 0x6B, 0x63, 0x63, 0x00},
    {0x63, 0x67, 0x6F, 0x7B, 0x73, 0x63, 0x63, 0x00}, {0x1C, 0x36, 0x63, 0x63, 0x63, 0x36, 0x1C, 0x00},
    {0x3F, 0x66, 0x66, 0x3E, 0x06, 0x06, 0x0F, 0x00}, {0x1E, 0x33, 0x33, 0x33, 0x3B, 0x1E, 0x38, 0x00},
    {0x3F, 0x66, 0x66, 0x3E, 0x36, 0x66, 0x67, 0x00}, {0x1E, 0x33, 0x07, 0x0E, 0x38, 0x33, 0x1E, 0x00},
    {0x3F, 0x2D, 0x0C, 0x0C, 0x0C, 0x0C, 0x1E, 0x00}, {0x33, 0x33, 0x33, 0x33, 0x33, 0x33, 0x3F, 0x00},
    {0x33, 0x33, 0x33, 0x33, 0x33, 0x1E, 0x0C, 0x00}, {0x63, 0x63, 0x63, 0x6B, 0x7F, 0x77, 0x63, 0x00},
    {0x63, 0x63, 0x36, 0x1C, 0x1C, 0x36, 0x63, 0x00}, {0x33, 0x33, 0x33, 0x1E, 0x0C, 0x0C, 0x1E, 0x00},
    {0x7F, 0x63, 0x31, 0x18, 0x4C, 0x66, 0x7F, 0x00}, {0x1E, 0x06, 0x06, 0x06, 0x06, 0x06, 0x1E, 0x00},
    {0x03, 0x06, 0x0C, 0x18, 0x30, 0x60, 0x40, 0x00}, {0x1E, 0x18, 0x18, 0x18, 0x18, 0x18, 0x1E, 0x00},
    {0x08, 0x1C, 0x36, 0x63, 0x00, 0x00, 0x00, 0x00}, {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xFF},
    {0x0C, 0x0C, 0x18, 0x00, 0x00, 0x00, 0x00, 0x00}, {0x00, 0x00, 0x1E, 0x30, 0x3E, 0x33, 0x6E, 0x00},
    {0x07, 0x06, 0x06, 0x3E, 0x66, 0x66, 0x3B, 0x00}, {0x00, 0x00, 0x1E, 0x33, 0x03, 0x33, 0x1E, 0x00},
    {0x38, 0x30, 0x30, 0x3E, 0x33, 0x33, 0x6E, 0x00}, {0x00, 0x00, 0x1E, 0x33, 0x3F, 0x03, 0x1E, 0x00},
    {0x1C, 0x36, 0x06, 0x0F, 0x06, 0x06, 0x0F, 0x00}, {0x00, 0x00, 0x6E, 0x33, 0x33, 0x3E, 0x30, 0x1F},
    {0x07, 0x06, 0x36, 0x6E, 0x66, 0x66, 0x67, 0x00}, {0x0C, 0x00, 0x0E, 0x0C, 0x0C, 0x0C, 0x1E, 0x00},
    {0x30, 0x00, 0x30, 0x30, 0x30, 0x33, 0x33, 0x1E}, {0x07, 0x06, 0x66, 0x36, 0x1E, 0x36, 0x67, 0x00},
    {0x0E, 0x0C, 0x0C, 0x0C, 0x0C, 0x0C, 0x1E, 0x00}, {0x00, 0x00, 0x33, 0x7F, 0x7F, 0x6B, 0x63, 0x00},
    {0x00, 0x00, 0x1F, 0x33, 0x33, 0x33, 0x33, 0x00}, {0x00, 0x00, 0x1E, 0x33, 0x33, 0x33, 0x1E, 0x00},
    {0x00, 0x00, 0x3B, 0x66, 0x66, 0x3E, 0x06, 0x0F}, {0x00, 0x00, 0x6E, 0x33, 0x33, 0x3E, 0x30, 0x78},
    {0x00, 0x00, 0x3B, 0x6E, 0x66, 0x06, 0x0F, 0x00}, {0x00, 0x00, 0x3E, 0x03, 0x1E, 0x30, 0x1F, 0x00},
    {0x08, 0x0C, 0x3E, 0x0C, 0x0C, 0x2C, 0x18, 0x00}, {0x00, 0x00, 0x33, 0x33, 0x33, 0x33, 0x6E, 0x00},
    {0x00, 0x00, 0x33, 0x33, 0x33, 0x1E, 0x0C, 0x00}, {0x00, 0x00, 0x63, 0x6B, 0x7F, 0x7F, 0x36, 0x00},
    {0x00, 0x00, 0x63, 0x36, 0x1C, 0x36, 0x63, 0x00}, {0x00, 0x00, 0x33, 0x33, 0x33, 0x3E, 0x30, 0x1F},
    {0x00, 0x00, 0x3F, 0x19, 0x0C, 0x26, 0x3F, 0x00}, {0x38, 0x0C, 0x0C, 0x07, 0x0C, 0x0C, 0x38, 0x00},
    {0x18, 0x18, 0x18, 0x00, 0x18, 0x18, 0x18, 0x00}, {0x07, 0x0C, 0x0C, 0x38, 0x0C, 0x0C, 0x07, 0x00},
    {0x6E, 0x3B, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}};

static AP_FColor AP_FontDrawColor(void) {
  AP_FColor color;
  if (!AP_GetRenderDrawColorFloat(&color.r, &color.g, &color.b, &color.a)) {
    color.r = 1.0f;
    color.g = 1.0f;
    color.b = 1.0f;
    color.a = 1.0f;
  }
  return color;
}

static unsigned char *AP_FontLoadFile(const char *path, int *size) {
  FILE *file;
  long length;
  unsigned char *data;

  file = fopen(path, "rb");
  if (file == NULL) {
    AP_SET_ERROR(AP_ERROR_NOT_FOUND, "Font file could not be opened");
    return NULL;
  }

  if (fseek(file, 0, SEEK_END) != 0) {
    fclose(file);
    AP_SET_ERROR(AP_ERROR_OPERATION_FAILED, "Font file could not be read");
    return NULL;
  }

  length = ftell(file);
  if (length <= 0) {
    fclose(file);
    AP_SET_ERROR(AP_ERROR_OPERATION_FAILED, "Font file is empty");
    return NULL;
  }

  rewind(file);
  data = (unsigned char *)malloc((size_t)length);
  if (data == NULL) {
    fclose(file);
    AP_SET_ERROR(AP_ERROR_OUT_OF_MEMORY, "Failed to allocate font file");
    return NULL;
  }

  if (fread(data, 1, (size_t)length, file) != (size_t)length) {
    free(data);
    fclose(file);
    AP_SET_ERROR(AP_ERROR_OPERATION_FAILED, "Font file could not be read");
    return NULL;
  }

  fclose(file);
  *size = (int)length;
  return data;
}

static void AP_FontDestroy(AP_Font *font) {
  if (font == NULL) {
    return;
  }

  AP_DestroyTexture(font->atlas);
  free(font);
}

static AP_Font *AP_FontBakeBitmap(void) {
  const int atlas_w = AP_FONT_COLS * AP_FONT_CELL;
  const int atlas_h = 6 * AP_FONT_CELL;
  unsigned char *pixels;
  AP_Font *font;
  int glyph;

  pixels = (unsigned char *)calloc((size_t)atlas_w * (size_t)atlas_h * 4, 1);
  if (pixels == NULL) {
    AP_SET_ERROR(AP_ERROR_OUT_OF_MEMORY, "Failed to allocate font atlas");
    return NULL;
  }

  font = (AP_Font *)calloc(1, sizeof(AP_Font));
  if (font == NULL) {
    free(pixels);
    AP_SET_ERROR(AP_ERROR_OUT_OF_MEMORY, "Failed to allocate font");
    return NULL;
  }

  for (glyph = 0; glyph < AP_FONT_COUNT; ++glyph) {
    int col = glyph % AP_FONT_COLS;
    int row = glyph / AP_FONT_COLS;
    int origin_x = col * AP_FONT_CELL + AP_FONT_PAD;
    int origin_y = row * AP_FONT_CELL + AP_FONT_PAD;
    int y;
    int x;

    for (y = 0; y < AP_FONT_GLYPH; ++y) {
      unsigned char bits = AP_FONT_BITMAP[glyph][y];
      for (x = 0; x < AP_FONT_GLYPH; ++x) {
        if ((bits & (1u << x)) != 0) {
          int index = ((origin_y + y) * atlas_w + (origin_x + x)) * 4;
          pixels[index + 0] = 255;
          pixels[index + 1] = 255;
          pixels[index + 2] = 255;
          pixels[index + 3] = 255;
        }
      }
    }

    font->glyphs[glyph].src.x = (float)origin_x;
    font->glyphs[glyph].src.y = (float)origin_y;
    font->glyphs[glyph].src.w = (float)AP_FONT_GLYPH;
    font->glyphs[glyph].src.h = (float)AP_FONT_GLYPH;
    font->glyphs[glyph].x_advance = (float)AP_FONT_GLYPH;
    font->glyphs[glyph].x_off = 0.0f;
    font->glyphs[glyph].y_off = 0.0f;
    font->glyphs[glyph].width = (float)AP_FONT_GLYPH;
    font->glyphs[glyph].height = (float)AP_FONT_GLYPH;
  }

  font->atlas = AP_CreateTextureFromPixels(atlas_w, atlas_h, pixels, atlas_w * 4);
  free(pixels);
  if (font->atlas == NULL) {
    free(font);
    return NULL;
  }

  AP_SetTextureScaleMode(font->atlas, AP_SCALEMODE_NEAREST);
  AP_SetTextureBlendMode(font->atlas, AP_BLENDMODE_BLEND);
  font->size = (float)AP_FONT_GLYPH;
  font->ascent = (float)AP_FONT_GLYPH;
  font->descent = 0.0f;
  font->line_height = (float)AP_FONT_GLYPH + 2.0f;
  font->builtin = true;
  return font;
}

static AP_Font *AP_FontBakeTTF(const unsigned char *data, int data_size,
                               float pixel_size) {
  const int atlas_w = 512;
  const int atlas_h = 512;
  unsigned char *bitmap;
  unsigned char *rgba;
  stbtt_bakedchar packed[AP_FONT_COUNT];
  AP_Font *font;
  int glyph;
  int baked;

  if (pixel_size < 6.0f) {
    pixel_size = 6.0f;
  }

  bitmap = (unsigned char *)calloc((size_t)atlas_w * (size_t)atlas_h, 1);
  if (bitmap == NULL) {
    AP_SET_ERROR(AP_ERROR_OUT_OF_MEMORY, "Failed to allocate font atlas");
    return NULL;
  }

  baked = stbtt_BakeFontBitmap(data, 0, pixel_size, bitmap, atlas_w, atlas_h,
                               AP_FONT_FIRST, AP_FONT_COUNT, packed);
  if (baked <= 0) {
    free(bitmap);
    AP_SET_ERROR(AP_ERROR_OPERATION_FAILED, "Failed to bake TrueType font");
    return NULL;
  }

  rgba = (unsigned char *)malloc((size_t)atlas_w * (size_t)atlas_h * 4);
  if (rgba == NULL) {
    free(bitmap);
    AP_SET_ERROR(AP_ERROR_OUT_OF_MEMORY, "Failed to allocate font atlas");
    return NULL;
  }

  for (glyph = 0; glyph < atlas_w * atlas_h; ++glyph) {
    unsigned char alpha = bitmap[glyph];
    rgba[glyph * 4 + 0] = 255;
    rgba[glyph * 4 + 1] = 255;
    rgba[glyph * 4 + 2] = 255;
    rgba[glyph * 4 + 3] = alpha;
  }
  free(bitmap);

  font = (AP_Font *)calloc(1, sizeof(AP_Font));
  if (font == NULL) {
    free(rgba);
    AP_SET_ERROR(AP_ERROR_OUT_OF_MEMORY, "Failed to allocate font");
    return NULL;
  }

  font->atlas = AP_CreateTextureFromPixels(atlas_w, atlas_h, rgba, atlas_w * 4);
  free(rgba);
  if (font->atlas == NULL) {
    free(font);
    return NULL;
  }

  AP_SetTextureScaleMode(font->atlas, AP_SCALEMODE_LINEAR);
  AP_SetTextureBlendMode(font->atlas, AP_BLENDMODE_BLEND);
  font->size = pixel_size;
  font->ascent = pixel_size;
  font->descent = 0.0f;
  font->line_height = pixel_size * 1.25f;
  font->builtin = false;

  for (glyph = 0; glyph < AP_FONT_COUNT; ++glyph) {
    float x0 = (float)packed[glyph].x0;
    float y0 = (float)packed[glyph].y0;
    float x1 = (float)packed[glyph].x1;
    float y1 = (float)packed[glyph].y1;
    font->glyphs[glyph].src.x = x0;
    font->glyphs[glyph].src.y = y0;
    font->glyphs[glyph].src.w = x1 - x0;
    font->glyphs[glyph].src.h = y1 - y0;
    font->glyphs[glyph].x_advance = packed[glyph].xadvance;
    font->glyphs[glyph].x_off = packed[glyph].xoff;
    font->glyphs[glyph].y_off = packed[glyph].yoff;
    font->glyphs[glyph].width = x1 - x0;
    font->glyphs[glyph].height = y1 - y0;
  }

  (void)data_size;
  return font;
}

static AP_Font *AP_FontActive(AP_Font *font) {
  if (font != NULL) {
    return font;
  }

  if (g_current_font != NULL) {
    return g_current_font;
  }

  return AP_GetDefaultFont();
}

static int AP_FontGlyphIndex(int codepoint) {
  if (codepoint < AP_FONT_FIRST || codepoint > AP_FONT_LAST) {
    codepoint = '?';
  }

  return codepoint - AP_FONT_FIRST;
}

static bool AP_FontDrawGlyph(AP_Font *font, float x, float y,
                             const AP_FontGlyph *glyph, AP_FColor color,
                             float scale) {
  AP_FRect src = glyph->src;
  AP_FRect dst;
  float red;
  float green;
  float blue;
  float alpha;
  bool ok;

  if (glyph->width <= 0.0f || glyph->height <= 0.0f) {
    return true;
  }

  dst.x = x + glyph->x_off * scale;
  dst.y = y + glyph->y_off * scale;
  dst.w = glyph->width * scale;
  dst.h = glyph->height * scale;

  if (!AP_GetTextureColorModFloat(font->atlas, &red, &green, &blue) ||
      !AP_GetTextureAlphaModFloat(font->atlas, &alpha)) {
    return false;
  }

  if (!AP_SetTextureColorModFloat(font->atlas, color.r * red, color.g * green,
                                  color.b * blue) ||
      !AP_SetTextureAlphaModFloat(font->atlas, color.a * alpha)) {
    return false;
  }

  ok = AP_RenderTexture(font->atlas, &src, &dst);
  AP_SetTextureColorModFloat(font->atlas, red, green, blue);
  AP_SetTextureAlphaModFloat(font->atlas, alpha);
  return ok;
}

bool AP_FontInit(void) {
  if (g_default_font != NULL) {
    return true;
  }

  g_default_font = AP_FontBakeBitmap();
  if (g_default_font == NULL) {
    return false;
  }

  if (g_current_font == NULL) {
    g_current_font = g_default_font;
  }

  return true;
}

void AP_FontShutdown(void) {
  g_current_font = NULL;
  AP_FontDestroy(g_default_font);
  g_default_font = NULL;
}

AP_Font *AP_GetDefaultFont(void) {
  if (!AP_FontInit()) {
    return NULL;
  }

  return g_default_font;
}

AP_Font *AP_CreateFontFromMemory(const void *data, int data_size,
                                 float pixel_size) {
  if (data == NULL || data_size <= 0) {
    AP_SET_ERROR(AP_ERROR_INVALID_ARGUMENT, "Font data cannot be NULL");
    return NULL;
  }

  return AP_FontBakeTTF((const unsigned char *)data, data_size, pixel_size);
}

AP_Font *AP_LoadFont(const char *path, float pixel_size) {
  unsigned char *data;
  int size = 0;
  AP_Font *font;

  if (path == NULL) {
    AP_SET_ERROR(AP_ERROR_INVALID_ARGUMENT, "Font path cannot be NULL");
    return NULL;
  }

  data = AP_FontLoadFile(path, &size);
  if (data == NULL) {
    return NULL;
  }

  font = AP_FontBakeTTF(data, size, pixel_size);
  free(data);
  return font;
}

void AP_DestroyFont(AP_Font *font) {
  if (font == NULL) {
    return;
  }

  if (font == g_default_font) {
    AP_SET_ERROR(AP_ERROR_INVALID_ARGUMENT, "Cannot destroy the default font");
    return;
  }

  if (g_current_font == font) {
    g_current_font = g_default_font;
  }

  AP_FontDestroy(font);
}

bool AP_FontIsValid(const AP_Font *font) {
  return font != NULL && AP_TextureIsValid(font->atlas);
}

float AP_GetFontSize(const AP_Font *font) {
  font = AP_FontActive((AP_Font *)font);
  return font != NULL ? font->size : 0.0f;
}

float AP_GetFontLineHeight(const AP_Font *font) {
  font = AP_FontActive((AP_Font *)font);
  return font != NULL ? font->line_height : 0.0f;
}

bool AP_SetFont(AP_Font *font) {
  if (font == NULL) {
    g_current_font = AP_GetDefaultFont();
    return g_current_font != NULL;
  }

  if (!AP_FontIsValid(font)) {
    AP_SET_ERROR(AP_ERROR_INVALID_ARGUMENT, "Font is not valid");
    return false;
  }

  g_current_font = font;
  return true;
}

AP_Font *AP_GetFont(void) { return AP_FontActive(NULL); }

AP_FPoint AP_MeasureTextEx(AP_Font *font, const char *text, float size) {
  AP_FPoint size_px;
  float scale;
  float line_width = 0.0f;
  float max_width = 0.0f;
  float height;
  const char *cursor;

  size_px.x = 0.0f;
  size_px.y = 0.0f;
  font = AP_FontActive(font);
  if (font == NULL || text == NULL) {
    return size_px;
  }

  if (size <= 0.0f) {
    size = font->size;
  }

  scale = size / font->size;
  height = font->line_height * scale;

  for (cursor = text; *cursor != '\0'; ++cursor) {
    unsigned char ch = (unsigned char)*cursor;
    if (ch == '\n') {
      if (line_width > max_width) {
        max_width = line_width;
      }
      line_width = 0.0f;
      height += font->line_height * scale;
      continue;
    }

    if (ch == '\t') {
      line_width += font->glyphs[0].x_advance * scale * 4.0f;
      continue;
    }

    line_width += font->glyphs[AP_FontGlyphIndex(ch)].x_advance * scale;
  }

  if (line_width > max_width) {
    max_width = line_width;
  }

  size_px.x = max_width;
  size_px.y = height;
  return size_px;
}

AP_FPoint AP_MeasureText(const char *text) {
  return AP_MeasureTextEx(NULL, text, 0.0f);
}

bool AP_RenderTextEx(AP_Font *font, float x, float y, const char *text,
                     AP_FColor color, float size) {
  float scale;
  float cursor_x;
  float cursor_y;
  const char *cursor;

  font = AP_FontActive(font);
  if (font == NULL || text == NULL) {
    AP_SET_ERROR(AP_ERROR_INVALID_ARGUMENT, "Text cannot be drawn");
    return false;
  }

  if (size <= 0.0f) {
    size = font->size;
  }

  scale = size / font->size;
  cursor_x = x;
  cursor_y = y;

  for (cursor = text; *cursor != '\0'; ++cursor) {
    unsigned char ch = (unsigned char)*cursor;
    const AP_FontGlyph *glyph;

    if (ch == '\n') {
      cursor_x = x;
      cursor_y += font->line_height * scale;
      continue;
    }

    if (ch == '\t') {
      cursor_x += font->glyphs[0].x_advance * scale * 4.0f;
      continue;
    }

    glyph = &font->glyphs[AP_FontGlyphIndex(ch)];
    if (!AP_FontDrawGlyph(font, cursor_x, cursor_y, glyph, color, scale)) {
      return false;
    }

    cursor_x += glyph->x_advance * scale;
  }

  return true;
}

bool AP_RenderText(float x, float y, const char *text) {
  return AP_RenderTextEx(NULL, x, y, text, AP_FontDrawColor(), 0.0f);
}

bool AP_RenderTextAligned(AP_Font *font, const AP_FRect *bounds,
                          const char *text, AP_FColor color, float size,
                          AP_TextAlign align) {
  AP_FPoint measured;
  float x;
  float y;

  if (bounds == NULL) {
    AP_SET_ERROR(AP_ERROR_INVALID_ARGUMENT, "Text bounds cannot be NULL");
    return false;
  }

  font = AP_FontActive(font);
  measured = AP_MeasureTextEx(font, text, size);
  x = bounds->x;
  y = bounds->y + (bounds->h - measured.y) * 0.5f;

  if (align == AP_TEXT_ALIGN_CENTER) {
    x = bounds->x + (bounds->w - measured.x) * 0.5f;
  } else if (align == AP_TEXT_ALIGN_RIGHT) {
    x = bounds->x + bounds->w - measured.x;
  }

  return AP_RenderTextEx(font, x, y, text, color, size);
}
