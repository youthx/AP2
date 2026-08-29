/*
 * AP2 — Application Primitives
 * Copyright (c) 2026 Jack Waechter
 *
 * Licensed under the MIT License.
 * See LICENSE in the project root for full terms.
 */

#ifndef AP2_TEXTURE_H
#define AP2_TEXTURE_H

#include "AP2/AP2_Renderer.h"
#include "AP2/AP2_Types.h"

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * AP2 Texture
 *
 * GPU image used by AP_DrawTexture() and AP_Sprite. Matches
 * SDL3's SDL_Texture calling convention, minus the renderer
 * pointer — textures belong to the active window.
 *
 *     AP_Texture *tex = AP_LoadTexture("player.png");
 *     AP_DrawTexture(tex, NULL, &(AP_FRect){64.0f, 64.0f, 128.0f, 128.0f});
 *     AP_DestroyTexture(tex);
 *
 * Pixel data is 8-bit RGBA, tightly packed (pitch = width * 4)
 * unless a pitch is supplied to AP_UpdateTexture().
 */

typedef struct AP_Texture AP_Texture;

typedef enum AP_ScaleMode {
  AP_SCALEMODE_NEAREST = 0,
  AP_SCALEMODE_LINEAR,
  AP_SCALEMODE_MIPMAP
} AP_ScaleMode;

typedef enum AP_FlipMode {
  AP_FLIP_NONE = 0,
  AP_FLIP_HORIZONTAL = 1 << 0,
  AP_FLIP_VERTICAL = 1 << 1
} AP_FlipMode;

typedef enum AP_TextureAccess {
  AP_TEXTUREACCESS_STATIC = 0,
  AP_TEXTUREACCESS_STREAMING,
  AP_TEXTUREACCESS_TARGET
} AP_TextureAccess;

typedef enum AP_TextureAddressMode {
  AP_TEXTUREADDRESS_CLAMP = 0,
  AP_TEXTUREADDRESS_WRAP,
  AP_TEXTUREADDRESS_MIRROR
} AP_TextureAddressMode;

/* =========================================================
 * Creation
 * ========================================================= */

AP_Texture *AP_CreateTexture(int width, int height);

AP_Texture *AP_CreateTextureWithAccess(int width, int height,
                                       AP_TextureAccess access);

AP_Texture *AP_CreateTextureFromPixels(int width, int height,
                                       const void *pixels, int pitch);

AP_Texture *AP_LoadTexture(const char *path);

AP_Texture *AP_LoadTextureFromMemory(const void *data, int size);

AP_Texture *AP_CopyTexture(AP_Texture *texture);

void AP_DestroyTexture(AP_Texture *texture);

bool AP_TextureIsValid(const AP_Texture *texture);

AP_TextureAccess AP_GetTextureAccess(const AP_Texture *texture);

/* =========================================================
 * Size / pixels
 * ========================================================= */

bool AP_GetTextureSize(AP_Texture *texture, int *w, int *h);

int AP_GetTextureWidth(AP_Texture *texture);

int AP_GetTextureHeight(AP_Texture *texture);

bool AP_UpdateTexture(AP_Texture *texture, const AP_Rect *rect,
                      const void *pixels, int pitch);

bool AP_LockTexture(AP_Texture *texture, const AP_Rect *rect, void **pixels,
                    int *pitch);

bool AP_UnlockTexture(AP_Texture *texture);

bool AP_ReadTexturePixels(AP_Texture *texture, const AP_Rect *rect,
                          void *pixels, int pitch);

bool AP_GenerateTextureMipmaps(AP_Texture *texture);

/* =========================================================
 * Sampler state
 * ========================================================= */

bool AP_SetTextureColorMod(AP_Texture *texture, AP_U8 r, AP_U8 g, AP_U8 b);

bool AP_SetTextureColorModFloat(AP_Texture *texture, float r, float g, float b);

bool AP_GetTextureColorMod(AP_Texture *texture, AP_U8 *r, AP_U8 *g, AP_U8 *b);

bool AP_GetTextureColorModFloat(AP_Texture *texture, float *r, float *g,
                                float *b);

bool AP_SetTextureAlphaMod(AP_Texture *texture, AP_U8 alpha);

bool AP_SetTextureAlphaModFloat(AP_Texture *texture, float alpha);

bool AP_GetTextureAlphaMod(AP_Texture *texture, AP_U8 *alpha);

bool AP_GetTextureAlphaModFloat(AP_Texture *texture, float *alpha);

bool AP_SetTextureBlendMode(AP_Texture *texture, AP_BlendMode mode);

AP_BlendMode AP_GetTextureBlendMode(AP_Texture *texture);

bool AP_SetTextureScaleMode(AP_Texture *texture, AP_ScaleMode mode);

AP_ScaleMode AP_GetTextureScaleMode(AP_Texture *texture);

bool AP_SetTextureAddressMode(AP_Texture *texture, AP_TextureAddressMode mode);

AP_TextureAddressMode AP_GetTextureAddressMode(AP_Texture *texture);

/* =========================================================
 * Render target  (SDL_SetRenderTarget)
 *
 * NULL restores the window framebuffer.
 * ========================================================= */

bool AP_SetRenderTarget(AP_Texture *texture);

AP_Texture *AP_GetRenderTarget(void);

/* =========================================================
 * Draw
 *
 * NULL src uses the full texture.
 * NULL dst fills the current viewport.
 * Angle is degrees, clockwise in window space (Y-down).
 * NULL center rotates about the destination rectangle center.
 * ========================================================= */

bool AP_RenderTexture(AP_Texture *texture, const AP_FRect *src,
                      const AP_FRect *dst);

bool AP_RenderTextureRotated(AP_Texture *texture, const AP_FRect *src,
                             const AP_FRect *dst, float angle,
                             const AP_FPoint *center, AP_FlipMode flip);

bool AP_RenderTextureTiled(AP_Texture *texture, const AP_FRect *src,
                           float scale, const AP_FRect *dst);

bool AP_RenderTexture9Grid(AP_Texture *texture, const AP_FRect *src, float left,
                           float right, float top, float bottom, float scale,
                           const AP_FRect *dst);

bool AP_RenderTextureAffine(AP_Texture *texture, const AP_FRect *src,
                            const AP_FPoint *origin, const AP_FPoint *right,
                            const AP_FPoint *down);

bool AP_RenderTextureQuad(AP_Texture *texture, const AP_FRect *src,
                          const AP_FPoint corners[4]);

bool AP_RenderTextureGeometry(AP_Texture *texture, const AP_Vertex *vertices,
                              int num_vertices, const int *indices,
                              int num_indices);

#define AP_DrawTexture AP_RenderTexture
#define AP_DrawTextureRotated AP_RenderTextureRotated
#define AP_DrawTextureTiled AP_RenderTextureTiled
#define AP_DrawTexture9Grid AP_RenderTexture9Grid
#define AP_DrawTextureAffine AP_RenderTextureAffine
#define AP_DrawTextureQuad AP_RenderTextureQuad
#define AP_DrawTextureGeometry AP_RenderTextureGeometry

#ifdef __cplusplus
}
#endif

#endif /* AP2_TEXTURE_H */
