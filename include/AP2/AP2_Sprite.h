/*
 * AP2 — Application Primitives
 * Copyright (c) 2024-2026 Jack Waechter
 *
 * Licensed under the MIT License.
 * See LICENSE in the project root for full terms.
 */

#ifndef AP2_SPRITE_H
#define AP2_SPRITE_H

#include "AP2/AP2_Texture.h"
#include "AP2/AP2_Types.h"

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * AP2 Sprite
 *
 * A drawable slice of a texture with origin, scale, rotation,
 * flip, and tint. (x, y) passed to AP_DrawSprite() is where
 * the origin is placed in window pixels.
 *
 *     AP_Sprite sprite = AP_CreateSprite(texture);
 *     AP_SpriteSetOriginNormalized(&sprite, 0.5f, 0.5f);
 *     AP_DrawSprite(&sprite, 640.0f, 360.0f);
 *
 * Atlas frames:
 *
 *     AP_SpriteSetFrame(&sprite, 8, 4, frame_index);
 */

typedef struct AP_Sprite {
  AP_Texture *texture;
  AP_FRect src;
  AP_FPoint origin;
  float rotation;
  float scale_x;
  float scale_y;
  AP_FlipMode flip;
  AP_FColor color;
  int anim_columns;
  int anim_rows;
  int anim_start;
  int anim_count;
  int anim_index;
  float anim_fps;
  float anim_elapsed;
  bool anim_loop;
  bool anim_playing;
} AP_Sprite;

AP_Sprite AP_CreateSprite(AP_Texture *texture);

bool AP_SpriteSetTexture(AP_Sprite *sprite, AP_Texture *texture);

bool AP_SpriteSetSource(AP_Sprite *sprite, const AP_FRect *src);

bool AP_SpriteSetOrigin(AP_Sprite *sprite, float x, float y);

bool AP_SpriteSetOriginNormalized(AP_Sprite *sprite, float x, float y);

bool AP_SpriteSetRotation(AP_Sprite *sprite, float degrees);

bool AP_SpriteSetScale(AP_Sprite *sprite, float scale);

bool AP_SpriteSetScaleXY(AP_Sprite *sprite, float scale_x, float scale_y);

bool AP_SpriteSetFlip(AP_Sprite *sprite, AP_FlipMode flip);

bool AP_SpriteSetColor(AP_Sprite *sprite, AP_FColor color);

bool AP_SpriteSetFrame(AP_Sprite *sprite, int columns, int rows, int frame);

bool AP_SpritePlay(AP_Sprite *sprite, int columns, int rows, int start_frame,
                   int frame_count, float fps, bool loop);

bool AP_SpriteStop(AP_Sprite *sprite);

bool AP_SpriteUpdate(AP_Sprite *sprite, float delta_seconds);

int AP_SpriteGetFrame(const AP_Sprite *sprite);

bool AP_RenderSprite(const AP_Sprite *sprite, float x, float y);

bool AP_RenderSpriteEx(const AP_Sprite *sprite, const AP_FRect *dst);

#define AP_DrawSprite AP_RenderSprite
#define AP_DrawSpriteEx AP_RenderSpriteEx

#ifdef __cplusplus
}
#endif

#endif /* AP2_SPRITE_H */
