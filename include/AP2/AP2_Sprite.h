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
 * flip, and tint. (x, y) passed to AP_RenderSprite() is where
 * the origin is placed in window pixels.
 *
 *     AP_Sprite sprite = AP_CreateSprite(texture);
 *     AP_SpriteSetOriginNormalized(&sprite, 0.5f, 0.5f);
 *     AP_RenderSprite(&sprite, 640.0f, 360.0f);
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

bool AP_RenderSprite(const AP_Sprite *sprite, float x, float y);

bool AP_RenderSpriteEx(const AP_Sprite *sprite, const AP_FRect *dst);

#ifdef __cplusplus
}
#endif

#endif /* AP2_SPRITE_H */
