/*
 * AP2 — Application Primitives
 * Copyright (c) 2024-2026 Jack Waechter
 *
 * Licensed under the MIT License.
 * See LICENSE in the project root for full terms.
 */

#ifndef AP2_IMAGE_H
#define AP2_IMAGE_H

#include "AP2/AP2_Renderer.h"
#include "AP2/AP2_Types.h"

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * AP2 Image
 *
 * CPU-side RGBA8 bitmap. Pixel data is tightly packed
 * (pitch = width * 4). Use this to load, edit, and save
 * images, then upload with AP_CreateTextureFromImage().
 *
 *     AP_Image *img = AP_LoadImage("player.png");
 *     AP_FlipImage(img, true, false);
 *     AP_ImageGrayscale(img);
 *
 *     AP_Texture *tex = AP_CreateTextureFromImage(img);
 *     AP_DestroyImage(img);
 *
 *     AP_DrawTexture(tex, NULL, &(AP_FRect){64.0f, 64.0f, 128.0f, 128.0f});
 *
 * Load/save use stb_image / stb_image_write (PNG, JPEG, BMP, …).
 * Exclude with AP2_NO_IMAGE.
 */

typedef struct AP_Image AP_Image;

typedef enum AP_ImageFilter {
  AP_IMAGE_NEAREST = 0,
  AP_IMAGE_LINEAR
} AP_ImageFilter;

/* =========================================================
 * Creation
 * ========================================================= */

AP_Image *AP_CreateImage(int width, int height);

AP_Image *AP_CreateImageFromPixels(int width, int height, const void *pixels,
                                   int pitch);

AP_Image *AP_LoadImage(const char *path);

AP_Image *AP_LoadImageFromMemory(const void *data, int size);

AP_Image *AP_CopyImage(const AP_Image *image);

void AP_DestroyImage(AP_Image *image);

bool AP_ImageIsValid(const AP_Image *image);

/* =========================================================
 * Size / pixels
 * ========================================================= */

bool AP_GetImageSize(const AP_Image *image, int *w, int *h);

int AP_GetImageWidth(const AP_Image *image);

int AP_GetImageHeight(const AP_Image *image);

int AP_GetImagePitch(const AP_Image *image);

const void *AP_GetImagePixels(const AP_Image *image);

void *AP_GetImagePixelsMutable(AP_Image *image);

bool AP_GetImagePixel(const AP_Image *image, int x, int y, AP_Color8 *color);

bool AP_SetImagePixel(AP_Image *image, int x, int y, AP_Color8 color);

bool AP_GetImagePixelFloat(const AP_Image *image, int x, int y,
                           AP_Color *color);

bool AP_SetImagePixelFloat(AP_Image *image, int x, int y, AP_Color color);

/* =========================================================
 * Fill / blit
 *
 * NULL src_rect uses the full source image.
 * NULL fill/blit dest rect fills the whole destination.
 * Overlapping blit of an image onto itself is safe.
 * ========================================================= */

bool AP_FillImage(AP_Image *image, AP_Color8 color);

bool AP_FillImageRect(AP_Image *image, const AP_Rect *rect, AP_Color8 color);

bool AP_BlitImage(const AP_Image *src, const AP_Rect *src_rect, AP_Image *dst,
                  int dst_x, int dst_y);

bool AP_BlitImageBlend(const AP_Image *src, const AP_Rect *src_rect,
                       AP_Image *dst, int dst_x, int dst_y, AP_BlendMode blend);

bool AP_BlitImageScaled(const AP_Image *src, const AP_Rect *src_rect,
                        AP_Image *dst, const AP_Rect *dst_rect,
                        AP_ImageFilter filter);

/* =========================================================
 * Transform
 *
 * Flip is in-place. Rotate/scale/crop allocate a new image.
 * Rotation is clockwise degrees in 90-degree steps (90, 180, 270).
 * ========================================================= */

bool AP_FlipImage(AP_Image *image, bool horizontal, bool vertical);

AP_Image *AP_RotateImage(const AP_Image *image, int degrees);

AP_Image *AP_ScaleImage(const AP_Image *image, int width, int height,
                        AP_ImageFilter filter);

AP_Image *AP_CropImage(const AP_Image *image, const AP_Rect *rect);

/* =========================================================
 * Color
 * ========================================================= */

bool AP_ModulateImage(AP_Image *image, AP_Color8 color);

bool AP_ImageGrayscale(AP_Image *image);

bool AP_ImageInvert(AP_Image *image);

bool AP_ImagePremultiply(AP_Image *image);

bool AP_ReplaceImageColor(AP_Image *image, AP_Color8 from, AP_Color8 to);

bool AP_MapImageColorKey(AP_Image *image, AP_Color8 key);

/* =========================================================
 * Save
 *
 * AP_SaveImage picks PNG / BMP / JPEG from the path extension.
 * JPEG quality is 1–100 (default 90).
 * ========================================================= */

bool AP_SaveImage(const AP_Image *image, const char *path);

bool AP_SaveImagePNG(const AP_Image *image, const char *path);

bool AP_SaveImageBMP(const AP_Image *image, const char *path);

bool AP_SaveImageJPG(const AP_Image *image, const char *path, int quality);

#ifdef __cplusplus
}
#endif

#endif /* AP2_IMAGE_H */
