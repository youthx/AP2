/*
 * AP2 — Application Primitives
 * Copyright (c) 2024-2026 Jack Waechter
 *
 * Licensed under the MIT License.
 * See LICENSE in the project root for full terms.
 */

#include "AP2/AP2_Font.h"

#include "AP2_Internal.h"

#include "AP2/AP2_Error.h"
#include "AP2/AP2_Platform.h"
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

typedef struct AP_FontGlyph
{
  AP_FRect src;
  float x_advance;
  float x_off;
  float y_off;
  float width;
  float height;
} AP_FontGlyph;

struct AP_Font
{
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
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, {0x18, 0x3C, 0x3C, 0x18, 0x18, 0x00, 0x18, 0x00}, {0x36, 0x36, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, {0x36, 0x36, 0x7F, 0x36, 0x7F, 0x36, 0x36, 0x00}, {0x0C, 0x3E, 0x03, 0x1E, 0x30, 0x1F, 0x0C, 0x00}, {0x00, 0x63, 0x33, 0x18, 0x0C, 0x66, 0x63, 0x00}, {0x1C, 0x36, 0x1C, 0x6E, 0x3B, 0x33, 0x6E, 0x00}, {0x06, 0x06, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00}, {0x18, 0x0C, 0x06, 0x06, 0x06, 0x0C, 0x18, 0x00}, {0x06, 0x0C, 0x18, 0x18, 0x18, 0x0C, 0x06, 0x00}, {0x00, 0x66, 0x3C, 0xFF, 0x3C, 0x66, 0x00, 0x00}, {0x00, 0x0C, 0x0C, 0x3F, 0x0C, 0x0C, 0x00, 0x00}, {0x00, 0x00, 0x00, 0x00, 0x00, 0x0C, 0x0C, 0x06}, {0x00, 0x00, 0x00, 0x3F, 0x00, 0x00, 0x00, 0x00}, {0x00, 0x00, 0x00, 0x00, 0x00, 0x0C, 0x0C, 0x00}, {0x60, 0x30, 0x18, 0x0C, 0x06, 0x03, 0x01, 0x00}, {0x3E, 0x63, 0x73, 0x7B, 0x6F, 0x67, 0x3E, 0x00}, {0x0C, 0x0E, 0x0C, 0x0C, 0x0C, 0x0C, 0x3F, 0x00}, {0x1E, 0x33, 0x30, 0x1C, 0x06, 0x33, 0x3F, 0x00}, {0x1E, 0x33, 0x30, 0x1C, 0x30, 0x33, 0x1E, 0x00}, {0x38, 0x3C, 0x36, 0x33, 0x7F, 0x30, 0x78, 0x00}, {0x3F, 0x03, 0x1F, 0x30, 0x30, 0x33, 0x1E, 0x00}, {0x1C, 0x06, 0x03, 0x1F, 0x33, 0x33, 0x1E, 0x00}, {0x3F, 0x33, 0x30, 0x18, 0x0C, 0x0C, 0x0C, 0x00}, {0x1E, 0x33, 0x33, 0x1E, 0x33, 0x33, 0x1E, 0x00}, {0x1E, 0x33, 0x33, 0x3E, 0x30, 0x18, 0x0E, 0x00}, {0x00, 0x0C, 0x0C, 0x00, 0x00, 0x0C, 0x0C, 0x00}, {0x00, 0x0C, 0x0C, 0x00, 0x00, 0x0C, 0x0C, 0x06}, {0x18, 0x0C, 0x06, 0x03, 0x06, 0x0C, 0x18, 0x00}, {0x00, 0x00, 0x3F, 0x00, 0x00, 0x3F, 0x00, 0x00}, {0x06, 0x0C, 0x18, 0x30, 0x18, 0x0C, 0x06, 0x00}, {0x1E, 0x33, 0x30, 0x18, 0x0C, 0x00, 0x0C, 0x00}, {0x3E, 0x63, 0x7B, 0x7B, 0x7B, 0x03, 0x1E, 0x00}, {0x0C, 0x1E, 0x33, 0x33, 0x3F, 0x33, 0x33, 0x00}, {0x3F, 0x66, 0x66, 0x3E, 0x66, 0x66, 0x3F, 0x00}, {0x3C, 0x66, 0x03, 0x03, 0x03, 0x66, 0x3C, 0x00}, {0x1F, 0x36, 0x66, 0x66, 0x66, 0x36, 0x1F, 0x00}, {0x7F, 0x46, 0x16, 0x1E, 0x16, 0x46, 0x7F, 0x00}, {0x7F, 0x46, 0x16, 0x1E, 0x16, 0x06, 0x0F, 0x00}, {0x3C, 0x66, 0x03, 0x03, 0x73, 0x66, 0x7C, 0x00}, {0x33, 0x33, 0x33, 0x3F, 0x33, 0x33, 0x33, 0x00}, {0x1E, 0x0C, 0x0C, 0x0C, 0x0C, 0x0C, 0x1E, 0x00}, {0x78, 0x30, 0x30, 0x30, 0x33, 0x33, 0x1E, 0x00}, {0x67, 0x66, 0x36, 0x1E, 0x36, 0x66, 0x67, 0x00}, {0x0F, 0x06, 0x06, 0x06, 0x46, 0x66, 0x7F, 0x00}, {0x63, 0x77, 0x7F, 0x7F, 0x6B, 0x63, 0x63, 0x00}, {0x63, 0x67, 0x6F, 0x7B, 0x73, 0x63, 0x63, 0x00}, {0x1C, 0x36, 0x63, 0x63, 0x63, 0x36, 0x1C, 0x00}, {0x3F, 0x66, 0x66, 0x3E, 0x06, 0x06, 0x0F, 0x00}, {0x1E, 0x33, 0x33, 0x33, 0x3B, 0x1E, 0x38, 0x00}, {0x3F, 0x66, 0x66, 0x3E, 0x36, 0x66, 0x67, 0x00}, {0x1E, 0x33, 0x07, 0x0E, 0x38, 0x33, 0x1E, 0x00}, {0x3F, 0x2D, 0x0C, 0x0C, 0x0C, 0x0C, 0x1E, 0x00}, {0x33, 0x33, 0x33, 0x33, 0x33, 0x33, 0x3F, 0x00}, {0x33, 0x33, 0x33, 0x33, 0x33, 0x1E, 0x0C, 0x00}, {0x63, 0x63, 0x63, 0x6B, 0x7F, 0x77, 0x63, 0x00}, {0x63, 0x63, 0x36, 0x1C, 0x1C, 0x36, 0x63, 0x00}, {0x33, 0x33, 0x33, 0x1E, 0x0C, 0x0C, 0x1E, 0x00}, {0x7F, 0x63, 0x31, 0x18, 0x4C, 0x66, 0x7F, 0x00}, {0x1E, 0x06, 0x06, 0x06, 0x06, 0x06, 0x1E, 0x00}, {0x03, 0x06, 0x0C, 0x18, 0x30, 0x60, 0x40, 0x00}, {0x1E, 0x18, 0x18, 0x18, 0x18, 0x18, 0x1E, 0x00}, {0x08, 0x1C, 0x36, 0x63, 0x00, 0x00, 0x00, 0x00}, {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xFF}, {0x0C, 0x0C, 0x18, 0x00, 0x00, 0x00, 0x00, 0x00}, {0x00, 0x00, 0x1E, 0x30, 0x3E, 0x33, 0x6E, 0x00}, {0x07, 0x06, 0x06, 0x3E, 0x66, 0x66, 0x3B, 0x00}, {0x00, 0x00, 0x1E, 0x33, 0x03, 0x33, 0x1E, 0x00}, {0x38, 0x30, 0x30, 0x3E, 0x33, 0x33, 0x6E, 0x00}, {0x00, 0x00, 0x1E, 0x33, 0x3F, 0x03, 0x1E, 0x00}, {0x1C, 0x36, 0x06, 0x0F, 0x06, 0x06, 0x0F, 0x00}, {0x00, 0x00, 0x6E, 0x33, 0x33, 0x3E, 0x30, 0x1F}, {0x07, 0x06, 0x36, 0x6E, 0x66, 0x66, 0x67, 0x00}, {0x0C, 0x00, 0x0E, 0x0C, 0x0C, 0x0C, 0x1E, 0x00}, {0x30, 0x00, 0x30, 0x30, 0x30, 0x33, 0x33, 0x1E}, {0x07, 0x06, 0x66, 0x36, 0x1E, 0x36, 0x67, 0x00}, {0x0E, 0x0C, 0x0C, 0x0C, 0x0C, 0x0C, 0x1E, 0x00}, {0x00, 0x00, 0x33, 0x7F, 0x7F, 0x6B, 0x63, 0x00}, {0x00, 0x00, 0x1F, 0x33, 0x33, 0x33, 0x33, 0x00}, {0x00, 0x00, 0x1E, 0x33, 0x33, 0x33, 0x1E, 0x00}, {0x00, 0x00, 0x3B, 0x66, 0x66, 0x3E, 0x06, 0x0F}, {0x00, 0x00, 0x6E, 0x33, 0x33, 0x3E, 0x30, 0x78}, {0x00, 0x00, 0x3B, 0x6E, 0x66, 0x06, 0x0F, 0x00}, {0x00, 0x00, 0x3E, 0x03, 0x1E, 0x30, 0x1F, 0x00}, {0x08, 0x0C, 0x3E, 0x0C, 0x0C, 0x2C, 0x18, 0x00}, {0x00, 0x00, 0x33, 0x33, 0x33, 0x33, 0x6E, 0x00}, {0x00, 0x00, 0x33, 0x33, 0x33, 0x1E, 0x0C, 0x00}, {0x00, 0x00, 0x63, 0x6B, 0x7F, 0x7F, 0x36, 0x00}, {0x00, 0x00, 0x63, 0x36, 0x1C, 0x36, 0x63, 0x00}, {0x00, 0x00, 0x33, 0x33, 0x33, 0x3E, 0x30, 0x1F}, {0x00, 0x00, 0x3F, 0x19, 0x0C, 0x26, 0x3F, 0x00}, {0x38, 0x0C, 0x0C, 0x07, 0x0C, 0x0C, 0x38, 0x00}, {0x18, 0x18, 0x18, 0x00, 0x18, 0x18, 0x18, 0x00}, {0x07, 0x0C, 0x0C, 0x38, 0x0C, 0x0C, 0x07, 0x00}, {0x6E, 0x3B, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}};

static AP_FColor AP_FontDrawColor(void)
{
  AP_FColor color;
  if (!AP_GetRenderDrawColorFloat(&color.r, &color.g, &color.b, &color.a))
  {
    color.r = 1.0f;
    color.g = 1.0f;
    color.b = 1.0f;
    color.a = 1.0f;
  }
  return color;
}

static unsigned char *AP_FontLoadFile(const char *path, int *size)
{
  FILE *file;
  long length;
  unsigned char *data;

  file = fopen(path, "rb");
  if (file == NULL)
  {
    AP_SET_ERROR(AP_ERROR_NOT_FOUND, "Font file could not be opened");
    return NULL;
  }

  if (fseek(file, 0, SEEK_END) != 0)
  {
    fclose(file);
    AP_SET_ERROR(AP_ERROR_OPERATION_FAILED, "Font file could not be read");
    return NULL;
  }

  length = ftell(file);
  if (length <= 0)
  {
    fclose(file);
    AP_SET_ERROR(AP_ERROR_OPERATION_FAILED, "Font file is empty");
    return NULL;
  }

  rewind(file);
  data = (unsigned char *)malloc((size_t)length);
  if (data == NULL)
  {
    fclose(file);
    AP_SET_ERROR(AP_ERROR_OUT_OF_MEMORY, "Failed to allocate font file");
    return NULL;
  }

  if (fread(data, 1, (size_t)length, file) != (size_t)length)
  {
    free(data);
    fclose(file);
    AP_SET_ERROR(AP_ERROR_OPERATION_FAILED, "Font file could not be read");
    return NULL;
  }

  fclose(file);
  *size = (int)length;
  return data;
}

static void AP_FontDestroy(AP_Font *font)
{
  if (font == NULL)
  {
    return;
  }

  AP_DestroyTexture(font->atlas);
  free(font);
}

static AP_Font *AP_FontBakeBitmap(void)
{
  const int atlas_w = AP_FONT_COLS * AP_FONT_CELL;
  const int atlas_h = 6 * AP_FONT_CELL;
  unsigned char *pixels;
  AP_Font *font;
  int glyph;

  pixels = (unsigned char *)calloc((size_t)atlas_w * (size_t)atlas_h * 4, 1);
  if (pixels == NULL)
  {
    AP_SET_ERROR(AP_ERROR_OUT_OF_MEMORY, "Failed to allocate font atlas");
    return NULL;
  }

  font = (AP_Font *)calloc(1, sizeof(AP_Font));
  if (font == NULL)
  {
    free(pixels);
    AP_SET_ERROR(AP_ERROR_OUT_OF_MEMORY, "Failed to allocate font");
    return NULL;
  }

  for (glyph = 0; glyph < AP_FONT_COUNT; ++glyph)
  {
    int col = glyph % AP_FONT_COLS;
    int row = glyph / AP_FONT_COLS;
    int origin_x = col * AP_FONT_CELL + AP_FONT_PAD;
    int origin_y = row * AP_FONT_CELL + AP_FONT_PAD;
    int y;
    int x;

    for (y = 0; y < AP_FONT_GLYPH; ++y)
    {
      unsigned char bits = AP_FONT_BITMAP[glyph][y];
      for (x = 0; x < AP_FONT_GLYPH; ++x)
      {
        if ((bits & (1u << x)) != 0)
        {
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
  if (font->atlas == NULL)
  {
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
                               float pixel_size)
{
  const int atlas_w = 512;
  const int atlas_h = 512;
  unsigned char *bitmap;
  unsigned char *rgba;
  stbtt_bakedchar packed[AP_FONT_COUNT];
  AP_Font *font;
  int glyph;
  int baked;

  if (pixel_size < 6.0f)
  {
    pixel_size = 6.0f;
  }

  bitmap = (unsigned char *)calloc((size_t)atlas_w * (size_t)atlas_h, 1);
  if (bitmap == NULL)
  {
    AP_SET_ERROR(AP_ERROR_OUT_OF_MEMORY, "Failed to allocate font atlas");
    return NULL;
  }

  baked = stbtt_BakeFontBitmap(data, 0, pixel_size, bitmap, atlas_w, atlas_h,
                               AP_FONT_FIRST, AP_FONT_COUNT, packed);
  if (baked <= 0)
  {
    free(bitmap);
    AP_SET_ERROR(AP_ERROR_OPERATION_FAILED, "Failed to bake TrueType font");
    return NULL;
  }

  rgba = (unsigned char *)malloc((size_t)atlas_w * (size_t)atlas_h * 4);
  if (rgba == NULL)
  {
    free(bitmap);
    AP_SET_ERROR(AP_ERROR_OUT_OF_MEMORY, "Failed to allocate font atlas");
    return NULL;
  }

  for (glyph = 0; glyph < atlas_w * atlas_h; ++glyph)
  {
    unsigned char alpha = bitmap[glyph];
    rgba[glyph * 4 + 0] = 255;
    rgba[glyph * 4 + 1] = 255;
    rgba[glyph * 4 + 2] = 255;
    rgba[glyph * 4 + 3] = alpha;
  }
  free(bitmap);

  font = (AP_Font *)calloc(1, sizeof(AP_Font));
  if (font == NULL)
  {
    free(rgba);
    AP_SET_ERROR(AP_ERROR_OUT_OF_MEMORY, "Failed to allocate font");
    return NULL;
  }

  font->atlas = AP_CreateTextureFromPixels(atlas_w, atlas_h, rgba, atlas_w * 4);
  free(rgba);
  if (font->atlas == NULL)
  {
    free(font);
    return NULL;
  }

  AP_SetTextureScaleMode(font->atlas, AP_SCALEMODE_LINEAR);
  AP_SetTextureBlendMode(font->atlas, AP_BLENDMODE_BLEND);
  font->size = pixel_size;
  font->builtin = false;

  /* stb bakes glyph x_off/y_off relative to the baseline, not the top of the
     line, so we need the font's real ascent to convert our top-left-origin
     "y" into a baseline for AP_FontDrawGlyph. Fall back to pixel_size if the
     font data can't be parsed for some reason (still renders, just less precise). */
  {
    stbtt_fontinfo info;
    if (stbtt_InitFont(&info, data, 0))
    {
      int ascent_units, descent_units, line_gap_units;
      float metric_scale = stbtt_ScaleForPixelHeight(&info, pixel_size);
      stbtt_GetFontVMetrics(&info, &ascent_units, &descent_units, &line_gap_units);
      font->ascent = (float)ascent_units * metric_scale;
      font->descent = -(float)descent_units * metric_scale;
      font->line_height = (float)(ascent_units - descent_units + line_gap_units) * metric_scale;
    }
    else
    {
      font->ascent = pixel_size;
      font->descent = 0.0f;
      font->line_height = pixel_size * 1.25f;
    }
  }

  for (glyph = 0; glyph < AP_FONT_COUNT; ++glyph)
  {
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

static AP_Font *AP_FontActive(AP_Font *font)
{
  if (font != NULL)
  {
    return font;
  }

  if (g_current_font != NULL)
  {
    return g_current_font;
  }

  return AP_GetDefaultFont();
}

static int AP_FontGlyphIndex(int codepoint)
{
  if (codepoint < AP_FONT_FIRST || codepoint > AP_FONT_LAST)
  {
    codepoint = '?';
  }

  return codepoint - AP_FONT_FIRST;
}

static bool AP_FontDrawGlyph(AP_Font *font, float x, float y,
                             const AP_FontGlyph *glyph, AP_FColor color,
                             float scale)
{
  AP_FRect src = glyph->src;
  AP_FRect dst;
  float red;
  float green;
  float blue;
  float alpha;
  bool ok;

  if (glyph->width <= 0.0f || glyph->height <= 0.0f)
  {
    return true;
  }

  dst.x = x + glyph->x_off * scale;
  dst.y = y + glyph->y_off * scale;
  dst.w = glyph->width * scale;
  dst.h = glyph->height * scale;

  if (!AP_GetTextureColorModFloat(font->atlas, &red, &green, &blue) ||
      !AP_GetTextureAlphaModFloat(font->atlas, &alpha))
  {
    return false;
  }

  if (!AP_SetTextureColorModFloat(font->atlas, color.r * red, color.g * green,
                                  color.b * blue) ||
      !AP_SetTextureAlphaModFloat(font->atlas, color.a * alpha))
  {
    return false;
  }

  ok = AP_RenderTexture(font->atlas, &src, &dst);
  AP_SetTextureColorModFloat(font->atlas, red, green, blue);
  AP_SetTextureAlphaModFloat(font->atlas, alpha);
  return ok;
}

bool AP_FontInit(void)
{
  if (g_default_font != NULL)
  {
    return true;
  }

  g_default_font = AP_FontBakeBitmap();
  if (g_default_font == NULL)
  {
    return false;
  }

  if (g_current_font == NULL)
  {
    g_current_font = g_default_font;
  }

  return true;
}

void AP_FontShutdown(void)
{
  g_current_font = NULL;
  AP_FontDestroy(g_default_font);
  g_default_font = NULL;
}

AP_Font *AP_GetDefaultFont(void)
{
  if (!AP_FontInit())
  {
    return NULL;
  }

  return g_default_font;
}

AP_Font *AP_CreateFontFromMemory(const void *data, int data_size,
                                 float pixel_size)
{
  if (data == NULL || data_size <= 0)
  {
    AP_SET_ERROR(AP_ERROR_INVALID_ARGUMENT, "Font data cannot be NULL");
    return NULL;
  }

  return AP_FontBakeTTF((const unsigned char *)data, data_size, pixel_size);
}

AP_Font *AP_LoadFont(const char *path, float pixel_size)
{
  unsigned char *data;
  int size = 0;
  AP_Font *font;

  if (path == NULL)
  {
    AP_SET_ERROR(AP_ERROR_INVALID_ARGUMENT, "Font path cannot be NULL");
    return NULL;
  }

  data = AP_FontLoadFile(path, &size);
  if (data == NULL)
  {
    return NULL;
  }

  font = AP_FontBakeTTF(data, size, pixel_size);
  free(data);
  return font;
}

void AP_DestroyFont(AP_Font *font)
{
  if (font == NULL)
  {
    return;
  }

  if (font == g_default_font)
  {
    AP_SET_ERROR(AP_ERROR_INVALID_ARGUMENT, "Cannot destroy the default font");
    return;
  }

  if (g_current_font == font)
  {
    g_current_font = g_default_font;
  }

  AP_FontDestroy(font);
}

bool AP_FontIsValid(const AP_Font *font)
{
  return font != NULL && AP_TextureIsValid(font->atlas);
}

float AP_GetFontSize(const AP_Font *font)
{
  font = AP_FontActive((AP_Font *)font);
  return font != NULL ? font->size : 0.0f;
}

float AP_GetFontLineHeight(const AP_Font *font)
{
  font = AP_FontActive((AP_Font *)font);
  return font != NULL ? font->line_height : 0.0f;
}

bool AP_SetFont(AP_Font *font)
{
  if (font == NULL)
  {
    g_current_font = AP_GetDefaultFont();
    return g_current_font != NULL;
  }

  if (!AP_FontIsValid(font))
  {
    AP_SET_ERROR(AP_ERROR_INVALID_ARGUMENT, "Font is not valid");
    return false;
  }

  g_current_font = font;
  return true;
}

AP_Font *AP_GetFont(void) { return AP_FontActive(NULL); }

AP_FPoint AP_MeasureTextEx(AP_Font *font, const char *text, float size)
{
  AP_FPoint size_px;
  float scale;
  float line_width = 0.0f;
  float max_width = 0.0f;
  float height;
  const char *cursor;

  size_px.x = 0.0f;
  size_px.y = 0.0f;
  font = AP_FontActive(font);
  if (font == NULL || text == NULL)
  {
    return size_px;
  }

  if (size <= 0.0f)
  {
    size = font->size;
  }

  scale = size / font->size;
  height = font->line_height * scale;

  for (cursor = text; *cursor != '\0'; ++cursor)
  {
    unsigned char ch = (unsigned char)*cursor;
    if (ch == '\n')
    {
      if (line_width > max_width)
      {
        max_width = line_width;
      }
      line_width = 0.0f;
      height += font->line_height * scale;
      continue;
    }

    if (ch == '\t')
    {
      line_width += font->glyphs[0].x_advance * scale * 4.0f;
      continue;
    }

    line_width += font->glyphs[AP_FontGlyphIndex(ch)].x_advance * scale;
  }

  if (line_width > max_width)
  {
    max_width = line_width;
  }

  size_px.x = max_width;
  size_px.y = height;
  return size_px;
}

AP_FPoint AP_MeasureText(const char *text)
{
  return AP_MeasureTextEx(NULL, text, 0.0f);
}

bool AP_RenderTextEx(AP_Font *font, float x, float y, const char *text,
                     AP_FColor color, float size)
{
  float scale;
  float cursor_x;
  float cursor_y;
  const char *cursor;

  font = AP_FontActive(font);
  if (font == NULL || text == NULL)
  {
    AP_SET_ERROR(AP_ERROR_INVALID_ARGUMENT, "Text cannot be drawn");
    return false;
  }

  if (size <= 0.0f)
  {
    size = font->size;
  }

  scale = size / font->size;
  cursor_x = x;
  /* "y" is the top of the line (top-left origin); stb's baked glyph offsets
     are relative to the baseline, so shift down by the font's ascent first. */
  cursor_y = y + (font->builtin ? 0.0f : font->ascent * scale);

  for (cursor = text; *cursor != '\0'; ++cursor)
  {
    unsigned char ch = (unsigned char)*cursor;
    const AP_FontGlyph *glyph;

    if (ch == '\n')
    {
      cursor_x = x;
      cursor_y += font->line_height * scale;
      continue;
    }

    if (ch == '\t')
    {
      cursor_x += font->glyphs[0].x_advance * scale * 4.0f;
      continue;
    }

    glyph = &font->glyphs[AP_FontGlyphIndex(ch)];
    if (!AP_FontDrawGlyph(font, cursor_x, cursor_y, glyph, color, scale))
    {
      return false;
    }

    cursor_x += glyph->x_advance * scale;
  }

  return true;
}

bool AP_RenderText(float x, float y, const char *text)
{
  return AP_RenderTextEx(NULL, x, y, text, AP_FontDrawColor(), 0.0f);
}

bool AP_RenderTextAligned(AP_Font *font, const AP_FRect *bounds,
                          const char *text, AP_FColor color, float size,
                          AP_TextAlign align)
{
  AP_FPoint measured;
  float x;
  float y;

  if (bounds == NULL)
  {
    AP_SET_ERROR(AP_ERROR_INVALID_ARGUMENT, "Text bounds cannot be NULL");
    return false;
  }

  font = AP_FontActive(font);
  measured = AP_MeasureTextEx(font, text, size);
  x = bounds->x;
  y = bounds->y + (bounds->h - measured.y) * 0.5f;

  if (align == AP_TEXT_ALIGN_CENTER)
  {
    x = bounds->x + (bounds->w - measured.x) * 0.5f;
  }
  else if (align == AP_TEXT_ALIGN_RIGHT)
  {
    x = bounds->x + bounds->w - measured.x;
  }

  return AP_RenderTextEx(font, x, y, text, color, size);
}

/* =========================================================
 * System Fonts
 * ========================================================= */

#if AP_PLATFORM_WINDOWS
#include <windows.h>
#else
#include <dirent.h>
#include <sys/stat.h>
#endif

typedef struct AP_SystemFontEntry
{
  char *name;
  char *path;
} AP_SystemFontEntry;

static AP_SystemFontEntry *g_system_fonts = NULL;
static int g_system_font_count = 0;
static int g_system_font_capacity = 0;
static bool g_system_fonts_scanned = false;

static char *AP_FontStrdup(const char *s)
{
  size_t len = strlen(s) + 1;
  char *copy = (char *)malloc(len);
  if (copy != NULL)
  {
    memcpy(copy, s, len);
  }
  return copy;
}

static bool AP_FontHasExtension(const char *path, const char *ext)
{
  size_t path_len = strlen(path);
  size_t ext_len = strlen(ext);
  if (path_len < ext_len)
  {
    return false;
  }
#if AP_PLATFORM_WINDOWS
  return _stricmp(path + (path_len - ext_len), ext) == 0;
#else
  return strcasecmp(path + (path_len - ext_len), ext) == 0;
#endif
}

static bool AP_FontIsFontFile(const char *path)
{
  return AP_FontHasExtension(path, ".ttf") || AP_FontHasExtension(path, ".ttc") ||
         AP_FontHasExtension(path, ".otf");
}

static void AP_SystemFontAdd(const char *name, const char *path)
{
  int i;

  if (name == NULL || path == NULL || name[0] == '\0' || !AP_FontIsFontFile(path))
  {
    return;
  }

  for (i = 0; i < g_system_font_count; ++i)
  {
    if (strcmp(g_system_fonts[i].name, name) == 0)
    {
      return;
    }
  }

  if (g_system_font_count >= g_system_font_capacity)
  {
    int new_capacity = g_system_font_capacity > 0 ? g_system_font_capacity * 2 : 64;
    AP_SystemFontEntry *grown = (AP_SystemFontEntry *)realloc(
        g_system_fonts, (size_t)new_capacity * sizeof(AP_SystemFontEntry));
    if (grown == NULL)
    {
      return;
    }
    g_system_fonts = grown;
    g_system_font_capacity = new_capacity;
  }

  g_system_fonts[g_system_font_count].name = AP_FontStrdup(name);
  g_system_fonts[g_system_font_count].path = AP_FontStrdup(path);
  if (g_system_fonts[g_system_font_count].name != NULL &&
      g_system_fonts[g_system_font_count].path != NULL)
  {
    g_system_font_count++;
  }
}

static void AP_SystemFontsClear(void)
{
  int i;
  for (i = 0; i < g_system_font_count; ++i)
  {
    free(g_system_fonts[i].name);
    free(g_system_fonts[i].path);
  }
  g_system_font_count = 0;
}

#if AP_PLATFORM_WINDOWS

/* Registry values look like "Segoe UI (TrueType)" -> "segoeui.ttf" (relative
   to %WINDIR%\Fonts) or an absolute path for per-user installed fonts. */
static void AP_SystemFontsScanWindows(void)
{
  HKEY key;
  char fonts_dir[MAX_PATH];
  UINT windir_len;

  windir_len = GetWindowsDirectoryA(fonts_dir, MAX_PATH);
  if (windir_len == 0 || windir_len >= MAX_PATH - 16)
  {
    strncpy(fonts_dir, "C:\\Windows", sizeof(fonts_dir) - 1);
    fonts_dir[sizeof(fonts_dir) - 1] = '\0';
  }
  strncat(fonts_dir, "\\Fonts\\", sizeof(fonts_dir) - strlen(fonts_dir) - 1);

  if (RegOpenKeyExA(HKEY_LOCAL_MACHINE,
                    "SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion\\Fonts",
                    0, KEY_READ, &key) == ERROR_SUCCESS)
  {
    DWORD index = 0;
    char value_name[512];
    unsigned char value_data[MAX_PATH];

    for (;;)
    {
      DWORD name_size = (DWORD)sizeof(value_name);
      DWORD data_size = (DWORD)sizeof(value_data);
      DWORD type = 0;
      char display_name[512];
      char full_path[MAX_PATH * 2];
      const char *suffix;
      size_t name_len;

      if (RegEnumValueA(key, index, value_name, &name_size, NULL, &type,
                        value_data, &data_size) != ERROR_SUCCESS)
      {
        break;
      }
      index++;

      if (type != REG_SZ || data_size == 0)
      {
        continue;
      }

      strncpy(display_name, value_name, sizeof(display_name) - 1);
      display_name[sizeof(display_name) - 1] = '\0';

      /* Strip trailing " (TrueType)" / " (OpenType)" style suffixes */
      suffix = strrchr(display_name, '(');
      if (suffix != NULL && suffix != display_name)
      {
        name_len = (size_t)(suffix - display_name);
        while (name_len > 0 && display_name[name_len - 1] == ' ')
        {
          name_len--;
        }
        display_name[name_len] = '\0';
      }

      if (strchr((const char *)value_data, '\\') != NULL ||
          strchr((const char *)value_data, ':') != NULL)
      {
        strncpy(full_path, (const char *)value_data, sizeof(full_path) - 1);
        full_path[sizeof(full_path) - 1] = '\0';
      }
      else
      {
        snprintf(full_path, sizeof(full_path), "%s%s", fonts_dir,
                 (const char *)value_data);
      }

      AP_SystemFontAdd(display_name, full_path);
    }

    RegCloseKey(key);
  }
}

#else

static void AP_SystemFontsScanDir(const char *dir_path, int depth)
{
  DIR *dir;
  struct dirent *entry;

  if (depth > 2)
  {
    return;
  }

  dir = opendir(dir_path);
  if (dir == NULL)
  {
    return;
  }

  while ((entry = readdir(dir)) != NULL)
  {
    char full_path[1024];
    struct stat st;

    if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
    {
      continue;
    }

    snprintf(full_path, sizeof(full_path), "%s/%s", dir_path, entry->d_name);

    if (stat(full_path, &st) != 0)
    {
      continue;
    }

    if (S_ISDIR(st.st_mode))
    {
      AP_SystemFontsScanDir(full_path, depth + 1);
    }
    else if (AP_FontIsFontFile(full_path))
    {
      char name[256];
      const char *base = strrchr(entry->d_name, '/');
      const char *dot;
      size_t len;

      base = base != NULL ? base + 1 : entry->d_name;
      dot = strrchr(base, '.');
      len = dot != NULL ? (size_t)(dot - base) : strlen(base);
      if (len >= sizeof(name))
      {
        len = sizeof(name) - 1;
      }
      memcpy(name, base, len);
      name[len] = '\0';

      AP_SystemFontAdd(name, full_path);
    }
  }

  closedir(dir);
}

static void AP_SystemFontsScanPosix(void)
{
  const char *home = getenv("HOME");
  char user_path[1024];

#if AP_PLATFORM_MACOS
  AP_SystemFontsScanDir("/System/Library/Fonts", 0);
  AP_SystemFontsScanDir("/Library/Fonts", 0);
  if (home != NULL)
  {
    snprintf(user_path, sizeof(user_path), "%s/Library/Fonts", home);
    AP_SystemFontsScanDir(user_path, 0);
  }
#else
  AP_SystemFontsScanDir("/usr/share/fonts", 0);
  AP_SystemFontsScanDir("/usr/local/share/fonts", 0);
  if (home != NULL)
  {
    snprintf(user_path, sizeof(user_path), "%s/.fonts", home);
    AP_SystemFontsScanDir(user_path, 0);
    snprintf(user_path, sizeof(user_path), "%s/.local/share/fonts", home);
    AP_SystemFontsScanDir(user_path, 0);
  }
#endif
}

#endif /* AP_PLATFORM_WINDOWS */

void AP_RefreshSystemFonts(void)
{
  AP_SystemFontsClear();
  g_system_fonts_scanned = true;

#if AP_PLATFORM_WINDOWS
  AP_SystemFontsScanWindows();
#else
  AP_SystemFontsScanPosix();
#endif
}

static void AP_SystemFontsEnsureScanned(void)
{
  if (!g_system_fonts_scanned)
  {
    AP_RefreshSystemFonts();
  }
}

int AP_GetSystemFontCount(void)
{
  AP_SystemFontsEnsureScanned();
  return g_system_font_count;
}

const char *AP_GetSystemFontName(int index)
{
  AP_SystemFontsEnsureScanned();
  if (index < 0 || index >= g_system_font_count)
  {
    return NULL;
  }
  return g_system_fonts[index].name;
}

const char *AP_GetSystemFontPath(int index)
{
  AP_SystemFontsEnsureScanned();
  if (index < 0 || index >= g_system_font_count)
  {
    return NULL;
  }
  return g_system_fonts[index].path;
}

AP_Font *AP_LoadSystemFont(const char *name, float pixel_size)
{
  int i;
  int best = -1;

  if (name == NULL)
  {
    AP_SET_ERROR(AP_ERROR_INVALID_ARGUMENT, "Font name cannot be NULL");
    return NULL;
  }

  AP_SystemFontsEnsureScanned();

  /* Exact (case-insensitive) match first */
  for (i = 0; i < g_system_font_count; ++i)
  {
#if AP_PLATFORM_WINDOWS
    if (_stricmp(g_system_fonts[i].name, name) == 0)
    {
#else
    if (strcasecmp(g_system_fonts[i].name, name) == 0)
    {
#endif
      best = i;
      break;
    }
  }

  /* Fall back to a substring match (e.g. "Segoe" matches "Segoe UI") */
  if (best < 0)
  {
    for (i = 0; i < g_system_font_count; ++i)
    {
      if (strstr(g_system_fonts[i].name, name) != NULL)
      {
        best = i;
        break;
      }
    }
  }

  if (best < 0)
  {
    AP_SET_ERROR(AP_ERROR_NOT_FOUND, "System font not found");
    return NULL;
  }

  return AP_LoadFont(g_system_fonts[best].path, pixel_size);
}

AP_Font *AP_LoadSystemDefaultFont(float pixel_size)
{
  static const char *candidates[] = {
#if AP_PLATFORM_WINDOWS
      "Segoe UI", "Tahoma", "Arial", "Verdana"
#elif AP_PLATFORM_MACOS
      "Helvetica Neue", "Helvetica", "Arial"
#else
      "Noto Sans", "DejaVu Sans", "Liberation Sans", "Ubuntu", "Arial"
#endif
  };
  size_t i;
  AP_Font *font;

  AP_SystemFontsEnsureScanned();

  for (i = 0; i < sizeof(candidates) / sizeof(candidates[0]); ++i)
  {
    font = AP_LoadSystemFont(candidates[i], pixel_size);
    if (font != NULL)
    {
      return font;
    }
  }

  /* No system fonts found (or none matched) — first available one, if any */
  if (g_system_font_count > 0)
  {
    font = AP_LoadFont(g_system_fonts[0].path, pixel_size);
    if (font != NULL)
    {
      return font;
    }
  }

  return AP_GetDefaultFont();
}
